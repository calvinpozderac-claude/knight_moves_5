"""AlphaZero training loop (NumPy network, C++ self-play/MCTS).
usage: python3 az_train.py ITERS [resume_weights]
"""
import sys, os, time, subprocess, struct, numpy as np

D_IN, H, NA = 64 * 21 + 64 + 10, 256, 24
TSCALE = 10000.0
SAMPLE = np.dtype([('alt', '<i2', 64), ('vis', 'u1', 64), ('pos', 'u1'), ('T', '<i4'),
                   ('mask', '<u4'), ('pi', '<f4', NA), ('z', '<f4')])
assert SAMPLE.itemsize == 301

def featurize(alt, vis, pos, T):
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
    def __init__(self, rng):
        self.p = {
            'W1': (rng.standard_normal((D_IN, H)) * np.sqrt(2 / 139)).astype(np.float32),
            'b1': np.zeros(H, np.float32),
            'W2': (rng.standard_normal((H, H)) * np.sqrt(2 / H)).astype(np.float32),
            'b2': np.zeros(H, np.float32),
            'Wp': (rng.standard_normal((H, NA)) * 0.01).astype(np.float32),
            'bp': np.zeros(NA, np.float32),
            'wv': (rng.standard_normal(H) * 0.01).astype(np.float32),
            'bv': np.full(1, -0.85, np.float32),
        }
        self.m = {k: np.zeros_like(v) for k, v in self.p.items()}
        self.v = {k: np.zeros_like(v) for k, v in self.p.items()}
        self.t = 0
    def save(self, fn):
        with open(fn, 'wb') as f:
            for k in ['W1', 'b1', 'W2', 'b2', 'Wp', 'bp', 'wv', 'bv']:
                f.write(self.p[k].astype(np.float32).tobytes())
    def load(self, fn):
        a = np.fromfile(fn, np.float32); o = 0
        for k in ['W1', 'b1', 'W2', 'b2', 'Wp', 'bp', 'wv', 'bv']:
            n = self.p[k].size; self.p[k] = a[o:o + n].reshape(self.p[k].shape).copy(); o += n
    def step(self, X, mask, pi, z, lr=1e-3, cv=1.0, wd=1e-5):
        P = self.p; B = X.shape[0]
        a1 = X @ P['W1'] + P['b1']; h1 = np.maximum(a1, 0)
        a2 = h1 @ P['W2'] + P['b2']; h2 = np.maximum(a2, 0)
        lg = h2 @ P['Wp'] + P['bp']
        lg = np.where(mask, lg, -1e9)
        lg = lg - lg.max(1, keepdims=True)
        e = np.exp(lg) * mask; sm = e / e.sum(1, keepdims=True)
        vlin = h2 @ P['wv'] + P['bv'][0]; v = 1 / (1 + np.exp(-vlin))
        lp = -(pi * np.log(sm + 1e-12)).sum(1).mean()
        lv = ((v - z) ** 2).mean()
        # grads
        dlg = (sm - pi) / B
        dv = cv * 2 * (v - z) / B * v * (1 - v)
        g = {}
        g['Wp'] = h2.T @ dlg; g['bp'] = dlg.sum(0)
        g['wv'] = h2.T @ dv; g['bv'] = np.array([dv.sum()], np.float32)
        dh2 = dlg @ P['Wp'].T + np.outer(dv, P['wv'])
        da2 = dh2 * (a2 > 0)
        g['W2'] = h1.T @ da2; g['b2'] = da2.sum(0)
        dh1 = da2 @ P['W2'].T
        da1 = dh1 * (a1 > 0)
        g['W1'] = X.T @ da1; g['b1'] = da1.sum(0)
        self.t += 1; b1, b2, eps = 0.9, 0.999, 1e-8
        for k in P:
            gk = g[k].astype(np.float32) + (wd * P[k] if k.startswith('W') else 0)
            self.m[k] = b1 * self.m[k] + (1 - b1) * gk
            self.v[k] = b2 * self.v[k] + (1 - b2) * gk * gk
            mh = self.m[k] / (1 - b1 ** self.t); vh = self.v[k] / (1 - b2 ** self.t)
            P[k] -= (lr * mh / (np.sqrt(vh) + eps)).astype(np.float32)
        return lp, lv

def load_samples(fn):
    if not os.path.exists(fn) or os.path.getsize(fn) == 0:
        return np.zeros(0, SAMPLE)
    return np.fromfile(fn, SAMPLE)

def main():
    iters = int(sys.argv[1])
    EP, SIMS, TEMPM = int(os.environ.get('EP', 12)), int(os.environ.get('SIMS', 200)), int(os.environ.get('TEMPM', 25))
    BUF, STEPS, BS = int(os.environ.get('BUF', 150000)), int(os.environ.get('STEPS', 120)), 256
    rng = np.random.default_rng(0)
    net = Net(rng)
    start_it = 0
    if len(sys.argv) > 2:
        net.load(sys.argv[2]); start_it = int(os.environ.get('START', 0))
    os.makedirs('azrun', exist_ok=True)
    net.save('azrun/w_cur.bin')
    for w in range(2):
        fb = f'azrun/best_{w}.txt'
        if os.path.exists(fb): os.remove(fb)
    buf = np.zeros(0, SAMPLE)
    best = int(open('azrun/best_overall.txt').readline()) if os.path.exists('azrun/best_overall.txt') else -1
    log = open('azrun/log.txt', 'a')
    for it in range(start_it, start_it + iters):
        t0 = time.time()
        procs = []
        for w in range(2):
            seed = it * 10 + w + 1
            args = ['./az', 'selfplay', 'azrun/w_cur.bin', f'azrun/s_{w}.bin', str(EP), str(SIMS), str(seed), str(TEMPM), f'azrun/best_{w}.txt']
            if os.path.exists('azrun/best_overall.txt'):
                args += ['azrun/best_overall.txt', os.environ.get('PSTART', '0.5')]
            procs.append(subprocess.Popen(args, stdout=subprocess.PIPE, text=True))
        outs = [p.communicate()[0] for p in procs]
        scores = [int(l.split()[-1]) for o in outs for l in o.splitlines() if l.startswith('EP')]
        for w in range(2):
            fb = f'azrun/best_{w}.txt'
            if os.path.exists(fb) and os.path.getsize(fb) > 0:
                sc = int(open(fb).readline())
                if sc > best:
                    best = sc
                    os.replace(fb, 'azrun/best_overall.txt') if False else open('azrun/best_overall.txt', 'w').write(open(fb).read())
        new = np.concatenate([load_samples(f'azrun/s_{w}.bin') for w in range(2)])
        buf = np.concatenate([buf, new])[-BUF:]
        t1 = time.time()
        lps, lvs = [], []
        for s in range(STEPS):
            idx = rng.integers(0, len(buf), BS)
            b = buf[idx]
            X = featurize(b['alt'], b['vis'], b['pos'].astype(np.int64), b['T'].astype(np.float32))
            mask = ((b['mask'][:, None] >> np.arange(NA)) & 1).astype(bool)
            lp, lv = net.step(X, mask, b['pi'], b['z'])
            lps.append(lp); lvs.append(lv)
        net.save('azrun/w_cur.bin')
        if it % 10 == 0:
            net.save(f'azrun/w_{it:04d}.bin')
        sc = np.array(scores)
        msg = (f'it {it} eps {len(sc)} meanT {sc.mean():.0f} maxT {sc.max()} fail {np.mean(sc == 0):.2f} '
               f'best {best} buf {len(buf)} lp {np.mean(lps):.3f} lv {np.mean(lvs):.4f} '
               f'selfplay {t1 - t0:.0f}s train {time.time() - t1:.0f}s')
        print(msg, flush=True); log.write(msg + '\n'); log.flush()

if __name__ == '__main__':
    main()
