"""AlphaZero training loop for the swamp-knight puzzle.

Self-play and tree search run in the C++ binary (build/az), one process per worker.
The network trains here, in NumPy on the CPU (default) or in PyTorch on a GPU (--backend torch).

Run from the repository root, for example:
    python3 train/az_train.py --run runs/a1_tau --start a1 --credit 0.5 --tau1 2 --tau2 1 --workers 16
    python3 train/az_train.py --run runs/a1_tau --resume --rounds 200
Every round writes <run>/w_cur.bin, state.json, buffer.npy, log.txt, log.csv and best_overall.txt
(best finished tour: line 1 = minutes, line 2 = the tour).
"""
import argparse, json, os, re, shutil, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

D_IN, H, NA = 64 * 21 + 64 + 10, 256, 24
TSCALE = 10000.0
SAMPLE = np.dtype([('alt', '<i2', 64), ('vis', 'u1', 64), ('pos', 'u1'), ('T', '<i4'),
                   ('mask', '<u4'), ('pi', '<f4', NA), ('z', '<f4')])
assert SAMPLE.itemsize == 301  # must match the packed Sample struct in src/az.cpp


def featurize(alt, vis, pos, T):
    """Network input; must match featurize() in src/az.cpp exactly."""
    B = alt.shape[0]
    alt = alt.astype(np.int32)
    A = alt[np.arange(B), pos]
    r = alt - A[:, None]
    rc = np.clip(r, -8, 8)
    X = np.zeros((B, D_IN), np.float32)
    rows = np.repeat(np.arange(B), 64)
    sq = np.tile(np.arange(64), B)
    X[rows, sq * 21 + (rc.ravel() + 8)] = 1
    X[rows, sq * 21 + 17 + vis.ravel().astype(np.int64)] = 1
    X[np.arange(B), 64 * 21 + pos] = 1
    opp = 63 - pos
    eq = (r == 0)
    n = eq.sum(1) - eq[np.arange(B), opp]
    rem = (3 - vis[:, :63].astype(np.int32)).sum(1)
    cl = lambda v, m: np.clip(v, -m, m) / m
    G = 64 * 21 + 64
    X[:, G + 0] = n / 64
    X[:, G + 1] = cl(alt[:, 63] - A, 16)
    X[:, G + 2] = T / TSCALE
    X[:, G + 3] = eq.sum(1) / 64
    X[:, G + 4] = (r == -1).sum(1) / 64
    X[:, G + 5] = (r == -2).sum(1) / 64
    X[:, G + 6] = (r == 1).sum(1) / 64
    X[:, G + 7] = (r == 2).sum(1) / 64
    X[:, G + 8] = rem / 189
    X[:, G + 9] = cl(alt[np.arange(B), opp] - A, 16)
    return X


class Net:
    """MLP: 1418 inputs -> `layers` hidden ReLU layers of width `hidden` -> 24 policy logits + 1 value. Adam, in NumPy.
    The weight file layout matches src/az.cpp: W1, b1, (W, b) per extra hidden layer, Wp, bp, wv, bv."""

    def __init__(self, rng, lr=1e-3, wd=1e-5, hidden=256, layers=2):
        self.lr, self.wd, self.H, self.L = lr, wd, hidden, layers
        H = hidden
        self.p = {'W1': (rng.standard_normal((D_IN, H)) * np.sqrt(2 / 139)).astype(np.float32),
                  'b1': np.zeros(H, np.float32)}
        for l in range(2, layers + 1):
            self.p[f'W{l}'] = (rng.standard_normal((H, H)) * np.sqrt(2 / H)).astype(np.float32)
            self.p[f'b{l}'] = np.zeros(H, np.float32)
        self.p['Wp'] = (rng.standard_normal((H, NA)) * 0.01).astype(np.float32)
        self.p['bp'] = np.zeros(NA, np.float32)
        self.p['wv'] = (rng.standard_normal(H) * 0.01).astype(np.float32)
        self.p['bv'] = np.zeros(1, np.float32)
        self.ORDER = ['W1', 'b1'] + [k for l in range(2, layers + 1) for k in (f'W{l}', f'b{l}')] + ['Wp', 'bp', 'wv', 'bv']
        self.m = {k: np.zeros_like(v) for k, v in self.p.items()}
        self.v = {k: np.zeros_like(v) for k, v in self.p.items()}
        self.t = 0

    def save(self, fn):
        tmp = fn + '.tmp'
        with open(tmp, 'wb') as f:
            for k in self.ORDER:
                f.write(self.p[k].astype(np.float32).tobytes())
        os.replace(tmp, fn)

    def load(self, fn):
        a = np.fromfile(fn, np.float32)
        need = sum(self.p[k].size for k in self.ORDER)
        if a.size != need:
            sys.exit(f'{fn} has {a.size} weights, but a {self.L}x{self.H} network needs {need}: check --hidden/--layers')
        o = 0
        for k in self.ORDER:
            n = self.p[k].size; self.p[k] = a[o:o + n].reshape(self.p[k].shape).copy(); o += n

    def infer(self, idx, g):
        """Batched evaluation for self-play: idx (n, 129) active one-hot inputs, g (n, 10) summary features.
        Returns (n, 25): 24 move logits, then the value."""
        P = self.p
        h = np.maximum(P['W1'][idx].sum(1) + g @ P['W1'][64 * 21 + 64:] + P['b1'], 0)
        for l in range(2, self.L + 1):
            h = np.maximum(h @ P[f'W{l}'] + P[f'b{l}'], 0)
        return np.concatenate([h @ P['Wp'] + P['bp'], (h @ P['wv'] + P['bv'][0])[:, None]], 1)

    def step(self, X, mask, pi, z, cv=1.0):
        P = self.p; B = X.shape[0]
        acts, pre = [X], []
        for l in range(1, self.L + 1):
            a_ = acts[-1] @ P[f'W{l}'] + P[f'b{l}']; pre.append(a_); acts.append(np.maximum(a_, 0))
        h = acts[-1]
        lg = h @ P['Wp'] + P['bp']
        lg = np.where(mask, lg, -1e9)
        lg = lg - lg.max(1, keepdims=True)
        e = np.exp(lg) * mask; sm = e / e.sum(1, keepdims=True)
        v = h @ P['wv'] + P['bv'][0]
        lp = -(pi * np.log(sm + 1e-12)).sum(1).mean()
        lv = ((v - z) ** 2).mean()
        dlg = (sm - pi) / B
        dv = cv * 2 * (v - z) / B
        g = {'Wp': h.T @ dlg, 'bp': dlg.sum(0), 'wv': h.T @ dv, 'bv': np.array([dv.sum()], np.float32)}
        dh = dlg @ P['Wp'].T + np.outer(dv, P['wv'])
        for l in range(self.L, 0, -1):
            da = dh * (pre[l - 1] > 0)
            g[f'W{l}'] = acts[l - 1].T @ da; g[f'b{l}'] = da.sum(0)
            if l > 1:
                dh = da @ P[f'W{l}'].T
        self.t += 1; b1, b2, eps = 0.9, 0.999, 1e-8
        for k in P:
            gk = g[k].astype(np.float32) + (self.wd * P[k] if k.startswith('W') else 0)
            self.m[k] = b1 * self.m[k] + (1 - b1) * gk
            self.v[k] = b2 * self.v[k] + (1 - b2) * gk * gk
            mh = self.m[k] / (1 - b1 ** self.t); vh = self.v[k] / (1 - b2 ** self.t)
            P[k] -= (self.lr * mh / (np.sqrt(vh) + eps)).astype(np.float32)
        return lp, lv


# ---- self-imitation of the best tour (optional) ----
ADX, ADY, ADZ = [], [], []
for d in [(1, 2), (2, 1), (2, -1), (1, -2), (-1, -2), (-2, -1), (-2, 1), (-1, 2)]: ADX.append(d[0]); ADY.append(d[1]); ADZ.append(0)
for d in [(1, 0), (0, 1), (-1, 0), (0, -1)]:
    for zz in (2, -2): ADX.append(d[0]); ADY.append(d[1]); ADZ.append(zz)
for d in [(1, 0), (0, 1), (-1, 0), (0, -1)]:
    for zz in (1, -1): ADX.append(2 * d[0]); ADY.append(2 * d[1]); ADZ.append(zz)
ROWS = [[9, 8, 10, 12, 11, 8, 10, 17], [7, 9, 11, 9, 10, 12, 14, 12], [4, 7, 5, 8, 8, 6, 13, 10], [4, 10, 7, 9, 6, 8, 7, 9],
        [2, 6, 4, 2, 5, 9, 8, 11], [0, 3, 1, 4, 2, 7, 10, 7], [1, 2, 0, 1, 2, 5, 7, 6], [0, 2, 4, 3, 5, 6, 2, 4]]
ALT0 = [ROWS[7 - i // 8][i % 8] for i in range(64)]


def act_legal(alt, vis, p, a):
    x, y = p % 8 + ADX[a], p // 8 + ADY[a]
    if not (0 <= x < 8 and 0 <= y < 8): return -1, -1
    q = x + 8 * y
    if q != 63 and vis[q] >= 3: return -1, q
    if alt[q] == alt[p] and q != 63 - p: return (0 if ADZ[a] == 0 else -1), q
    d = ADZ[a] - (alt[q] - alt[p]); return (d if d >= 0 else -1), q


def apply(alt, vis, p, delta, q):
    waited = 0
    if delta > 0:
        A = alt[p]; O = 63 - p; rise = alt[O] != A; n = 0
        for i in range(64):
            if alt[i] == A and i != O: alt[i] -= delta; n += 1
        if rise: alt[O] += delta
        waited = n * delta
    vis[q] += 1; return waited


def tour_samples(fn, open_len):
    """Training samples (one-hot policy, true remaining minutes) along the tour in fn."""
    lines = open(fn).read().split('\n'); mv = re.findall(r'\((\d+),\s*([a-h][1-8])\)', lines[1])
    alt = ALT0[:]; vis = [0] * 64; vis[0] = 1; p = 0; T = 0; states = []
    for k, (t, sq) in enumerate(mv):
        q = (ord(sq[0]) - 97) + 8 * (int(sq[1]) - 1)
        if k >= open_len:
            mask = 0; act = -1
            n = sum(1 for i in range(64) if alt[i] == alt[p] and i != 63 - p)
            for a in range(NA):
                dl, qq = act_legal(alt, vis, p, a)
                if dl >= 0:
                    mask |= 1 << a
                    if qq == q and dl * n == int(t): act = a
            states.append((alt[:], vis[:], p, T, mask, act))
        A = alt[p]; n = sum(1 for i in range(64) if alt[i] == A and i != 63 - p)
        T += apply(alt, vis, p, int(t) // max(1, n), q); p = q
    out = np.zeros(len(states), SAMPLE)
    for i, (a_, v_, p_, T_, m_, act) in enumerate(states):
        out[i]['alt'] = a_; out[i]['vis'] = v_; out[i]['pos'] = p_; out[i]['T'] = T_; out[i]['mask'] = m_
        pi = np.zeros(NA, np.float32)
        if act >= 0: pi[act] = 1
        out[i]['pi'] = pi; out[i]['z'] = (T - T_) / TSCALE
    return out


def load_samples(fn):
    if not os.path.exists(fn) or os.path.getsize(fn) == 0:
        return np.zeros(0, SAMPLE)
    return np.fromfile(fn, SAMPLE)


EVAL_REQ = np.dtype([('idx', '<i2', 64 * 2 + 1), ('g', '<f4', 10)])  # matches EvalReq in src/az.cpp


def _read_exact(fd, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = os.read(fd, n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return bytes(buf)


def serve_inference(net, fifos):
    """Answer the self-play workers' batched evaluation requests until every worker is done.
    Each pass reads one request from every live worker, evaluates them together, and replies."""
    fds = []
    for rq, rs in fifos:  # same order as the workers open them: request first, then response
        r = os.open(rq, os.O_RDONLY); w = os.open(rs, os.O_WRONLY); fds.append((r, w))
    alive, total = list(range(len(fds))), 0
    while alive:
        batch = []
        for w in list(alive):
            hdr = _read_exact(fds[w][0], 4)
            if hdr is None:
                alive.remove(w); continue
            n = int.from_bytes(hdr, 'little')
            batch.append((w, np.frombuffer(_read_exact(fds[w][0], n * EVAL_REQ.itemsize), EVAL_REQ)))
        if not batch:
            break
        allr = np.concatenate([b for _, b in batch])
        out = np.ascontiguousarray(net.infer(allr['idx'].astype(np.int64), np.ascontiguousarray(allr['g'])), dtype=np.float32)
        total += len(allr); o = 0
        for w, b in batch:
            data = out[o:o + len(b)].tobytes(); o += len(b)
            while data:
                k = os.write(fds[w][1], data); data = data[k:]
    for r, w in fds:
        os.close(r); os.close(w)
    for rq, rs in fifos:
        os.remove(rq); os.remove(rs)
    return total


def find_az(path):
    cands = [path] if path else [os.path.join(ROOT, 'build', 'az'), os.path.join(ROOT, 'build', 'az.exe'),
                                 os.path.join(ROOT, 'build', 'Release', 'az.exe')]
    for c in cands:
        if c and os.path.isfile(c):
            return os.path.abspath(c)
    sys.exit('az binary not found: build it first (`make`, or cmake) or pass --az PATH')


# settings that define the algorithm; --resume restores these from <run>/config.json
ALGO_KEYS = ['games', 'sims', 'steps', 'batch', 'buffer', 'lr', 'wd', 'hidden', 'layers', 'start', 'opening_file', 'cpuct', 'tau1', 'tau2',
             'temp_moves', 'dir_alpha', 'dir_eps', 'credit', 'credit_ramp', 'restart_prob', 'self_imitation', 'dist_k',
             'classic', 'q_max_weight', 'start_tours', 'curriculum_start', 'curriculum_every', 'curriculum_mode', 'curriculum_min_finish',
             'curriculum_balance']


def tour_lengths(fn):
    """Number of jumps in each tour of a one-tour-per-line file."""
    return [n for n in (len(re.findall(r'\(\d+, [a-h][1-8]\)', line)) for line in open(fn)) if n > 1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_argument_group('run')
    g.add_argument('--run', default='runs/default', help='run directory (created if needed)')
    g.add_argument('--rounds', type=int, default=200, help='rounds to run (self-play + training each)')
    g.add_argument('--workers', type=int, default=max(1, (os.cpu_count() or 2) // 2),
                   help='parallel self-play processes (default: half the logical CPUs)')
    g.add_argument('--resume', action='store_true', help='continue the run in --run (restores its settings)')
    g.add_argument('--init-weights', default=None, help='start from these weights instead of a fresh network')
    g.add_argument('--az', default=None, help='path to the az binary (default: build/az)')
    g.add_argument('--backend', choices=['numpy', 'torch'], default='numpy', help='trainer: numpy (CPU) or torch')
    g.add_argument('--device', default='cuda', help='torch device for --backend torch (cuda or cpu)')
    g.add_argument('--gpu-selfplay', action='store_true',
                   help='evaluate the network for self-play here, in batches (on the GPU with --backend torch); '
                        'each worker plays all its games at once, so raise --games (e.g. 512)')
    g.add_argument('--infer-leaves', type=int, default=8, help='leaf positions each game sends per batch')
    g = ap.add_argument_group('self-play and training')
    g.add_argument('--games', type=int, default=32, help='self-play games per round (split across workers)')
    g.add_argument('--sims', type=int, default=1600, help='tree-search simulations per move')
    g.add_argument('--steps', type=int, default=150, help='training steps per round')
    g.add_argument('--batch', type=int, default=256)
    g.add_argument('--buffer', type=int, default=150000, help='replay buffer size (positions)')
    g.add_argument('--lr', type=float, default=1e-3)
    g.add_argument('--wd', type=float, default=1e-5)
    g.add_argument('--hidden', type=int, default=256, help='network hidden width')
    g.add_argument('--layers', type=int, default=2, help='number of hidden layers')
    g = ap.add_argument_group('where games start')
    g.add_argument('--start', choices=['a1', 'opening'], default='a1',
                   help='a1: every game starts at a1, T=0; opening: after the fixed 23-move opening')
    g.add_argument('--opening-file', default='data/opening_23.txt')
    g.add_argument('--restart-prob', type=float, default=0.0,
                   help='chance a game instead starts from a random point of the best tour (Go-Explore); 0 = off')
    g.add_argument('--seed-tour', default=None, help='tour file to use as the initial best tour (for restarts)')
    g.add_argument('--self-imitation', type=int, default=0, help='copies of the best tour added to the buffer each round')
    g = ap.add_argument_group('search')
    g.add_argument('--classic', action='store_true',
                   help='classic AlphaZero averaging instead of the single-player search (max/exact values, solved '
                        'subtrees, tree reuse, caches, tours found inside the search, exact-floor value targets)')
    g.add_argument('--q-max-weight', type=float, default=0.5,
                   help='single-player search: weight of the best result vs the average in node values')
    g = ap.add_argument_group('exploration')
    g.add_argument('--cpuct', type=float, default=1.5, help='PUCT constant C')
    g.add_argument('--tau1', type=float, default=1.0, help='move temperature for the first --temp-moves moves')
    g.add_argument('--tau2', type=float, default=0.0, help='move temperature afterwards (0 = always the most-visited move)')
    g.add_argument('--temp-moves', type=int, default=25)
    g.add_argument('--dir-alpha', type=float, default=0.3, help='Dirichlet root-noise alpha')
    g.add_argument('--dir-eps', type=float, default=0.25, help='Dirichlet root-noise weight')
    g = ap.add_argument_group('partial credit (training only)')
    g.add_argument('--credit', type=float, default=0.0,
                   help='fraction of its minutes a stuck tour keeps until the first finish (0 = puzzle rules)')
    g.add_argument('--credit-ramp', type=int, default=60, help='rounds over which the credit then fades to 0')
    g.add_argument('--dist-k', type=float, default=0.0,
                   help='if > 0, a stuck tour keeps exp(-k * d) of its minutes instead (replaces --credit); '
                        'd = 3D distance from the knight to h8, scaled so d = 1 at the start')
    g = ap.add_argument_group('curriculum: start games partway along known tours, moving back toward a1')
    g.add_argument('--start-tours', default=None, help='tour file, one tour per line (e.g. data/tours/curriculum.txt)')
    g.add_argument('--curriculum-mode', choices=['start', 'end'], default='start',
                   help='start: begin K moves into a random tour, K falling by 1 each step until games start at a1; '
                        'end: begin K moves before that tour finishes, K rising by 1 each step')
    g.add_argument('--curriculum-start', type=int, default=20, help='first K (start mode) or first K (end mode, e.g. 1)')
    g.add_argument('--curriculum-every', type=int, default=10, help='rounds at each K before stepping')
    g.add_argument('--curriculum-min-finish', type=float, default=0.0,
                   help='also require this fraction of a round\'s games to finish before stepping (0 = pure schedule)')
    g.add_argument('--curriculum-balance', type=float, default=1.0,
                   help='tours are grouped by their first 10 moves; a group is picked with probability ~ size^this '
                        '(1 = every tour equally likely, 0 = every opening equally likely)')
    a = ap.parse_args()

    run = os.path.abspath(a.run); os.makedirs(run, exist_ok=True)
    cfg_fn, state_fn = os.path.join(run, 'config.json'), os.path.join(run, 'state.json')
    if a.resume:
        if not os.path.exists(cfg_fn) or not os.path.exists(state_fn):
            sys.exit(f'nothing to resume in {run}')
        saved = json.load(open(cfg_fn))
        for k in ALGO_KEYS:
            if k in saved:
                setattr(a, k, saved[k])
        state = json.load(open(state_fn))
    else:
        if os.path.exists(state_fn):
            sys.exit(f'{run} already holds a run; use --resume or a new --run')
        state = {'round': 0, 'best': 0, 'first_finish': None}
        json.dump({k: getattr(a, k) for k in ALGO_KEYS}, open(cfg_fn, 'w'), indent=1)
    az = find_az(a.az)
    opening_file = os.path.abspath(os.path.join(ROOT, a.opening_file)) if not os.path.isabs(a.opening_file) else a.opening_file
    open_len = 0
    if a.start == 'opening':
        open_len = len(re.findall(r'\d+\s+[a-h][1-8]', open(opening_file).read()))

    net = Net(np.random.default_rng(0), lr=a.lr, wd=a.wd, hidden=a.hidden, layers=a.layers)
    w_cur = os.path.join(run, 'w_cur.bin')
    if a.resume:
        net.load(w_cur)
    elif a.init_weights:
        net.load(a.init_weights)
    if a.gpu_selfplay and os.name == 'nt':
        sys.exit('--gpu-selfplay needs Linux or WSL2 (it uses named pipes)')
    if a.backend == 'torch':
        from torch_backend import TorchNet
        net = TorchNet(net, device=a.device, lr=a.lr, wd=a.wd)
    net.save(w_cur)

    best_fn = os.path.join(run, 'best_overall.txt')
    if a.seed_tour and not os.path.exists(best_fn):
        shutil.copy(a.seed_tour, best_fn)
        state['best'] = max(state['best'], int(open(best_fn).readline()))
    buf_fn = os.path.join(run, 'buffer.npy')
    buf = np.load(buf_fn) if a.resume and os.path.exists(buf_fn) else np.zeros(0, SAMPLE)
    rng = np.random.default_rng(state['round'] + 12345)
    best, first_fin = state['best'], state['first_finish']
    best_target = state.get('best_target', 0.0)
    tgt_fn = os.path.join(run, 'best_target.txt')  # tour with the highest training target so far
    # curriculum state: K (moves into / before the end of a start tour) and rounds spent at this K
    tours_fn, max_len = None, 0
    if a.start_tours:
        tours_fn = os.path.abspath(os.path.join(ROOT, a.start_tours)) if not os.path.isabs(a.start_tours) else a.start_tours
        lens = tour_lengths(tours_fn)
        if not lens:
            sys.exit(f'no tours found in {tours_fn}')
        max_len = max(lens)
        print(f'curriculum: {len(lens)} start tours ({min(lens)}-{max_len} jumps), mode {a.curriculum_mode}, '
              f'K from {a.curriculum_start}, stepping every {a.curriculum_every} rounds', flush=True)
    cur_k = state.get('prefix_len', a.curriculum_start if tours_fn else 0)
    cur_rounds = state.get('rounds_at_prefix', 0)
    log = open(os.path.join(run, 'log.txt'), 'a')
    csv_fn = os.path.join(run, 'log.csv')
    if not os.path.exists(csv_fn):
        open(csv_fn, 'w').write('round,games,finished,mean_finish,max_finish,best,credit,mean_moves,mean_minutes,'
                                'max_minutes,mean_dist,mean_target,max_target,policy_loss,value_loss,selfplay_s,train_s,'
                                'prefix\n')
    print(f'run {run}: az={az} workers={a.workers} backend={a.backend} start={a.start} '
          f'cpuct={a.cpuct} tau={a.tau1}/{a.tau2} credit={a.credit} dist_k={a.dist_k} net={a.layers}x{a.hidden} '
          f'selfplay_eval={"batched" if a.gpu_selfplay else "in-worker CPU"} '
          f'search={"classic" if a.classic else "single-player"}', flush=True)

    for it in range(state['round'], state['round'] + a.rounds):
        kappa = a.credit if first_fin is None else a.credit * max(0.0, 1 - (it - first_fin) / a.credit_ramp)
        env = dict(os.environ, CPUCT=str(a.cpuct), TAU1=str(a.tau1), TAU2=str(a.tau2), DIRA=str(a.dir_alpha),
                   DIRE=str(a.dir_eps), OPENING='1' if a.start == 'opening' else '0', OPENING_FILE=opening_file,
                   CREDIT=f'{kappa:.4f}', DISTK=str(a.dist_k), NET_H=str(a.hidden), NET_LAYERS=str(a.layers),
                   SP='0' if a.classic else '1', QMAX_W=str(0.0 if a.classic else a.q_max_weight))
        if tours_fn and cur_k > 0:
            env.update(PREFIX_TOURS=tours_fn, PREFIX_LEN=str(cur_k), PREFIX_BALANCE=str(a.curriculum_balance),
                       PREFIX_FROM_END='1' if a.curriculum_mode == 'end' else '0')
        t0 = time.time()
        nw = max(1, min(a.workers, a.games))
        per = [a.games // nw + (1 if w < a.games % nw else 0) for w in range(nw)]
        procs, logs, fifos, evals = [], [], [], 0
        for w in range(nw):
            args = [az, 'selfplay', w_cur, os.path.join(run, f's_{w}.bin'), str(per[w]), str(a.sims),
                    str(it * 1000 + w + 1), str(a.temp_moves), os.path.join(run, f'best_{w}.txt')]
            if a.restart_prob > 0 and os.path.exists(best_fn):
                args += [best_fn, str(a.restart_prob)]
            wenv = env
            if a.gpu_selfplay:  # this worker sends its leaf positions here for evaluation
                rq, rs = os.path.join(run, f'infer_req_{w}'), os.path.join(run, f'infer_resp_{w}')
                for f in (rq, rs):
                    if os.path.exists(f): os.remove(f)
                    os.mkfifo(f)
                fifos.append((rq, rs)); wenv = dict(env, INFER_REQ=rq, INFER_RESP=rs, INFER_LEAVES=str(a.infer_leaves))
            logs.append(open(os.path.join(run, f'selfplay_{w}.log'), 'w+'))
            procs.append(subprocess.Popen(args, stdout=logs[-1], env=wenv, cwd=ROOT))
        if a.gpu_selfplay:
            evals = serve_inference(net, fifos)
        for p in procs:
            p.wait()
        outs = []
        for f in logs:
            f.seek(0); outs.append(f.read()); f.close()
        if any(p.returncode != 0 for p in procs):
            sys.exit('a self-play worker failed; see the message above')
        rows = [l.split() for o in outs for l in o.splitlines() if l.startswith('EP')]
        scores = np.array([int(r[5]) for r in rows]); ends = np.array([int(r[7]) for r in rows])
        nmoves = np.array([int(r[3]) for r in rows])
        founds = np.array([int(r[11]) if len(r) > 11 else int(r[5]) for r in rows])
        dists = np.array([float(r[9]) if len(r) > 9 else 0.0 for r in rows])
        if a.dist_k > 0:
            targets = np.where(scores > 0, scores, ends * np.exp(-a.dist_k * dists))
        else:
            targets = np.where(scores > 0, scores, ends * kappa)
        if first_fin is None and scores.max() > 0:
            first_fin = it
        k_used = cur_k
        if tours_fn and cur_k > 0:  # step the curriculum
            cur_rounds += 1
            if cur_rounds >= a.curriculum_every and np.mean(scores > 0) >= a.curriculum_min_finish:
                cur_rounds = 0
                if a.curriculum_mode == 'start':
                    cur_k -= 1  # 0 = games start at a1
                else:
                    cur_k = cur_k + 1 if cur_k < max_len else 0
        for w in range(nw):
            fb = os.path.join(run, f'best_{w}.txt')
            if os.path.exists(fb) and os.path.getsize(fb) > 0:
                sc = int(open(fb).readline())
                if sc > best:
                    best = sc
                    shutil.copy(fb, best_fn)
                os.remove(fb)
            if os.path.exists(fb + '.target') and os.path.getsize(fb + '.target') > 0:
                tv = float(open(fb + '.target').readline())
                if tv > best_target:
                    best_target = tv
                    shutil.copy(fb + '.target', tgt_fn)
                os.remove(fb + '.target')
        new = np.concatenate([load_samples(os.path.join(run, f's_{w}.bin')) for w in range(nw)])
        if a.self_imitation and os.path.exists(best_fn):
            new = np.concatenate([new] + [tour_samples(best_fn, open_len)] * a.self_imitation)
        buf = np.concatenate([buf, new])[-a.buffer:]
        t1 = time.time()
        lps, lvs = [], []
        for s in range(a.steps):
            b = buf[rng.integers(0, len(buf), a.batch)]
            X = featurize(b['alt'], b['vis'], b['pos'].astype(np.int64), b['T'].astype(np.float32))
            mask = ((b['mask'][:, None] >> np.arange(NA)) & 1).astype(bool)
            lp, lv = net.step(X, mask, np.ascontiguousarray(b['pi']), np.ascontiguousarray(b['z']))
            lps.append(lp); lvs.append(lv)
        net.save(w_cur)
        if it % 10 == 0:
            net.save(os.path.join(run, f'w_{it:04d}.bin'))
        # write-then-rename so a run killed mid-save can always be resumed
        np.save(buf_fn + '.tmp.npy', buf); os.replace(buf_fn + '.tmp.npy', buf_fn)
        with open(state_fn + '.tmp', 'w') as f:
            json.dump({'round': it + 1, 'best': best, 'first_finish': first_fin, 'best_target': best_target,
                       'prefix_len': cur_k, 'rounds_at_prefix': cur_rounds}, f)
        os.replace(state_fn + '.tmp', state_fn)
        t2 = time.time()
        fin = scores[scores > 0]
        msg = (f'round {it} games {len(scores)} finished {len(fin)} meanT {fin.mean() if len(fin) else 0:.0f} '
               f'maxT {scores.max()} best {best} credit {kappa:.2f} moves {nmoves.mean():.0f} '
               f'minutes {ends.mean():.0f} maxmin {ends.max()} dist {dists.mean():.3f} target {targets.mean():.0f} '
               f'maxtarget {targets.max():.0f} besttarget {best_target:.0f} lp {np.mean(lps):.3f} lv {np.mean(lvs):.4f} '
               f'selfplay {t1 - t0:.0f}s train {t2 - t1:.0f}s')
        msg += f' found {founds.max()}'
        if a.gpu_selfplay:
            msg += f' evals {evals}'
        if tours_fn:
            msg += f' start {a.curriculum_mode}{k_used}' if k_used > 0 else ' start a1'
        print(msg, flush=True); log.write(msg + '\n'); log.flush()
        with open(csv_fn, 'a') as f:
            f.write(f'{it},{len(scores)},{len(fin)},{fin.mean() if len(fin) else 0:.1f},{scores.max()},{best},'
                    f'{kappa:.4f},{nmoves.mean():.1f},{ends.mean():.1f},{ends.max()},{dists.mean():.4f},'
                    f'{targets.mean():.1f},{targets.max():.1f},{np.mean(lps):.5f},'
                    f'{np.mean(lvs):.6f},{t1 - t0:.1f},{t2 - t1:.1f},{k_used if tours_fn else 0}\n')


if __name__ == '__main__':
    main()
