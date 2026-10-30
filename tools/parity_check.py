"""Check that the PyTorch trainer matches the NumPy trainer step for step.

    python3 tools/parity_check.py            # uses CUDA if available, else CPU
    python3 tools/parity_check.py --device cpu
"""
import argparse, os, sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'train'))
from az_train import Net, featurize, NA  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--device', default=None)
ap.add_argument('--steps', type=int, default=20)
args = ap.parse_args()

import torch  # noqa: E402
from torch_backend import TorchNet  # noqa: E402

dev = args.device or ('cuda' if torch.cuda.is_available() else 'cpu')
rng = np.random.default_rng(1)
B = 256
alt = rng.integers(-30, 20, (B, 64)).astype(np.int16)
vis = rng.integers(0, 4, (B, 64)).astype(np.uint8)
pos = rng.integers(0, 64, B)
T = rng.integers(0, 6000, B).astype(np.float32)
X = featurize(alt, vis, pos, T)
mask = rng.random((B, NA)) < 0.4
mask[np.arange(B), rng.integers(0, NA, B)] = True
pi = (rng.random((B, NA)) * mask).astype(np.float32)
pi /= pi.sum(1, keepdims=True)
z = (rng.standard_normal(B) * 0.1).astype(np.float32)

a = Net(np.random.default_rng(0))
b = TorchNet(Net(np.random.default_rng(0)), device=dev)
worst = 0.0
for i in range(args.steps):
    la, lb = a.step(X, mask, pi, z), b.step(X, mask, pi, z)
    rel = max(abs(la[0] - lb[0]) / abs(la[0]), abs(la[1] - lb[1]) / max(abs(la[1]), 1e-8))
    worst = max(worst, rel)
    if i < 3 or i == args.steps - 1:
        print(f'step {i}: numpy loss {la[0]:.6f} {la[1]:.6f} | torch loss {lb[0]:.6f} {lb[1]:.6f}')

# compare predictions after training
P = a.p
h2 = np.maximum(np.maximum(X @ P['W1'] + P['b1'], 0) @ P['W2'] + P['b2'], 0)
lg_np, v_np = h2 @ P['Wp'] + P['bp'], h2 @ P['wv'] + P['bv'][0]
with torch.no_grad():
    lg_t, v_t = b.forward(torch.from_numpy(X).to(b.dev))
d_lg = np.abs(lg_np - lg_t.cpu().numpy()).max()
d_v = np.abs(v_np - v_t.cpu().numpy()).max()
print(f'device {dev}: worst relative loss difference {worst:.2e}; '
      f'max difference after {args.steps} steps: logits {d_lg:.2e}, value {d_v:.2e}')
ok = worst < 1e-3 and d_lg < 1e-2 and d_v < 1e-2
print('PARITY OK' if ok else 'PARITY MISMATCH')
sys.exit(0 if ok else 1)
