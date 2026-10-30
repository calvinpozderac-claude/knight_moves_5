"""PyTorch trainer for the same network as az_train.Net: same math, same Adam, same weight file.

Used by `az_train.py --backend torch --device cuda`. Self-play still evaluates the network
on the CPU inside build/az, so the GPU only speeds up the training step.
Check that it matches the NumPy trainer with `python3 tools/parity_check.py`.
"""
import os
import numpy as np
import torch


class TorchNet:

    def __init__(self, np_net, device='cuda', lr=1e-3, wd=1e-5):
        if device.startswith('cuda') and not torch.cuda.is_available():
            raise SystemExit('PyTorch cannot see a CUDA GPU; install a CUDA build of PyTorch, '
                             'or use --device cpu / --backend numpy')
        self.dev = torch.device(device)
        self.ORDER, self.L = list(np_net.ORDER), np_net.L
        self.p = {k: torch.tensor(np_net.p[k], dtype=torch.float32, device=self.dev, requires_grad=True)
                  for k in self.ORDER}
        # L2 weight decay on the weight matrices only, exactly like the NumPy trainer
        self.opt = torch.optim.Adam(
            [{'params': [self.p[k] for k in self.ORDER if k.startswith('W')], 'weight_decay': wd},
             {'params': [self.p[k] for k in self.ORDER if not k.startswith('W')], 'weight_decay': 0.0}],
            lr=lr, betas=(0.9, 0.999), eps=1e-8)

    def forward(self, X):
        P = self.p; h = X
        for l in range(1, self.L + 1):
            h = torch.relu(h @ P[f'W{l}'] + P[f'b{l}'])
        return h @ P['Wp'] + P['bp'], h @ P['wv'] + P['bv'][0]

    @torch.no_grad()
    def infer(self, idx, g):
        """Batched evaluation for self-play (see az_train.Net.infer). Runs on self.dev."""
        d, P = self.dev, self.p
        I = torch.from_numpy(np.array(idx, dtype=np.int64)).to(d); G = torch.from_numpy(np.array(g, dtype=np.float32)).to(d)
        h = torch.relu(torch.nn.functional.embedding_bag(I, P['W1'], mode='sum') + G @ P['W1'][64 * 21 + 64:] + P['b1'])
        for l in range(2, self.L + 1):
            h = torch.relu(h @ P[f'W{l}'] + P[f'b{l}'])
        return torch.cat([h @ P['Wp'] + P['bp'], (h @ P['wv'] + P['bv'][0]).unsqueeze(1)], 1).cpu().numpy()

    @torch.no_grad()
    def loss(self, X, mask, pi, z):
        d = self.dev
        lg, v = self.forward(torch.from_numpy(X).to(d))
        logp = torch.log_softmax(lg.masked_fill(~torch.from_numpy(mask).to(d), -1e9), dim=1)
        PI = torch.from_numpy(np.array(pi, dtype=np.float32)).to(d); Z = torch.from_numpy(np.array(z, dtype=np.float32)).to(d)
        return float(-(PI * logp).sum(1).mean()), float(((v - Z) ** 2).mean())

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
