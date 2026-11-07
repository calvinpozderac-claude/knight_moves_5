"""Cross-checks the branch-and-bound search against a plain exhaustive search on small
random sub-lists of the word list (no pruning bound, just the definition of f).

usage: python3 crosscheck.py <path to built search binary> [trials]
"""
import functools
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from verify import mark  # noqa: E402

words = open(os.path.join(HERE, "words.txt")).read().split()
search = sys.argv[1]
trials = int(sys.argv[2]) if len(sys.argv) > 2 else 40
random.seed(7)
for trial in range(trials):
    suffix = random.choice(["ER", "E", "Y", "T", "S", "H", "D"])
    pool = [w for w in words if w.endswith(suffix)]
    sub = sorted(random.sample(pool, min(len(pool), random.randint(15, 45))))

    @functools.lru_cache(None)
    def f(S):
        best = 1
        for w in S:
            classes = {}
            for x in S:
                if x != w:
                    classes.setdefault(mark(w, x), []).append(x)
            for c in classes.values():
                best = max(best, 1 + f(tuple(c)))
        return best

    naive = f(tuple(sub))
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
        tmp.write("\n".join(sub) + "\n")
    out = subprocess.run([search, tmp.name, "2", "26"], capture_output=True, text=True).stdout
    os.unlink(tmp.name)
    got = int(out.split("optimum = ")[1].split()[0])
    print(trial, f"*{suffix}", len(sub), "words: brute force", naive, "search", got,
          "OK" if naive == got else "MISMATCH")
    assert naive == got
print("all agree")
