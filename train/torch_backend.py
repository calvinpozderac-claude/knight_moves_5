"""PyTorch trainer for the same network as az_train.Net: same math, same Adam, same weight file.

Used by `az_train.py --backend torch --device cuda`. Self-play still evaluates the network
on the CPU inside build/az, so the GPU only speeds up the training step.
Check that it matches the NumPy trainer with `python3 tools/parity_check.py`.
"""
import os
import numpy as np
import torch


class TorchNet:
    ORDER = ['W1', 'b1', 'W2', 'b2', 'Wp', 'bp', 'wv', 'bv']

    def __init__(self, np_net, device='cuda', lr=1e-3, wd=1e-5):
        if device.startswith('cuda') and not torch.cuda.is_available():
            raise SystemExit('PyTorch cannot see a CUDA GPU; install a CUDA build of PyTorch, '
                             'or use --device cpu / --backend numpy')
        self.dev = torch.device(device)
        self.p = {k: torch.tensor(np_net.p[k], dtype=torch.float32, device=self.dev, requires_grad=True)
                  for k in self.ORDER}
        # L2 weight decay on the weight matrices only, exactly like the NumPy trainer
        self.opt = torch.optim.Adam(
            [{'params': [self.p[k] for k in ('W1', 'W2', 'Wp')], 'weight_decay': wd},
             {'params': [self.p[k] for k in ('b1', 'b2', 'bp', 'wv', 'bv')], 'weight_decay': 0.0}],
            lr=lr, betas=(0.9, 0.999), eps=1e-8)

    def forward(self, X):
        P = self.p
        h1 = torch.relu(X @ P['W1'] + P['b1'])
        h2 = torch.relu(h1 @ P['W2'] + P['b2'])
        return h2 @ P['Wp'] + P['bp'], h2 @ P['wv'] + P['bv'][0]

    def step(self, X, mask, pi, z):
        d = self.dev
        X = torch.from_numpy(X).to(d)
        M = torch.from_numpy(mask).to(d)
        PI = torch.from_numpy(np.ascontiguousarray(pi, dtype=np.float32)).to(d)
        Z = torch.from_numpy(np.ascontiguousarray(z, dtype=np.float32)).to(d)
        lg, v = self.forward(X)
        logp = torch.log_softmax(lg.masked_fill(~M, -1e9), dim=1)
        lp = -(PI * logp).sum(1).mean()
        lv = ((v - Z) ** 2).mean()
        self.opt.zero_grad(set_to_none=True)
        (lp + lv).backward()
        self.opt.step()
        return lp.detach().item(), lv.detach().item()

    def save(self, fn):
        tmp = fn + '.tmp'
        with open(tmp, 'wb') as f:
            for k in self.ORDER:
                f.write(self.p[k].detach().cpu().numpy().astype(np.float32).tobytes())
        os.replace(tmp, fn)

    def load(self, fn):
        a = np.fromfile(fn, np.float32); o = 0
        with torch.no_grad():
            for k in self.ORDER:
                n = self.p[k].numel()
                self.p[k].copy_(torch.from_numpy(a[o:o + n].reshape(tuple(self.p[k].shape))))
                o += n
