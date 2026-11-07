#!/usr/bin/env python3
"""Run build/as2 sa under several settings and summarise the per-restart results.

    python3 sweep.py LABEL [as2 sa options ...]      run one setting, log + summarise it
    python3 sweep.py --summary [LABEL ...]            re-print summaries from results/logs/

Each setting's restarts are logged to results/logs/<LABEL>.csv (one line per restart:
grid, score, states, objective, citations) and a summary row is appended to
results/sweep_summary.csv.
"""
import os
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LOGS = os.path.join(HERE, "results", "logs")


def load(label):
    rows = []
    with open(os.path.join(LOGS, label + ".csv")) as f:
        for line in f:
            p = line.rstrip("\n").split(",")
            rows.append(dict(grid=p[0], score=int(p[1]), n=int(p[2]), obj=float(p[3]), cites=p[4]))
    return rows


def summary(label, secs=None):
    rows = load(label)
    s = [r["score"] / 1e6 for r in rows]
    best = max(rows, key=lambda r: r["obj"])
    line = (f"{label},{len(s)},{statistics.mean(s):.2f},{statistics.median(s):.2f},"
            f"{statistics.pstdev(s):.2f},{max(s):.3f},{best['grid']},{'' if secs is None else f'{secs:.0f}'}")
    return line


def main():
    os.makedirs(LOGS, exist_ok=True)
    if sys.argv[1] == "--summary":
        labels = sys.argv[2:] or sorted(f[:-4] for f in os.listdir(LOGS) if f.endswith(".csv"))
        print("label,restarts,mean_M,median_M,sd_M,max_M,best_grid,seconds")
        for l in labels:
            print(summary(l))
        return
    label, args = sys.argv[1], sys.argv[2:]
    log = os.path.join(LOGS, label + ".csv")
    if os.path.exists(log):
        os.remove(log)
    t = time.time()
    mode = "pt" if "--replicas" in args else "sa"
    subprocess.run([os.path.join(HERE, "build", "as2"), mode, *args, "--log", log],
                   cwd=HERE, check=True, stdout=subprocess.DEVNULL)
    secs = time.time() - t
    line = summary(label, secs)
    out = os.path.join(HERE, "results", "sweep_summary.csv")
    new = not os.path.exists(out)
    with open(out, "a") as f:
        if new:
            f.write("label,restarts,mean_M,median_M,sd_M,max_M,best_grid,seconds,args\n")
        f.write(line + "," + " ".join(args) + "\n")
    print(line, flush=True)


if __name__ == "__main__":
    main()
