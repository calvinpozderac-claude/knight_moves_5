"""Replay buffers of packed samples kept on the GPU, and data helpers."""
import glob
import os

import numpy as np
import torch

from .lib import SAMPLE_DTYPE
from .model import REC


class Replay:
    """Ring buffer of packed 204-byte samples on the GPU."""

    def __init__(self, cap, device):
        self.cap = int(cap)
        self.rec = torch.empty((self.cap, REC), dtype=torch.uint8, device=device)
        self.n = 0
        self.ptr = 0
        self.added = 0
        self.device = device

    def add(self, arr):
        if len(arr) == 0:
            return
        b = torch.from_numpy(np.ascontiguousarray(arr).view(np.uint8).reshape(-1, REC))
        if len(b) > self.cap:
            b = b[-self.cap:]
        k = len(b)
        e = self.ptr + k
        if e <= self.cap:
            self.rec[self.ptr:e].copy_(b)
        else:
            m = self.cap - self.ptr
            self.rec[self.ptr:].copy_(b[:m])
            self.rec[:k - m].copy_(b[m:])
        self.ptr = e % self.cap
        self.n = min(self.cap, self.n + k)
        self.added += k

    def sample(self, B):
        idx = torch.randint(0, self.n, (B,), device=self.device)
        return self.rec[idx]


def row_hash(arr, chunk=1 << 20):
    """64-bit hash of (alt, vis, pos, T) per sample."""
    rng = np.random.default_rng(12345)
    R1 = rng.integers(1, 2 ** 63, 64, dtype=np.uint64) | np.uint64(1)
    R2 = rng.integers(1, 2 ** 63, 64, dtype=np.uint64) | np.uint64(1)
    R3, R4 = (rng.integers(1, 2 ** 63, 2, dtype=np.uint64) | np.uint64(1))
    out = np.empty(len(arr), np.uint64)
    with np.errstate(over='ignore'):
        for s in range(0, len(arr), chunk):
            a = arr[s:s + chunk]
            h = (a['alt'].astype(np.int64).view(np.uint64) * R1).sum(1, dtype=np.uint64)
            h ^= (a['vis'].astype(np.uint64) * R2).sum(1, dtype=np.uint64)
            h ^= a['pos'].astype(np.uint64) * R3
            h ^= a['T'].astype(np.int64).view(np.uint64) * R4
            out[s:s + chunk] = h
    return out


def dedup_max(arr):
    """Keep one copy per (state, T), with the best label among its copies."""
    if len(arr) == 0:
        return arr
    h = row_hash(arr)
    order = np.lexsort((-arr['label'], h))  # by hash, best label first
    hs = h[order]
    first = np.ones(len(hs), bool)
    first[1:] = hs[1:] != hs[:-1]
    return arr[order[first]]


def load_shards(pattern, cap, newest_first=True, extra=None):
    """Load up to cap samples from .npy shards (newest first) into one array, plus `extra` samples."""
    files = sorted(glob.glob(pattern))
    if newest_first:
        files = files[::-1]
    sizes, n = [], 0
    for f in files:
        k = min(len(np.load(f, mmap_mode='r')), cap - n)
        if k <= 0:
            break
        sizes.append((f, k))
        n += k
    ne = 0 if extra is None else len(extra)
    out = np.empty(n + ne, SAMPLE_DTYPE)
    o = 0
    for f, k in sizes:
        out[o:o + k] = np.load(f, mmap_mode='r')[:k]
        o += k
    if ne:
        out[o:] = extra
    return out


class ShardWriter:
    """Writes samples to numbered .npy shards; with max_shards, only the newest shards are kept."""

    def __init__(self, folder, prefix, shard_size=1_000_000, max_shards=0):
        os.makedirs(folder, exist_ok=True)
        self.folder, self.prefix, self.size, self.max_shards = folder, prefix, shard_size, max_shards
        self.buf, self.n = [], 0
        existing = glob.glob(os.path.join(folder, f'{prefix}_*.npy'))
        self.idx = 1 + max([int(os.path.basename(f)[len(prefix) + 1:-4]) for f in existing], default=-1)
        self.written = 0

    def add(self, arr):
        if len(arr):
            self.buf.append(arr)
            self.n += len(arr)
        if self.n >= self.size:
            self.flush()

    def flush(self):
        if not self.n:
            return
        a = np.concatenate(self.buf)
        np.save(os.path.join(self.folder, f'{self.prefix}_{self.idx:05d}.npy'), a)
        self.idx += 1
        if self.max_shards > 0:
            for f in sorted(glob.glob(os.path.join(self.folder, f'{self.prefix}_*.npy')))[:-self.max_shards]:
                os.remove(f)
        self.written += len(a)
        self.buf, self.n = [], 0
