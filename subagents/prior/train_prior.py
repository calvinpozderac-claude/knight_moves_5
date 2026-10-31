#!/usr/bin/env python3
"""Train the learned move prior (listwise softmax over each position's legal moves) with numpy.

Target per position: softmax((L_i - Lmax) / tau) over the moves with a known tour through them
(L_i = best final score of a known tour through the child state), 0 for moves with no known tour.
Position weight: (Lmax / 8123)^wpow + wmin, so positions on long tours dominate.
Model: x' = (x - mean)/sd, logit = w2 . tanh(W1 x' + b1) (--hidden H) or w . x' (--hidden 0).

usage: train_prior.py DATA.bin [--hidden 32] [--epochs 30] [--out prior.txt] [--tau 50]
Reports top-1 accuracy (the predicted move is one of the best-labelled moves) on train and val
positions, against uniform / max-wait / zero-wait baselines.
"""
import argparse, sys, time
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('data')
ap.add_argument('--hidden', type=int, default=32)
ap.add_argument('--epochs', type=int, default=30)
ap.add_argument('--batch', type=int, default=1024, help='positions per minibatch')
ap.add_argument('--lr', type=float, default=3e-3)
ap.add_argument('--l2', type=float, default=1e-5)
ap.add_argument('--tau', type=float, default=50.0)
ap.add_argument('--wpow', type=float, default=2.0)
ap.add_argument('--wmin', type=float, default=0.02)
ap.add_argument('--seed', type=int, default=1)
ap.add_argument('--out', default=None)
ap.add_argument('--eval-every', type=int, default=5)
ap.add_argument('--drop', default='', help='comma-separated feature indices to zero (ablation)')
args = ap.parse_args()

raw = open(args.data, 'rb').read()
F, NR, NG = np.frombuffer(raw[:12], dtype=np.int32)
g = np.frombuffer(raw[12:12 + NG * 20], dtype=np.int32).reshape(NG, 5)
gstart, gcount, gsplit, gidx = g[:, 0].copy(), g[:, 1].copy(), g[:, 2].copy(), g[:, 3].copy()
gL = g[:, 4].copy().view(np.float32)
rows = np.frombuffer(raw[12 + NG * 20:], dtype=np.float32).reshape(NR, F + 1)
lab, X = rows[:, 0].copy(), rows[:, 1:].copy()
if args.drop:
    for k in args.drop.split(','): X[:, int(k)] = 0
gid = np.repeat(np.arange(NG), gcount)
print(f'{NG} positions, {NR} rows, {F} features; train {np.sum(gsplit == 0)}, val {np.sum(gsplit == 1)}')

# targets
Lmax_r = gL[gid]
tgt = np.where(lab > 0, np.exp((lab - Lmax_r) / args.tau), 0.0)
tsum = np.add.reduceat(tgt, gstart)
tgt = (tgt / tsum[gid]).astype(np.float64)
gw = (gL / 8123.0) ** args.wpow + args.wmin
best_r = (lab >= Lmax_r) & (lab > 0)

tr_mask_g = gsplit == 0
mean = X[tr_mask_g[gid]].mean(0); sd = X[tr_mask_g[gid]].std(0) + 1e-3
sd[X[tr_mask_g[gid]].std(0) < 1e-6] = 1.0
Xn = ((X - mean) / sd).astype(np.float32)

rng = np.random.default_rng(args.seed)
H = args.hidden
P = {}
if H > 0:
    P['W1'] = rng.normal(0, 1 / np.sqrt(F), (H, F)); P['b1'] = np.zeros(H); P['w2'] = rng.normal(0, 0.1, H)
else:
    P['w2'] = np.zeros(F)

def forward(Xb):
    if H == 0: return Xb @ P['w2'], None
    hdn = np.tanh(Xb @ P['W1'].T + P['b1']); return hdn @ P['w2'], hdn

def seg_softmax(z, starts, gidl):
    m = np.maximum.reduceat(z, starts); e = np.exp(z - m[gidl]); s = np.add.reduceat(e, starts)
    return e / s[gidl], m + np.log(s)

def batch_rows(gs):
    idx = np.concatenate([np.arange(gstart[k], gstart[k] + gcount[k]) for k in gs])
    starts = np.concatenate([[0], np.cumsum(gcount[gs])[:-1]])
    gidl = np.repeat(np.arange(len(gs)), gcount[gs])
    return idx, starts, gidl

def evaluate(gs):
    idx, starts, gidl = batch_rows(gs)
    z, _ = forward(Xn[idx]); p, lse = seg_softmax(z, starts, gidl)
    w = gw[gs]
    nll = -np.add.reduceat(tgt[idx] * np.log(p + 1e-12), starts)
    # top-1: predicted argmax is a best move
    def top1(score):
        score = score + 1e-7 * rng.random(len(score))
        mx = np.maximum.reduceat(score, starts)
        return np.add.reduceat(((score >= mx[gidl]) & best_r[idx]).astype(float), starts)
    acc = top1(z)
    pbest = np.add.reduceat(p * best_r[idx], starts)
    uni = np.add.reduceat(best_r[idx].astype(float), starts) / gcount[gs]
    wait = X[idx, 0]
    accw = top1(wait + 1e-6 * rng.random(len(idx)))
    acc0 = top1(-wait + 1e-6 * rng.random(len(idx)))
    out = {}
    for name, sel in [('all', np.ones(len(gs), bool)), ('long', gL[gs] >= 7000), ('dance', (gidx[gs] >= 35) & (gL[gs] >= 7000)), ('open', gidx[gs] < 35)]:
        if sel.sum() == 0: continue
        ww = w[sel]
        out[name] = dict(n=int(sel.sum()), nll=float(np.sum(nll[sel] * ww) / ww.sum()), acc=float(acc[sel].mean()), pbest=float(pbest[sel].mean()),
                         uniform=float(uni[sel].mean()), maxwait=float(accw[sel].mean()), zerowait=float(acc0[sel].mean()))
    return out

def show(tag, d):
    for k, v in d.items():
        print(f"  {tag:5s} {k:5s} n={v['n']:6d} nll={v['nll']:.4f} top1={v['acc']:.3f} p(best)={v['pbest']:.3f} | uniform={v['uniform']:.3f} maxwait={v['maxwait']:.3f} zerowait={v['zerowait']:.3f}")

tr_g = np.where(gsplit == 0)[0]; va_g = np.where(gsplit == 1)[0]
m = {k: np.zeros_like(v) for k, v in P.items()}; v2 = {k: np.zeros_like(v) for k, v in P.items()}
step = 0; t0 = time.time()
# fixed minibatches of groups (random assignment, reused every epoch, cheap)
_perm = rng.permutation(tr_g); tr_batches = []
for b in range(0, len(_perm), args.batch):
    gs = np.sort(_perm[b:b + args.batch]); tr_batches.append((gs,) + batch_rows(gs))
for ep in range(args.epochs):
    for b in rng.permutation(len(tr_batches)):
        gs, idx, starts, gidl = tr_batches[b]
        Xb = Xn[idx]
        z, hdn = forward(Xb); p, _ = seg_softmax(z, starts, gidl)
        wr = gw[gs][gidl] / gw[gs].sum()
        dz = (p - tgt[idx]) * wr
        G = {}
        if H == 0: G['w2'] = Xb.T @ dz
        else:
            G['w2'] = hdn.T @ dz
            dh = np.outer(dz, P['w2']) * (1 - hdn ** 2)
            G['W1'] = dh.T @ Xb; G['b1'] = dh.sum(0)
        step += 1
        for k in P:
            gk = G[k] + args.l2 * P[k]
            m[k] = 0.9 * m[k] + 0.1 * gk; v2[k] = 0.999 * v2[k] + 0.001 * gk * gk
            P[k] -= args.lr * (m[k] / (1 - 0.9 ** step)) / (np.sqrt(v2[k] / (1 - 0.999 ** step)) + 1e-8)
    if ep % args.eval_every == args.eval_every - 1 or ep == args.epochs - 1:
        e = evaluate(tr_g)['all']
        line = f'epoch {ep + 1} ({time.time() - t0:.0f}s): train nll {e["nll"]:.4f} top1 {e["acc"]:.3f}'
        if len(va_g): ev = evaluate(va_g)['all']; line += f' | val nll {ev["nll"]:.4f} top1 {ev["acc"]:.3f}'
        print(line); sys.stdout.flush()

print('final:'); show('train', evaluate(tr_g))
if len(va_g): show('val', evaluate(va_g))
if args.out:
    with open(args.out, 'w') as f:
        f.write(f'{F} {H}\n')
        f.write(' '.join(f'{x:.6g}' for x in mean) + '\n'); f.write(' '.join(f'{x:.6g}' for x in sd) + '\n')
        if H > 0:
            for r in P['W1']: f.write(' '.join(f'{x:.6g}' for x in r) + '\n')
            f.write(' '.join(f'{x:.6g}' for x in P['b1']) + '\n')
        f.write(' '.join(f'{x:.6g}' for x in P['w2']) + '\n')
    print('wrote', args.out)
