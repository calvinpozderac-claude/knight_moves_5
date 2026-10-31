"""GPU featurizer and the value network (a ResNet over the 8x8 lattice).

The network predicts, for a position, the best final score reachable from it, split into two heads:
  - fin: logit of "a finish is reachable" (label > 0)
  - fut: future minutes (label - T) / 1000, trained only on positions that can finish, on an upper
    quantile of the labels (--fut-quantile) so that uncertain positions are rated by their upside
The search value is  p_fin ** gamma * (T + 1000 * fut).
"""
import math

import torch
import torch.nn as nn
import torch.nn.functional as F

REC = 204  # bytes per packed sample (see SAMPLE_DTYPE in lib.py)
FUT_SCALE = 1000.0
OPP = torch.arange(63, -1, -1)  # index of the diametrically opposite point


def unpack(rec):
    """rec: (B, 204) uint8 tensor (any device) -> dict of tensors."""
    rec = rec.contiguous()
    alt = rec[:, 0:128].contiguous().view(torch.int16)
    vis = rec[:, 128:192]
    pos = rec[:, 192].long()
    src = rec[:, 193]
    depth = (rec[:, 194:196].contiguous().view(torch.int16).squeeze(1).long()) & 0xFFFF
    T = rec[:, 196:200].contiguous().view(torch.int32).squeeze(1)
    label = rec[:, 200:204].contiguous().view(torch.float32).squeeze(1)
    return dict(alt=alt, vis=vis, pos=pos, src=src, depth=depth, T=T, label=label)


N_IN = 52


def featurize(alt, vis, pos, T):
    """alt (B,64) int16, vis (B,64) uint8, pos (B,) long, T (B,) int -> (B, N_IN, 8, 8) float32.

    Board index i = file + 8 * rank (a1 = 0, h8 = 63); planes are laid out as [rank][file].
    """
    B = alt.shape[0]
    dev = alt.device
    a = alt.float()
    ar = torch.arange(B, device=dev)
    ak = a[ar, pos]  # knight's altitude
    rel = a - ak[:, None]  # (B,64)
    reli = rel.clamp(-9, 9).long()
    planes = []
    oh = F.one_hot(reli + 9, 19).float()  # -9 (<= -9) ... +9 (>= 9)
    planes.append(oh)  # 19
    planes.append((rel.clamp(-40, 40) / 40.0)[..., None])  # 1
    same = (a[:, :, None] == a[:, None, :]).float()  # (B,64,64)
    n_alt = same.sum(-1)  # points sharing each point's altitude (itself included)
    planes.append((n_alt / 16.0)[..., None])  # 1
    opp = OPP.to(dev)
    relo = rel[:, opp].clamp(-4, 4).long()  # knight-relative altitude of the opposite point
    planes.append(F.one_hot(relo + 4, 9).float())  # 9
    planes.append(F.one_hot(vis.long().clamp(0, 3), 4).float())  # 4
    kp = F.one_hot(pos, 64).float()
    planes.append(kp[..., None])  # knight
    planes.append(kp[:, opp][..., None])  # point opposite the knight
    idx = torch.arange(64, device=dev)
    const = torch.stack([(idx == 63).float(), (idx == 0).float(), (idx % 8).float() / 7.0, (idx // 8).float() / 7.0], -1)
    planes.append(const[None].expand(B, 64, 4))  # 4: h8, a1, file, rank
    # global features broadcast over the board
    excl_opp = (a[ar, opp[pos]] == ak).float()  # opposite point is in the knight's group (then ignored)
    nk = n_alt[ar, pos] - excl_opp
    cnt = lambda v: (reli == v).float().sum(1)
    rem = (3 - vis.float().clamp(max=3)).sum(1) - (3 - vis[:, 63].float().clamp(max=3))
    g = torch.stack([
        T.float() / 8000.0,
        (a[:, 63] - ak).clamp(-32, 32) / 32.0,
        nk / 32.0,
        cnt(0) / 64.0, cnt(-1) / 64.0, cnt(-2) / 64.0, cnt(1) / 64.0, cnt(2) / 64.0,
        rem / 189.0,
        (a[ar, opp[pos]] - ak).clamp(-32, 32) / 32.0,
        (n_alt.max(1).values) / 64.0,
        (vis.float() >= 3).float().sum(1) / 64.0,
    ], -1)  # 12
    planes.append(g[:, None, :].expand(B, 64, g.shape[1]))
    x = torch.cat(planes, -1)  # (B,64,N_IN)
    assert x.shape[-1] == N_IN, x.shape
    return x.view(B, 8, 8, -1).permute(0, 3, 1, 2).contiguous()


class GlobalPoolBias(nn.Module):
    """KataGo-style global pooling: per-channel mean and max over the board -> bias for the trunk."""

    def __init__(self, c, cg=32):
        super().__init__()
        self.conv = nn.Conv2d(c, cg, 1, bias=False)
        self.bn = nn.BatchNorm2d(cg)
        self.fc = nn.Linear(2 * cg, c)

    def forward(self, x):
        g = F.relu(self.bn(self.conv(x)))
        p = torch.cat([g.mean((2, 3)), g.amax((2, 3))], 1)
        return self.fc(p)[:, :, None, None]


class Block(nn.Module):
    def __init__(self, c, gpool=False):
        super().__init__()
        self.c1 = nn.Conv2d(c, c, 3, padding=1, bias=False)
        self.b1 = nn.BatchNorm2d(c)
        self.c2 = nn.Conv2d(c, c, 3, padding=1, bias=False)
        self.b2 = nn.BatchNorm2d(c)
        self.gp = GlobalPoolBias(c) if gpool else None

    def forward(self, x):
        y = F.relu(self.b1(self.c1(x)))
        if self.gp is not None:
            y = y + self.gp(y)
        y = self.b2(self.c2(y))
        return F.relu(x + y)


class ValueNet(nn.Module):
    def __init__(self, channels=128, blocks=12, gpool_every=3):
        super().__init__()
        self.cfg = dict(channels=channels, blocks=blocks, gpool_every=gpool_every)
        c = channels
        self.stem = nn.Sequential(nn.Conv2d(N_IN, c, 3, padding=1, bias=False), nn.BatchNorm2d(c), nn.ReLU())
        self.blocks = nn.Sequential(*[Block(c, gpool=(i % gpool_every == gpool_every - 1)) for i in range(blocks)])
        self.hconv = nn.Sequential(nn.Conv2d(c, 32, 1, bias=False), nn.BatchNorm2d(32), nn.ReLU())
        self.head = nn.Sequential(nn.Linear(32 * 64 + 2 * c, 512), nn.ReLU(), nn.Linear(512, 256), nn.ReLU(), nn.Linear(256, 2))

    def forward(self, x):
        h = self.blocks(self.stem(x))
        z = torch.cat([self.hconv(h).flatten(1), h.mean((2, 3)), h.amax((2, 3))], 1)
        with torch.autocast('cuda', enabled=False):  # fp32 head: bf16 would round 8000 minutes to steps of ~30
            o = self.head(z.float())
        return o[:, 0], o[:, 1]  # fin logit, future minutes / 1000


def search_value(fin_logit, fut, T, gamma=1.0):
    """Expected best final score: p_fin^gamma * (T + future)."""
    p = torch.sigmoid(fin_logit.float())
    total = T.float() + FUT_SCALE * fut.float().clamp(min=0)
    return p.pow(gamma) * total


def fut_loss(pred, target, tau=0.5, reduction='mean'):
    """Smooth-L1 quantile loss: under-predictions weigh tau, over-predictions 1 - tau (x2, so tau = 0.5 is plain
    smooth L1). tau > 0.5 fits an upper quantile of the labels: an optimistic estimate of the future minutes."""
    l = F.smooth_l1_loss(pred, target, beta=0.5, reduction='none')
    w = torch.where(target > pred, torch.full_like(l, tau), torch.full_like(l, 1.0 - tau))
    l = 2.0 * w * l
    return l.mean() if reduction == 'mean' else l.sum()


def loss_fn(fin_logit, fut, T, label, fut_w=1.0, tau=0.5):
    fin = (label > 0).float()
    l_fin = F.binary_cross_entropy_with_logits(fin_logit.float(), fin)
    target = ((label - T.float()).clamp(min=0) / FUT_SCALE)
    m = fin > 0
    if m.any():
        l_fut = fut_loss(fut.float()[m], target[m], tau)
    else:
        l_fut = fut.sum() * 0
    return l_fin + fut_w * l_fut, l_fin.detach(), l_fut.detach()


def make_model(channels=128, blocks=12, device='cuda'):
    m = ValueNet(channels, blocks).to(device)
    return m.to(memory_format=torch.channels_last)


def n_params(m):
    return sum(p.numel() for p in m.parameters())
