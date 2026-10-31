"""ctypes wrapper for build/libnnastar.so (src/nnastar.cpp)."""
import ctypes as C
import os

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB_PATH = os.path.join(ROOT, 'build', 'libnnastar.so')

# must match struct Sample in src/nnastar.cpp (packed, 204 bytes)
SAMPLE_DTYPE = np.dtype([('alt', '<i2', (64,)), ('vis', 'u1', (64,)), ('pos', 'u1'), ('src', 'u1'),
                         ('depth', '<u2'), ('T', '<i4'), ('label', '<f4')])
assert SAMPLE_DTYPE.itemsize == 204
SRC_NAMES = ['seed-prefix', 'seed-path', 'astar-state', 'label-path', 'astar-tree', 'known-tour']

_lib = None


def lib():
    global _lib
    if _lib is None:
        if not os.path.exists(LIB_PATH):
            raise SystemExit(f'{LIB_PATH} not found: run `make build/libnnastar.so` in {ROOT}')
        L = C.CDLL(LIB_PATH)
        vp, i64, d, i, u = C.c_void_p, C.c_longlong, C.c_double, C.c_int, C.c_uint
        sig = {
            'km_init': (None, [u]),
            'km_set': (i, [C.c_char_p, d]),
            'km_load_tours': (i, [C.c_char_p, d]),
            'km_num_tours': (i, []),
            'km_tour_samples': (i64, [vp, i64, i64]),
            'km_workers_start': (None, [i, u]),
            'km_workers_stop': (None, []),
            'km_drain': (i64, [vp, i64]),
            'km_worker_stats': (None, [vp]),
            'km_astar_init': (i, [C.c_char_p, i, C.c_ulonglong, C.c_ulonglong, i]),
            'km_astar_expand': (i64, [i, i, i, vp, i64]),
            'km_astar_push': (None, [i, vp, i64]),
            'km_astar_stats': (None, [vp]),
            'km_astar_best': (i, [C.c_char_p, i]),
            'km_astar_found': (i64, [C.c_char_p, i64]),
            'km_astar_tree_samples': (i64, [vp, i64]),
            'km_astar_harvest': (i64, [vp, i64, i]),
            'km_astar_rescore_export': (i64, [i64, i, vp]),
            'km_astar_rescore_import': (d, [vp, i64]),
            'km_tour_minutes': (i64, [C.c_char_p]),
            'km_label_init': (None, [i]),
            'km_label_merge': (i64, [vp, i64]),
        }
        for name, (res, args) in sig.items():
            f = getattr(L, name)
            f.restype = res
            f.argtypes = args
        _lib = L
    return _lib


def ptr(a):
    """Data pointer of a numpy array or a (CPU) torch tensor."""
    if isinstance(a, np.ndarray):
        return a.ctypes.data
    return a.data_ptr()


def init(seed=1):
    lib().km_init(seed)


def set_param(name, value):
    if not lib().km_set(name.encode(), float(value)):
        raise ValueError(f'unknown engine parameter {name}')


def load_tours(path, weight=1.0):
    return lib().km_load_tours(path.encode(), float(weight))


def tour_samples(min_score=0, cap=2_000_000):
    out = np.zeros(cap, SAMPLE_DTYPE)
    n = lib().km_tour_samples(ptr(out), cap, int(min_score))
    return out[:n]


class Workers:
    STAT_NAMES = ['paths', 'paths_fin', 'label_jobs', 'label_fin', 'samples', 'rollouts', 'queued', 'label_queue',
                  'l0', 'l0_fin', 'l1', 'l1_fin', 'l2', 'l2_fin', 'l3', 'l3_fin']

    def __init__(self, threads, seed=1):
        self.threads = threads
        self.buf = np.zeros(1 << 18, SAMPLE_DTYPE)
        lib().km_workers_start(threads, seed)

    def drain(self):
        parts = []
        while True:
            n = lib().km_drain(ptr(self.buf), len(self.buf))
            if n <= 0:
                break
            parts.append(self.buf[:n].copy())
            if n < len(self.buf):
                break
        return np.concatenate(parts) if parts else np.zeros(0, SAMPLE_DTYPE)

    def stats(self):
        o = np.zeros(16)
        lib().km_worker_stats(ptr(o))
        return dict(zip(self.STAT_NAMES, o.tolist()))

    def stop(self):
        lib().km_workers_stop()


class AStar:
    STAT_NAMES = ['expansions', 'children', 'finishes', 'dead', 'dup', 'stale', 'gc', 'prune', 'nodes', 'open', 'best',
                  'maxdepth', 'dedup', 'dedup_clears', 'top_f', 'lazy']

    def __init__(self, prefix_file='', prefix_len=0, max_nodes=400_000_000, max_open=40_000_000, dedup_log2=27):
        r = lib().km_astar_init(prefix_file.encode(), prefix_len, max_nodes, max_open, dedup_log2)
        if r:
            raise RuntimeError(f'km_astar_init failed ({r}): out of memory or bad prefix file')

    def expand(self, slot, batch, threads, out):
        return lib().km_astar_expand(slot, batch, threads, ptr(out), len(out))

    def push(self, slot, f):
        f = np.ascontiguousarray(f, dtype=np.float32)
        lib().km_astar_push(slot, ptr(f), len(f))

    def stats(self):
        o = np.zeros(16)
        lib().km_astar_stats(ptr(o))
        return dict(zip(self.STAT_NAMES, o.tolist()))

    def best(self):
        buf = C.create_string_buffer(1 << 20)
        lib().km_astar_best(buf, len(buf))
        return buf.value.decode()

    def found(self):
        cap = 1 << 20
        while True:
            buf = C.create_string_buffer(cap)
            n = lib().km_astar_found(buf, cap)
            if n < cap:
                break
            cap = n + 1024
        out = []
        for line in buf.value.decode().splitlines():
            if '\t' in line:
                sc, t = line.split('\t', 1)
                out.append((int(sc), t))
        return out

    def tree_samples(self, cap=1 << 20):
        out = np.zeros(cap, SAMPLE_DTYPE)
        n = lib().km_astar_tree_samples(ptr(out), cap)
        return out[:n].copy()

    def harvest(self, k, tries):
        out = np.zeros(k, SAMPLE_DTYPE)
        n = lib().km_astar_harvest(ptr(out), k, tries)
        return out[:n].copy()

    def rescore_export(self, k, threads):
        out = np.zeros(k, SAMPLE_DTYPE)
        n = lib().km_astar_rescore_export(k, threads, ptr(out))
        return out[:n]

    def rescore_import(self, f):
        f = np.ascontiguousarray(f, dtype=np.float32)
        return lib().km_astar_rescore_import(ptr(f), len(f))


def label_init(log2slots=26):
    lib().km_label_init(log2slots)


def label_merge(arr):
    """Raise each sample's label to the best label seen for the same state (in place); returns how many rose."""
    arr = np.ascontiguousarray(arr)
    return lib().km_label_merge(ptr(arr), len(arr)), arr


def tour_minutes(text):
    return lib().km_tour_minutes(text.encode())
