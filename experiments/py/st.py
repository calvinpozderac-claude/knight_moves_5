"""State helpers for planning tours with integer altitudes."""
from collections import deque
from core import *


class State:
    __slots__ = ('p', 'alt', 'vis', 'T', 'moves')

    def __init__(self, p=A1, alt=None, vis=None, T=0, moves=None):
        self.p = p
        self.alt = list(ALT0) if alt is None else list(alt)
        if vis is None:
            vis = [0] * 64
            vis[p] = 1
        self.vis = list(vis)
        self.T = T
        self.moves = [] if moves is None else list(moves)

    def copy(self):
        return State(self.p, self.alt, self.vis, self.T, self.moves)

    def step(self, delta, q, check=True):
        """Wait so the knight's group sinks by delta, then jump to q."""
        a2, n, t = apply(self.p, delta, self.alt)
        if check:
            assert legal(self.p, q, a2), (nm(self.p), nm(q), a2[q] - a2[self.p])
            if q != H8:
                assert self.vis[q] < 3, nm(q)
        s = self.copy()
        s.alt = a2
        s.vis[q] += 1
        s.T += t
        s.moves.append((t, nm(q)))
        s.p = q
        return s

    def group(self):
        return group_of(self.p, self.alt)

    def show(self):
        for y in range(7, -1, -1):
            row = []
            for x in range(8):
                i = x + 8 * y
                mark = '*' if i == self.p else ' '
                row.append(f'{self.alt[i]:4d}{mark}{self.vis[i]}')
            print(y + 1, ' '.join(row))
        print('   ' + ''.join(f'{c:>7s}' for c in 'abcdefgh'))
        g, rise = self.group()
        print('T=', self.T, 'pos', nm(self.p), 'alt', self.alt[self.p], 'n=', len(g),
              'opp', nm(63 - self.p), self.alt[63 - self.p], 'rise' if rise else 'excluded')

    def options(self, maxdelta=None):
        out = []
        for delta, q in moves_pinned(self.p, self.alt, self.vis, maxdelta=maxdelta):
            g, rise = self.group()
            out.append((delta * len(g), delta, nm(q), self.alt[q]))
        return sorted(out, reverse=True)


def static_path(s, goal, avoid=(H8,)):
    """Zero-wait BFS from s.p to goal index; returns list of indices or None."""
    prev = {s.p: None}
    dq = deque([s.p])
    while dq:
        u = dq.popleft()
        if u == goal:
            out = []
            while u is not None:
                out.append(u)
                u = prev[u]
            return out[::-1]
        for q, dz in NEIGH[u]:
            if q in prev or q in avoid or s.vis[q] >= 3:
                continue
            if (s.alt[q] - s.alt[u]) in dz:
                prev[q] = u
                dq.append(q)
    return None


def walk(s, path):
    for q in path[1:]:
        s = s.step(0, q)
    return s


def parse_moves(text):
    import re
    return [(int(a), b) for a, b in re.findall(r'\((\d+)\s*,\s*([a-h][1-8])\)', text)]


def fmt(moves):
    return ', '.join(f'({t}, {q})' for t, q in moves)
