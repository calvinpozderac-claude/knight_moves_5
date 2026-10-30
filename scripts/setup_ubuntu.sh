#!/usr/bin/env bash
# One-time setup on Ubuntu: builds the C++ agent (build/az), creates .venv with NumPy and PyTorch,
# checks that PyTorch can see the GPU, and checks the GPU trainer against the NumPy one.
# Install the NVIDIA driver first (see the README). Usage: bash scripts/setup_ubuntu.sh
set -euo pipefail
cd "$(dirname "$0")/.."

missing=()
for c in g++ make git python3; do command -v "$c" > /dev/null || missing+=("$c"); done
python3 -c 'import ensurepip' 2> /dev/null || missing+=(python3-venv)
if [ ${#missing[@]} -gt 0 ]; then
  echo "Missing: ${missing[*]}"
  echo "Install them with:  sudo apt update && sudo apt install -y build-essential git python3-venv python3-dev"
  exit 1
fi

echo "== Building build/az (the self-play and tree-search program)"
make

echo "== Python environment in .venv (NumPy + PyTorch; PyTorch is a large download)"
[ -d .venv ] || python3 -m venv .venv
.venv/bin/pip install --quiet --upgrade pip
.venv/bin/pip install --quiet numpy torch

echo "== GPU"
if command -v nvidia-smi > /dev/null; then
  nvidia-smi --query-gpu=name,driver_version --format=csv,noheader || true
else
  echo "nvidia-smi not found: the NVIDIA driver isn't installed (see the README)."
fi
GPU_OK=$(.venv/bin/python - <<'EOF'
import torch
ok = torch.cuda.is_available()
print(f'torch {torch.__version__}, CUDA available: {ok}', flush=True)
if ok:
    print(f'GPU: {torch.cuda.get_device_name(0)}, compute capability {torch.cuda.get_device_capability(0)}, '
          f'architectures in this PyTorch build: {" ".join(torch.cuda.get_arch_list())}', flush=True)
    torch.ones(1, device='cuda')  # fails here if the build doesn't support the card
print('GPU_OK' if ok else 'GPU_MISSING')
EOF
) || true
echo "$GPU_OK" | grep -v '^GPU_'

echo "== Checking the PyTorch trainer against the NumPy trainer"
.venv/bin/python tools/parity_check.py --steps 10 | tail -n 2 || echo "Parity check failed: train with --backend numpy instead."

echo
if echo "$GPU_OK" | grep -q GPU_OK; then
  echo "Setup done. To train (GPU for the training step, all CPU cores for self-play):"
  echo "  source .venv/bin/activate"
  echo "  python3 train/az_train.py --run runs/a1_dist1 --start a1 --dist-k 1 --backend torch --device cuda"
else
  echo "Setup done, but PyTorch can't use the GPU. Check that nvidia-smi lists the card with driver 580+"
  echo "(the PyTorch build installed here uses CUDA 13). You can still train on the CPU:"
  echo "  source .venv/bin/activate"
  echo "  python3 train/az_train.py --run runs/a1_dist1 --start a1 --dist-k 1"
fi
