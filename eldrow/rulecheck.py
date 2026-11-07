"""Rule-level brute force on small sub-lists, with no state abstraction at all.

For every target T and every ordering of guesses, a guess is allowed only if, for every
earlier guess h, mark(h, guess) == mark(h, T) (the puzzle's hard-mode rule, verify.py's
mark). The longest such sequence is compared with search and exhaust on the same sub-list.

usage: python3 rulecheck.py <search binary> <exhaust binary> [trials]
"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from verify import mark  # noqa: E402

words = open(os.path.join(HERE, "words.txt")).read().split()
search, exhaust = sys.argv[1], sys.argv[2]
trials = int(sys.argv[3]) if len(sys.argv) > 3 else 60


def longest_by_rule(sub):
    best = [0, None]

    def extend(seq, T):
        if len(seq) + 1 > best[0]:
            best[0], best[1] = len(seq) + 1, seq + [T]
        for w in sub:
            if w != T and w not in seq and all(mark(h, w) == mark(h, T) for h in seq):
                extend(seq + [w], T)

    for T in sub:
        extend([], T)
    return best


def solver_optimum(sub):
    d = tempfile.mkdtemp()
    wf, fb = os.path.join(d, "w.txt"), os.path.join(d, "fb.bin")
    open(wf, "w").write("\n".join(sub) + "\n")
    out = subprocess.run([search, wf, "2", "26"], capture_output=True, text=True).stdout
    s_opt = int(out.split("optimum = ")[1].split()[0])
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "fbmatrix.py"), wf, fb],
                   capture_output=True, check=True)
    e_opt = None
    for K in range(1, len(sub) + 2):
        r = subprocess.run([exhaust, wf, fb, str(K), "none", "1"], capture_output=True, text=True)
        if r.returncode != 0:
            e_opt = K - 1
            break
    return s_opt, e_opt


random.seed(2026)
families = ["?OWER", "?IGHT", "?ATCH", "?OUND", "?ASTE", "?ATER", "?ILLY", "?ONEY"]
for trial in range(trials):
    if trial % 2 == 0:
        fam = random.choice(families)
        core = [w for w in words if all(a == "?" or a == b for a, b in zip(fam, w))]
        extra = random.sample(words, random.randint(2, 5))
        sub = sorted(set(random.sample(core, min(len(core), random.randint(4, 8))) + extra))
        label = fam
    else:
        suffix = random.choice(["ER", "E", "Y", "T", "S", "H", "D"])
        pool = [w for w in words if w.endswith(suffix)]
        sub = sorted(random.sample(pool, random.randint(8, 12)))
        label = "*" + suffix
    by_rule, chain = longest_by_rule(sub)
    s_opt, e_opt = solver_optimum(sub)
    ok = by_rule == s_opt == e_opt
    print(f"{trial:2d} {label:6s} {len(sub):2d} words: rule {by_rule} search {s_opt} exhaust {e_opt}"
          f"  {'OK' if ok else 'MISMATCH'}  {','.join(chain)}", flush=True)
    assert ok
print("all agree")
