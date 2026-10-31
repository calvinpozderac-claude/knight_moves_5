#!/usr/bin/env python3
"""NN-guided A* search for Knight Moves 5.

Pipeline (all state lives in one run folder, default runs/nnastar):
  gen     worker threads finish known tours from random cut points with NRPA searches of mixed strength
          and label every position with the best final score known to be reachable from it (0 if none)
  train   fit a ResNet value network (finish probability + future minutes) to those labels
  search  best-first (A*) search from a1 ordered by the network's predicted final score; meanwhile the
          workers label positions the search expands, the network keeps training on them, and the open
          list is re-scored whenever the search switches to the updated weights
  all     gen, then train, then search

Examples (from the repo root):
  .venv/bin/python -m nnastar.run all                                   # everything, default budgets
  .venv/bin/python -m nnastar.run search --hours 12                     # more search with the trained net
  .venv/bin/python -m nnastar.run search --anchor-tour data/tours/record_7911.txt --anchor-len 35
Stop any phase with Ctrl-C: it saves the network, data and best tour before exiting.
"""
import argparse
import copy
import json
import math
import os
import signal
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import torch

from . import lib as K
from .model import featurize, loss_fn, make_model, n_params, search_value, unpack, REC
from .replay import Replay, ShardWriter, dedup_max, load_shards

ROOT = K.ROOT
sys.path.insert(0, os.path.join(ROOT, 'tools'))

STOP = False


def _sigint(sig, frm):
    global STOP
    if STOP:
        raise KeyboardInterrupt
    STOP = True
    print('\nstopping after the current step (Ctrl-C again to abort)...', flush=True)


class Log:
    def __init__(self, run):
        self.f = open(os.path.join(run, 'log.txt'), 'a')
        self.t0 = time.time()

    def __call__(self, msg):
        line = f'[{time.strftime("%H:%M:%S")} {time.time() - self.t0:7.0f}s] {msg}'
        print(line, flush=True)
        self.f.write(line + '\n')
        self.f.flush()


def record_minutes():
    best = 0
    d = os.path.join(ROOT, 'data', 'tours')
    for f in os.listdir(d):
        if f.startswith('record_'):
            try:
                best = max(best, int(open(os.path.join(d, f)).readline()))
            except ValueError:
                pass
    return best


def setup_engine(a, log):
    K.init(a.seed)
    tot = 0
    for spec in a.tours:
        path, _, w = spec.partition(':')
        path = path if os.path.isabs(path) else os.path.join(ROOT, path)
        if not os.path.exists(path):
            log(f'  (tour file {path} not found, skipped)')
            continue
        n = K.load_tours(path, float(w or 1.0))
        tot += n
        log(f'  seed tours: {n} from {os.path.relpath(path, ROOT)} (weight {w or 1.0})')
    if tot == 0:
        raise SystemExit('no seed tours loaded: pass --tours FILE[:weight] ...')
    for name in ['p_level0', 'p_level1', 'p_level2', 'p_level3', 'probes', 'probe_states', 'prefix_rate', 'label_path_rate',
                 'label_level', 'label_level_shallow', 'shallow_depth', 'nice', 'anchor_pow', 'p_label', 'pool_min', 'frac_label']:
        K.set_param(name, getattr(a, name))


def worker_line(st, prev, dt):
    paths = st['paths'] - prev.get('paths', 0)
    fin = st['paths_fin'] - prev.get('paths_fin', 0)
    lj = st['label_jobs'] - prev.get('label_jobs', 0)
    smp = st['samples'] - prev.get('samples', 0)
    ro = st['rollouts'] - prev.get('rollouts', 0)
    tot_fin = st['paths_fin'] / max(1, st['paths'])
    lv = ' '.join(f"L{l} {st[f'l{l}_fin'] / max(1, st[f'l{l}']):.0%}" for l in range(4))
    return (f'paths {paths / dt:.0f}/s (finished {fin / max(1, paths):.0%}, overall {tot_fin:.0%}; {lv}) '
            f'label jobs {lj / dt:.0f}/s, samples {smp / dt / 1e3:.1f}k/s, rollouts {ro / dt / 1e6:.2f}M/s')


# ------------------------------------------------------------------------------------------------ gen
def cmd_gen(a):
    log = Log(a.run)
    log(f'== gen: {a.gen_minutes} min or {a.gen_samples / 1e6:.0f}M samples on {a.gen_threads} threads')
    setup_engine(a, log)
    K.set_param('frac_label', 0.0)
    data = os.path.join(a.run, 'data')
    os.makedirs(data, exist_ok=True)
    np.save(os.path.join(data, 'tours.npy'), K.tour_samples(a.tour_min_score))
    sw = ShardWriter(data, 'gen')
    w = K.Workers(a.gen_threads, a.seed)
    t0 = time.time()
    last, prev = t0, {}
    n = 0
    try:
        while not STOP and time.time() - t0 < a.gen_minutes * 60 and n < a.gen_samples:
            time.sleep(1.0)
            d = w.drain()
            n += len(d)
            sw.add(d)
            if time.time() - last >= 15:
                st = w.stats()
                log(f'gen {n / 1e6:.2f}M samples | ' + worker_line(st, prev, time.time() - last))
                prev, last = st, time.time()
    finally:
        w.stop()
        sw.add(w.drain())
        sw.flush()
        st = w.stats()
        log(f'gen done: {sw.written / 1e6:.2f}M samples in {time.time() - t0:.0f}s; paths {st["paths"]:.0f}, '
            f'finished {st["paths_fin"] / max(1, st["paths"]):.1%}')


# ------------------------------------------------------------------------------------------------ data / model
def load_base(a, log, device, merge_labels=False):
    data = os.path.join(a.run, 'data')
    tp = os.path.join(data, 'tours.npy')
    tours = np.load(tp) if os.path.exists(tp) else np.zeros(0, K.SAMPLE_DTYPE)
    arr = load_shards(os.path.join(data, 'gen_*.npy'), int(a.base_cap * 1.25), extra=tours)
    if len(arr) <= len(tours):
        raise SystemExit(f'no generated data in {data}: run the gen step first')
    n0 = len(arr) - len(tours)
    arr = dedup_max(arr)
    log(f'base data: {n0 / 1e6:.2f}M generated + {len(tours)} known-tour samples -> {len(arr) / 1e6:.2f}M unique states')
    if merge_labels:
        K.label_merge(arr)  # the best-label table starts with every base label
    rng = np.random.default_rng(a.seed)
    rng.shuffle(arr)
    if len(arr) > a.base_cap:
        arr = arr[:a.base_cap]
    nval = min(100_000, len(arr) // 50)
    val, arr = arr[:nval].copy(), arr[nval:]
    # known tours are a tiny but crucial part of the data: repeat them
    rep = np.concatenate([tours] * a.tour_repeat) if len(tours) and a.tour_repeat > 0 else tours[:0]
    base = Replay(len(arr) + len(rep), device)
    base.add(arr)
    base.add(rep)
    fin = (arr['label'] > 0).mean()
    log(f'base replay {base.n / 1e6:.2f}M on GPU ({fin:.0%} can finish), validation {len(val)}')
    del arr
    return base, val


def save_model(path, model, opt=None, extra=None):
    d = {'cfg': model.cfg, 'state': model.state_dict()}
    if opt is not None:
        d['opt'] = opt.state_dict()
    if extra:
        d.update(extra)
    tmp = path + '.tmp'
    torch.save(d, tmp)
    os.replace(tmp, path)


def load_model(path, device):
    d = torch.load(path, map_location=device, weights_only=False)
    m = make_model(d['cfg']['channels'], d['cfg']['blocks'], device)
    m.load_state_dict(d['state'])
    return m, d


def maybe_compile(model, on, log):
    if not on:
        return model
    try:
        return torch.compile(model, dynamic=False)
    except Exception as e:  # pragma: no cover
        log(f'torch.compile unavailable ({e}); running eagerly')
        return model


class Trainer:
    def __init__(self, model, lr, wd, batch, fut_w, fwd=None):
        self.model = model
        self.fwd = model if fwd is None else fwd
        self.opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=wd)
        self.batch, self.fut_w = batch, fut_w
        self.acc = torch.zeros(3, device=next(model.parameters()).device)
        self.nacc = 0
        self.steps = 0

    def set_lr(self, lr):
        for g in self.opt.param_groups:
            g['lr'] = lr

    def step(self, rec):
        m = self.model
        m.train()
        d = unpack(rec)
        with torch.autocast('cuda', dtype=torch.bfloat16):
            x = featurize(d['alt'], d['vis'], d['pos'], d['T']).to(memory_format=torch.channels_last)
            fl, fu = self.fwd(x)
            loss, lf, lu = loss_fn(fl, fu, d['T'], d['label'], self.fut_w)
        self.opt.zero_grad(set_to_none=True)
        loss.backward()
        torch.nn.utils.clip_grad_norm_(m.parameters(), 1.0)
        self.opt.step()
        self.acc += torch.stack([loss.detach(), lf, lu])
        self.nacc += 1
        self.steps += 1

    def pop_losses(self):
        if not self.nacc:
            return 0, 0, 0
        v = (self.acc / self.nacc).tolist()
        self.acc.zero_()
        self.nacc = 0
        return v


@torch.inference_mode()
def predict(model, rec, gamma=1.0, depth_bonus=0.0, chunk=16384):
    """rec: (n,204) uint8 tensor on any device -> (search value, p_fin, future minutes) as GPU tensors."""
    model.eval()
    dev = next(model.parameters()).device
    vs, ps, fs = [], [], []
    for s in range(0, len(rec), chunk):
        r = rec[s:s + chunk].to(dev, non_blocking=True)
        m = len(r)
        P = next(b for b in (256, 1024, 4096, 8192, 16384, 1 << 30) if b >= m)
        P = min(P, chunk) if m <= chunk else m
        if P > m:
            r = torch.cat([r, r[:1].expand(P - m, -1)])
        d = unpack(r)
        with torch.autocast('cuda', dtype=torch.bfloat16):
            x = featurize(d['alt'], d['vis'], d['pos'], d['T']).to(memory_format=torch.channels_last)
            fl, fu = model(x)
        v = search_value(fl, fu, d['T'], gamma) + depth_bonus * d['depth'].float()
        vs.append(v[:m].float())
        ps.append(torch.sigmoid(fl[:m].float()))
        fs.append(1000.0 * fu[:m].float())
    if not vs:
        z = torch.zeros(0, device=dev)
        return z, z, z
    return torch.cat(vs), torch.cat(ps), torch.cat(fs)


def val_metrics(model, val_rec):
    if val_rec is None or len(val_rec) == 0:
        return {}
    _, p, fut = predict(model, val_rec)
    d = unpack(val_rec)
    lab, T = d['label'].float(), d['T'].float()
    fin = lab > 0
    acc = ((p > 0.5) == fin).float().mean().item()
    out = {'acc': acc, 'fin': fin.float().mean().item()}
    if fin.sum() > 10:
        tgt = (lab - T).clamp(min=0)[fin]
        pr = fut.clamp(min=0)[fin]
        out['mae'] = (pr - tgt).abs().mean().item()
        out['bias'] = (pr - tgt).mean().item()
        tot_p, tot_l = (T[fin] + pr), lab[fin]
        out['corr'] = torch.corrcoef(torch.stack([tot_p, tot_l]))[0, 1].item()
    return out


def fmt_metrics(m):
    if not m:
        return '-'
    s = f"acc {m['acc']:.3f}"
    if 'mae' in m:
        s += f" mae {m['mae']:.0f} bias {m['bias']:+.0f} corr {m['corr']:.3f}"
    return s


# ------------------------------------------------------------------------------------------------ train
def cmd_train(a):
    log = Log(a.run)
    dev = 'cuda'
    torch.backends.cudnn.benchmark = True
    log(f'== train: {a.train_minutes} min, batch {a.batch}, lr {a.lr}')
    base, val = load_base(a, log, dev)
    val_rec = torch.from_numpy(val.view(np.uint8).reshape(-1, REC)).to(dev)
    mp = os.path.join(a.run, 'model.pt')
    if a.resume_model and os.path.exists(mp):
        model, _ = load_model(mp, dev)
        log(f'resumed {mp}')
    else:
        model = make_model(a.channels, a.blocks, dev)
    log(f'network: {a.channels} channels x {a.blocks} blocks, {n_params(model) / 1e6:.2f}M parameters')
    tr = Trainer(model, a.lr, a.wd, a.batch, a.fut_w)  # eager: compiled training gave NaN gradients here
    t0 = time.time()
    total = a.train_minutes * 60
    last = t0
    while not STOP:
        el = time.time() - t0
        if el >= total:
            break
        lr = a.lr * (0.05 + 0.95 * 0.5 * (1 + math.cos(math.pi * el / total)))
        tr.set_lr(lr)
        tr.step(base.sample(a.batch))
        if time.time() - last >= 60:
            l, lf, lu = tr.pop_losses()
            seen = tr.steps * a.batch
            log(f'step {tr.steps} ({seen / base.n:.2f} epochs) lr {lr:.2e} loss {l:.4f} (fin {lf:.4f} fut {lu:.4f}) | '
                f'val {fmt_metrics(val_metrics(model, val_rec))}')
            last = time.time()
    save_model(mp, model, tr.opt)
    save_model(os.path.join(a.run, 'model_init.pt'), model)
    log(f'train done: {tr.steps} steps; val {fmt_metrics(val_metrics(model, val_rec))}; saved {mp}')


# ------------------------------------------------------------------------------------------------ search
def verify(text):
    from check_tour import check
    try:
        ok, T, mv, bad = check(text, show=False)
        return bool(ok) and mv <= 3 and not bad, int(T)
    except Exception as e:  # pragma: no cover
        return False, str(e)


def cmd_search(a):
    global STOP
    log = Log(a.run)
    dev = 'cuda'
    torch.backends.cudnn.benchmark = True
    torch.set_num_threads(2)
    rec_best = record_minutes()
    log(f'== search: {a.hours} h; batch {a.pop_batch} pops, {a.workers} worker threads, {a.expand_threads} expand threads; '
        f'record to beat {rec_best}')
    setup_engine(a, log)
    K.label_init(a.label_log2)
    mp = os.path.join(a.run, 'model.pt')
    if not os.path.exists(mp):
        raise SystemExit(f'{mp} not found: run the train step first')
    train_model, ck = load_model(mp, dev)
    eval_model = copy.deepcopy(train_model).eval()
    eval_fwd = maybe_compile(eval_model, a.compile, log)
    tr = Trainer(train_model, a.lr_online, a.wd, a.batch, a.fut_w)  # eager (compiled training gave NaN gradients)
    if 'opt' in ck:
        try:
            tr.opt.load_state_dict(ck['opt'])
        except Exception:
            pass
    tr.set_lr(a.lr_online)
    log(f'network {ck["cfg"]}, {n_params(train_model) / 1e6:.2f}M parameters')

    base, val = load_base(a, log, dev, merge_labels=True)
    val_rec = torch.from_numpy(val.view(np.uint8).reshape(-1, REC)).to(dev)
    fresh = Replay(a.fresh_cap, dev)
    fval = Replay(200_000, dev)
    data = os.path.join(a.run, 'data')
    old = load_shards(os.path.join(data, 'search_*.npy'), a.fresh_cap // 2)
    if len(old):
        fresh.add(old)
        log(f'fresh replay: {len(old) / 1e6:.2f}M samples from earlier searches')
    sw = ShardWriter(data, 'search', max_shards=a.keep_search_shards)

    prefix_file = a.anchor_tour or ''
    if prefix_file and not os.path.isabs(prefix_file):
        prefix_file = os.path.join(ROOT, prefix_file)
    astar = K.AStar(prefix_file, a.anchor_len if prefix_file else 0, a.max_nodes, a.max_open, a.dedup_log2)
    workers = K.Workers(a.workers, a.seed + 1)
    cap = 24 * a.pop_batch + 64
    bufs = [torch.empty((cap, REC), dtype=torch.uint8).pin_memory() for _ in range(2)]
    ex = ThreadPoolExecutor(1)

    pool_path = os.path.join(a.run, 'pool.txt')
    best_path = os.path.join(a.run, 'best.txt')
    best_score = -1
    t0 = time.time()
    t_inf = t_train = 0.0
    last_rep = last_sync = last_drain = t0
    prev_ws, prev_as = {}, {}
    slot = 0
    n_cur = astar.expand(slot, a.pop_batch, a.expand_threads, bufs[slot].numpy())
    restarts = 0
    last_improve = last_restart = t0
    pending = None
    tour_file = os.path.join(a.run, 'found_tmp.txt')

    def drain_data():
        d = [workers.drain(), astar.tree_samples()]
        if a.harvest > 0:
            d.append(astar.harvest(a.harvest, a.harvest * 20))
        d = np.concatenate(d)
        if len(d) == 0:
            return 0
        _, d = K.label_merge(d)
        sw.add(d)
        m = np.zeros(len(d), bool)
        m[::50] = True
        fval.add(d[m])
        fresh.add(d[~m])
        return len(d)

    try:
        while not STOP and time.time() - t0 < a.hours * 3600:
            # expand the next batch on a CPU thread while the GPU scores the current one
            nxt = 1 - slot
            pending = ex.submit(astar.expand, nxt, a.pop_batch, a.expand_threads, bufs[nxt].numpy())
            ti = time.time()
            if n_cur > 0:
                v, _, _ = predict(eval_fwd, bufs[slot][:n_cur], a.gamma, a.depth_bonus)
                f = v.cpu().numpy()
            else:
                f = np.zeros(0, np.float32)
            n_next = pending.result()
            pending = None
            astar.push(slot, f)
            t_inf += time.time() - ti
            slot, n_cur = nxt, n_next

            if n_cur == 0:
                st = astar.stats()
                if st['open'] == 0:
                    restarts += 1
                    log(f'open list empty: restarting the search (restart {restarts})')
                    astar = K.AStar(prefix_file, a.anchor_len if prefix_file else 0, a.max_nodes, a.max_open, a.dedup_log2)
                    slot = 0
                    n_cur = astar.expand(slot, a.pop_batch, a.expand_threads, bufs[slot].numpy())
                    continue

            # finished tours: all go to pool.txt; new bests are checked exactly and seed future worker jobs
            new_seeds = []
            for sc, text in astar.found():
                with open(pool_path, 'a') as fp:
                    fp.write(f'{sc}\n{text}\n')
                if sc <= best_score:
                    continue
                ok, T = verify(text)
                if not ok or T != sc:
                    log(f'WARNING: tour of {sc} failed the exact check ({T})')
                    continue
                best_score = sc
                last_improve = time.time()
                new_seeds.append(text)
                with open(best_path, 'w') as fp:
                    fp.write(f'{sc}\n{text}\n')
                log(f'new best tour: {sc} minutes (verified) -> {os.path.relpath(best_path, ROOT)}')
                if sc > rec_best:
                    rp = os.path.join(a.run, f'NEW_RECORD_{sc}.txt')
                    with open(rp, 'w') as fp:
                        fp.write(f'{sc}\n{text}\n')
                    log(f'*** NEW RECORD {sc} > {rec_best} *** saved to {os.path.relpath(rp, ROOT)}')
            if new_seeds:
                with open(tour_file, 'w') as fp:
                    fp.write(new_seeds[-1] + '\n')
                K.load_tours(tour_file, a.found_weight)  # future seed jobs also start from the new best

            # restart the tree when it has stopped improving (the network keeps what it learned)
            if (a.restart_stale_minutes > 0 and time.time() - max(last_improve, last_restart) > a.restart_stale_minutes * 60):
                restarts += 1
                log(f'no new best for {a.restart_stale_minutes:.0f} min: restarting the search tree (restart {restarts})')
                astar = K.AStar(prefix_file, a.anchor_len if prefix_file else 0, a.max_nodes, a.max_open, a.dedup_log2)
                last_restart = time.time()
                slot = 0
                n_cur = astar.expand(slot, a.pop_batch, a.expand_threads, bufs[slot].numpy())
                continue

            now = time.time()
            if now - last_drain >= 2:
                drain_data()
                last_drain = now

            # online training: keep training at about train_frac of the GPU time
            if fresh.n >= a.min_fresh:
                tt = time.time()
                target = a.train_frac / (1 - a.train_frac) * t_inf
                while t_train + (time.time() - tt) < target and not STOP:
                    nf = int(a.batch * a.fresh_frac)
                    rec = torch.cat([fresh.sample(nf), base.sample(a.batch - nf)])
                    tr.step(rec)
                    if time.time() - tt > 2.0:
                        break
                t_train += time.time() - tt

            # switch the search to the new weights and re-score the best part of the open list
            if now - last_sync >= a.sync_minutes * 60 and tr.steps > 0:
                ts = time.time()
                if not all(torch.isfinite(p).all() for p in train_model.parameters()):
                    log('WARNING: training produced non-finite weights; reverting to the search network')
                    train_model.load_state_dict(eval_model.state_dict())
                    tr = Trainer(train_model, a.lr_online, a.wd, a.batch, a.fut_w)
                    last_sync = time.time()
                    continue
                eval_model.load_state_dict(train_model.state_dict())
                eval_model.eval()
                rec = astar.rescore_export(a.rescore, a.expand_threads)
                shift = 0.0
                if len(rec):
                    rt = torch.from_numpy(rec.view(np.uint8).reshape(-1, REC))
                    v, _, _ = predict(eval_fwd, rt, a.gamma, a.depth_bonus)
                    shift = astar.rescore_import(v.cpu().numpy())
                save_model(mp, train_model, tr.opt)
                log(f'network updated ({tr.steps} online steps); re-scored {len(rec) / 1e6:.2f}M open nodes in '
                    f'{time.time() - ts:.1f}s (others shifted {shift:+.0f}); saved {os.path.relpath(mp, ROOT)}')
                last_sync = time.time()

            if now - last_rep >= a.report_seconds:
                dt = now - last_rep
                st, ws = astar.stats(), workers.stats()
                l, lf, lu = tr.pop_losses()
                de = st['expansions'] - prev_as.get('expansions', 0)
                log(f"A* {de / dt / 1e3:.1f}k exp/s | since best {(now - last_improve) / 60:.0f} min | open {st['open'] / 1e6:.2f}M nodes {st['nodes'] / 1e6:.1f}M "
                    f"maxdepth {st['maxdepth']:.0f} | best {st['best']:.0f} top f {st['top_f']:.0f} | finishes {st['finishes']:.0f} "
                    f"dead {st['dead']:.0f} dup {st['dup']:.0f} stale {st['stale']:.0f} gc {st['gc']:.0f} prune {st['prune']:.0f}")
                log(f"   workers: {worker_line(ws, prev_ws, dt)} | label queue {ws['label_queue']:.0f}")
                log(f"   train: {tr.steps} steps, loss {l:.4f} (fin {lf:.4f} fut {lu:.4f}), GPU {t_train / max(1e-9, t_train + t_inf):.0%} "
                    f"| fresh {fresh.n / 1e6:.2f}M | val base {fmt_metrics(val_metrics(train_model, val_rec[:20000]))} "
                    f"| val search {fmt_metrics(val_metrics(train_model, fval.rec[:min(fval.n, 20000)] if fval.n > 1000 else None))}")
                prev_as, prev_ws, last_rep = st, ws, now
    finally:
        STOP = True
        if pending is not None:
            pending.result()
        workers.stop()
        try:
            drain_data()
        except Exception:
            pass
        sw.flush()
        save_model(mp, train_model, tr.opt)
        st = astar.stats()
        bt = astar.best()
        log(f'search done after {time.time() - t0:.0f}s: {st["expansions"]:.0f} expansions, best {st["best"]:.0f}; '
            f'best tour in {os.path.relpath(best_path, ROOT)}; network saved')
        if bt and best_score < 0:
            with open(best_path, 'w') as fp:
                fp.write(f'{int(st["best"])}\n{bt}\n')


# ------------------------------------------------------------------------------------------------ main
def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('cmd', choices=['gen', 'train', 'search', 'all'])
    p.add_argument('--run', default='runs/nnastar', help='run folder (data, network, logs, tours)')
    p.add_argument('--seed', type=int, default=1)
    p.add_argument('--tours', nargs='+', default=['data/tours/record_8002.txt:3', 'data/tours/record_7911.txt', 'data/tours/record_7200.txt', 'data/tours/record_7066.txt',
                                                   'data/tours/tour_6303.txt', 'runs/pool.txt', 'data/tours/curriculum.txt:0.03',
                                                   'data/tours/openings.txt:0.03'],
                   help='seed tours FILE[:weight]; seed jobs pick a tour by weight')
    g = p.add_argument_group('generation (CPU workers)')
    g.add_argument('--gen-minutes', type=float, default=25)
    g.add_argument('--gen-samples', type=float, default=24e6)
    g.add_argument('--gen-threads', type=int, default=28)
    g.add_argument('--p-level0', type=float, default=0.2, help='seed jobs: share finished by one random rollout')
    g.add_argument('--p-level1', type=float, default=0.4, help='... by NRPA level 1 (100 rollouts)')
    g.add_argument('--p-level2', type=float, default=0.39, help='... by NRPA level 2 (10,000 rollouts)')
    g.add_argument('--p-level3', type=float, default=0.01, help='... by NRPA level 3 (1,000,000 rollouts)')
    g.add_argument('--probes', type=int, default=4, help='random probe rollouts per probed position')
    g.add_argument('--probe-states', type=int, default=12, help='probed positions per path')
    g.add_argument('--prefix-rate', type=float, default=0.05, help='share of the kept seed-tour positions emitted')
    g.add_argument('--anchor-pow', type=float, default=1.0, help='cut point = len * U^pow (<1: later cuts)')
    g.add_argument('--nice', type=int, default=10, help='worker thread niceness (keeps the desktop responsive)')
    g = p.add_argument_group('network and training')
    g.add_argument('--channels', type=int, default=128)
    g.add_argument('--blocks', type=int, default=12)
    g.add_argument('--train-minutes', type=float, default=25)
    g.add_argument('--batch', type=int, default=2048)
    g.add_argument('--lr', type=float, default=2e-3)
    g.add_argument('--lr-online', type=float, default=3e-4)
    g.add_argument('--wd', type=float, default=1e-4)
    g.add_argument('--fut-w', type=float, default=1.0, help='weight of the future-minutes loss')
    g.add_argument('--base-cap', type=int, default=16_000_000, help='generated samples kept on the GPU')
    g.add_argument('--tour-repeat', type=int, default=20, help='copies of the known-tour positions in the base data')
    g.add_argument('--tour-min-score', type=int, default=5000, help='known tours at least this long give labelled positions')
    g.add_argument('--resume-model', action='store_true', help='train: continue from the run folder model.pt')
    g = p.add_argument_group('search')
    g.add_argument('--hours', type=float, default=8)
    g.add_argument('--workers', type=int, default=22, help='labelling / generation threads during the search')
    g.add_argument('--expand-threads', type=int, default=8)
    g.add_argument('--pop-batch', type=int, default=4096, help='nodes expanded per network batch')
    g.add_argument('--max-nodes', type=int, default=400_000_000, help='search tree size (12 bytes per node)')
    g.add_argument('--max-open', type=int, default=40_000_000, help='open-list cap (worst entries are dropped)')
    g.add_argument('--dedup-log2', type=int, default=27, help='duplicate table: 2^k slots, 12 bytes each')
    g.add_argument('--label-log2', type=int, default=26, help='best-label table: 2^k slots, 12 bytes each')
    g.add_argument('--gamma', type=float, default=1.0, help='search value = p_finish^gamma * (T + future)')
    g.add_argument('--depth-bonus', type=float, default=0.0, help='minutes added to f per move from a1 (>0 dives deeper)')
    g.add_argument('--anchor-tour', default='', help='start the search after the first --anchor-len moves of this tour')
    g.add_argument('--anchor-len', type=int, default=0)
    g.add_argument('--p-label', type=float, default=0.05, help='share of expanded positions sent to the labelling workers')
    g.add_argument('--frac-label', type=float, default=0.75, help='share of worker jobs spent labelling search positions')
    g.add_argument('--label-level', type=int, default=1, help='NRPA level for labelling search positions')
    g.add_argument('--label-level-shallow', type=int, default=2, help='... for positions before --shallow-depth')
    g.add_argument('--shallow-depth', type=int, default=36)
    g.add_argument('--label-path-rate', type=float, default=0.25, help='share of a labelling path\'s positions also emitted')
    g.add_argument('--harvest', type=int, default=512, help='search-tree positions with a finished descendant sampled per drain')
    g.add_argument('--train-frac', type=float, default=0.3, help='share of GPU time spent training during the search')
    g.add_argument('--fresh-frac', type=float, default=0.7, help='share of each training batch from search data')
    g.add_argument('--fresh-cap', type=int, default=8_000_000)
    g.add_argument('--keep-search-shards', type=int, default=20, help='search-data shards kept on disk (1M samples, 204 MB each)')
    g.add_argument('--min-fresh', type=int, default=50_000)
    g.add_argument('--sync-minutes', type=float, default=10, help='how often the search adopts the trained weights')
    g.add_argument('--rescore', type=int, default=2_000_000, help='open nodes re-scored when the weights change')
    g.add_argument('--pool-min', type=int, default=7500, help='finished tours at least this long go to pool.txt')
    g.add_argument('--found-weight', type=float, default=1.0, help='seed weight of tours found by the search')
    g.add_argument('--report-seconds', type=float, default=30)
    g.add_argument('--restart-stale-minutes', type=float, default=60,
                   help='start a fresh search tree after this long without a new best (0: never)')
    g.add_argument('--no-compile', dest='compile', action='store_false', help='run the network without torch.compile')
    a = p.parse_args()
    a.run = a.run if os.path.isabs(a.run) else os.path.join(ROOT, a.run)
    os.makedirs(a.run, exist_ok=True)
    with open(os.path.join(a.run, f'args_{a.cmd}.json'), 'w') as fp:
        json.dump(vars(a), fp, indent=1)
    signal.signal(signal.SIGINT, _sigint)
    if a.cmd in ('gen', 'all'):
        cmd_gen(a)
    if a.cmd in ('train', 'all') and not STOP:
        cmd_train(a)
    if a.cmd in ('search', 'all') and not STOP:
        cmd_search(a)


if __name__ == '__main__':
    main()
