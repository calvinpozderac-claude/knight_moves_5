#!/usr/bin/env python3
"""Offline training of the astar_cpu value model (numpy only).

Reads the feature matrix written by `astar_cpu feat` and fits a 1-hidden-layer MLP with two outputs, the same
loss as nnastar: binary cross-entropy for "a finish is reachable" (label > 0) and smooth-L1 (beta 0.5) on the
future minutes / 1000 (label - T) for finishable positions. Adam, minibatches, early stopping on a held-out
split. Writes the model in the text format astar_cpu reads (ASTAR_MLP ver F H; mu; sd; W1; b1; W2; b2).

  python3 train.py --feat tmp/feat.bin --out tmp/model.txt --hidden 64 --minutes 10
"""
import argparse
import sys
import time

import numpy as np

FEAT_VERSION = 1


def load(fn):
    hdr = np.fromfile(fn, dtype=np.int32, count=4)
    if hdr[0] != 0x54414641 or hdr[1] != FEAT_VERSION:
        sys.exit(f'{fn}: not an astar_cpu feature file of version {FEAT_VERSION}')
    n, F = int(hdr[2]), int(hdr[3])
    a = np.fromfile(fn, dtype=np.float32, offset=16).reshape(n, F + 4)
    return a[:, :F], a[:, F], a[:, F + 1], a[:, F + 2], a[:, F + 3], F


class MLP:
    def __init__(self, F, H, rng):
        self.W1 = (rng.standard_normal((F, H)) * np.sqrt(2.0 / F)).astype(np.float32)
        self.b1 = np.zeros(H, np.float32)
        self.W2 = (rng.standard_normal((H, 2)) * np.sqrt(1.0 / H)).astype(np.float32)
        self.b2 = np.zeros(2, np.float32)
        self.params = ['W1', 'b1', 'W2', 'b2']
        self.m = {k: np.zeros_like(getattr(self, k)) for k in self.params}
        self.v = {k: np.zeros_like(getattr(self, k)) for k in self.params}
        self.t = 0

    def forward(self, z):
        hp = z @ self.W1 + self.b1
        h = np.maximum(hp, 0)
        o = h @ self.W2 + self.b2
        return hp, h, o


def losses(o, T, L, w, fut_w, beta=0.5):
    fin = (L > 0).astype(np.float32)
    lg = o[:, 0]
    p = 1 / (1 + np.exp(-np.clip(lg, -30, 30)))
    lf = -(fin * np.log(p + 1e-7) + (1 - fin) * np.log(1 - p + 1e-7))
    tgt = np.maximum(L - T, 0) / 1000.0
    d = o[:, 1] - tgt
    ad = np.abs(d)
    lu = np.where(ad < beta, 0.5 * d * d / beta, ad - 0.5 * beta) * fin
    gd = np.where(ad < beta, d / beta, np.sign(d)) * fin
    W = w.sum()
    Wf = max((w * fin).sum(), 1e-9)
    loss = (w * lf).sum() / W + fut_w * (w * lu).sum() / Wf
    g = np.zeros_like(o)
    g[:, 0] = w * (p - fin) / W
    g[:, 1] = fut_w * w * gd / Wf
    return loss, (w * lf).sum() / W, (w * lu).sum() / Wf, g


def step(net, z, T, L, w, lr, wd, fut_w):
    hp, h, o = net.forward(z)
    loss, lf, lu, go = losses(o, T, L, w, fut_w)
    gW2 = h.T @ go
    gb2 = go.sum(0)
    gh = (go @ net.W2.T) * (hp > 0)
    gW1 = z.T @ gh
    gb1 = gh.sum(0)
    grads = {'W1': gW1 + wd * net.W1, 'b1': gb1, 'W2': gW2 + wd * net.W2, 'b2': gb2}
    net.t += 1
    b1, b2 = 0.9, 0.999
    c1, c2 = 1 - b1 ** net.t, 1 - b2 ** net.t
    for k in net.params:
        g = grads[k].astype(np.float32)
        net.m[k] = b1 * net.m[k] + (1 - b1) * g
        net.v[k] = b2 * net.v[k] + (1 - b2) * g * g
        setattr(net, k, (getattr(net, k) - lr * (net.m[k] / c1) / (np.sqrt(net.v[k] / c2) + 1e-8)).astype(np.float32))
    return loss, lf, lu


def evaluate(net, z, T, L, w, fut_w, chunk=65536):
    tot = np.zeros(3)
    for s in range(0, len(z), chunk):
        _, _, o = net.forward(z[s:s + chunk])
        loss, lf, lu, _ = losses(o, T[s:s + chunk], L[s:s + chunk], w[s:s + chunk], fut_w)
        tot += np.array([loss, lf, lu]) * len(o)
    return tot / max(1, len(z))


def save(fn, net, mu, sd, F, H):
    with open(fn + '.tmp', 'w') as f:
        f.write(f'ASTAR_MLP {FEAT_VERSION} {F} {H}\n')
        for arr in (mu, sd, net.W1.reshape(-1), net.b1, net.W2.reshape(-1), net.b2):
            a = np.asarray(arr, np.float64).reshape(-1)
            for s in range(0, len(a), 16):
                f.write(' '.join(f'{x:.7g}' for x in a[s:s + 16]) + '\n')
    import os
    os.replace(fn + '.tmp', fn)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--feat', required=True, nargs='+', help='feature files from `astar_cpu feat`')
    ap.add_argument('--out', required=True)
    ap.add_argument('--hidden', type=int, default=64)
    ap.add_argument('--batch', type=int, default=1024)
    ap.add_argument('--lr', type=float, default=1e-3)
    ap.add_argument('--wd', type=float, default=1e-5)
    ap.add_argument('--fut-w', type=float, default=1.0)
    ap.add_argument('--elite-w', type=float, default=5.0, help='sample weight of known-tour positions (src 5)')
    ap.add_argument('--high-w', type=float, default=3.0, help='extra weight for positions labelled >= --high-min')
    ap.add_argument('--high-min', type=float, default=7500)
    ap.add_argument('--minutes', type=float, default=10, help='time budget')
    ap.add_argument('--epochs', type=int, default=200)
    ap.add_argument('--patience', type=int, default=6, help='evaluations without a better validation loss')
    ap.add_argument('--val', type=float, default=0.05)
    ap.add_argument('--eval-every', type=int, default=2000, help='steps between validations')
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    t0 = time.time()
    parts = [load(f) for f in a.feat]
    F = parts[0][5]
    X = np.concatenate([p[0] for p in parts]); T = np.concatenate([p[1] for p in parts])
    L = np.concatenate([p[2] for p in parts]); src = np.concatenate([p[4] for p in parts])
    n = len(X)
    w = np.ones(n, np.float32)
    w[src == 5] *= a.elite_w
    w[L >= a.high_min] *= a.high_w
    print(f'{n} samples, {F} features, {(L > 0).mean():.1%} finishable, {(L >= a.high_min).mean():.1%} labelled >= {a.high_min:.0f}, '
          f'max label {L.max():.0f}; loaded in {time.time() - t0:.1f}s', flush=True)
    rng = np.random.default_rng(a.seed)
    perm = rng.permutation(n)
    nv = max(1, int(n * a.val))
    vi, ti = perm[:nv], perm[nv:]
    mu = X[ti].mean(0)
    sd = X[ti].std(0)
    sd[sd < 1e-4] = 1.0
    X -= mu  # normalise in place (the matrix is the biggest thing in memory: 2.3 KB per sample)
    X /= sd
    Z = X
    net = MLP(F, a.hidden, rng)
    best, best_state, bad, steps = 1e18, None, 0, 0
    lr = a.lr
    done = False
    for ep in range(a.epochs):
        rng.shuffle(ti)
        acc = np.zeros(3); na = 0
        for s in range(0, len(ti) - a.batch + 1, a.batch):
            b = ti[s:s + a.batch]
            acc += step(net, Z[b], T[b], L[b], w[b], lr, a.wd, a.fut_w); na += 1
            steps += 1
            if steps % a.eval_every == 0:
                vl = evaluate(net, Z[vi], T[vi], L[vi], w[vi], a.fut_w)
                tr = acc / max(1, na); acc[:] = 0; na = 0
                mark = ''
                if vl[0] < best - 1e-5:
                    best, bad, mark = vl[0], 0, ' *'
                    best_state = {k: getattr(net, k).copy() for k in net.params}
                    save(a.out, net, mu, sd, F, a.hidden)
                else:
                    bad += 1
                    if bad == a.patience // 2:
                        lr *= 0.3
                print(f'[{time.time() - t0:6.0f}s] epoch {ep} step {steps}: train {tr[0]:.4f} (fin {tr[1]:.4f} fut {tr[2]:.4f}) '
                      f'val {vl[0]:.4f} (fin {vl[1]:.4f} fut {vl[2]:.4f}) lr {lr:.1e}{mark}', flush=True)
                if bad >= a.patience or time.time() - t0 > a.minutes * 60:
                    done = True
                    break
        if done:
            break
    if best_state is None:
        best_state = {k: getattr(net, k).copy() for k in net.params}
    for k, v in best_state.items():
        setattr(net, k, v)
    save(a.out, net, mu, sd, F, a.hidden)
    vl = evaluate(net, Z[vi], T[vi], L[vi], w[vi], a.fut_w)
    # quick sanity: mean absolute error of the future minutes on finishable validation positions
    _, _, o = net.forward(Z[vi])
    m = L[vi] > 0
    mae = np.abs(np.maximum(o[m, 1], 0) * 1000 - np.maximum(L[vi][m] - T[vi][m], 0)).mean() if m.any() else 0
    acc = ((o[:, 0] > 0) == (L[vi] > 0)).mean()
    print(f'saved {a.out}: val loss {vl[0]:.4f}, finish accuracy {acc:.3f}, future-minutes MAE {mae:.0f} min, {steps} steps, '
          f'{time.time() - t0:.0f}s', flush=True)


if __name__ == '__main__':
    main()
