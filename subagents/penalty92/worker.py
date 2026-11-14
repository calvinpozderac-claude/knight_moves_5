# worker: pulls jobs from queue.txt (flock), runs dlns -> sig -> lns_r6, appends results.tsv
# usage: python3 worker.py WID STOP_AFTER_SECONDS
import sys, os, time, fcntl, subprocess, zlib, shutil
import fam
D = os.path.dirname(os.path.abspath(__file__)); os.chdir(D)
WID = sys.argv[1]; STOP = time.time() + float(sys.argv[2])
REF = 'in/record_9275.txt'
DLT = int(os.environ.get('DLT', 120)); LT = int(os.environ.get('LT', 420))
COMMON = ['--xover', '0', '--t0', '40', '--budget', '400000', '--dmax', '8', '--pdmax', '4', '--rmax', '8',
          '--wide', '8', '--skel', '0.5', '--reroute', '0.05', '--free', '0']
def log(msg):
    with open('events.log', 'a') as f: f.write(time.strftime('%H:%M:%S') + f' [w{WID}] {msg}\n')
def pop():
    with open('queue.lock', 'w') as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        L = [l for l in open('queue.txt').read().split('\n') if l.strip() and not l.startswith('#')] if os.path.exists('queue.txt') else []
        if not L: return None
        open('queue.txt', 'w').write('\n'.join(L[1:]) + ('\n' if len(L) > 1 else ''))
        open('taken.txt', 'a').write(L[0] + '\n')
        return L[0]
def known(sig):
    for l in open('known_sigs.txt'):
        if '|' in l and l.split('|', 1)[1].strip() == sig: return l.split('|', 1)[0]
    return None
def addknown(label, sig):
    with open('queue.lock', 'w') as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        if not known(sig): open('known_sigs.txt', 'a').write(f'{label}|{sig}\n')
def row(*a):
    with open('results.tsv', 'a') as f: f.write('\t'.join(str(x) for x in a) + '\n')
def dist(p):
    r = fam.sig_of(REF).split(); s = fam.sig_of(p).split()
    return fam.lev(r, s), len(set(fam.seq_of(REF)) ^ set(fam.seq_of(p)))
def score(p):
    try: return int(open(p).readline())
    except Exception: return None
def run(cmd, logf, tmax):
    with open(logf, 'w') as f:
        subprocess.run(['timeout', str(tmax), 'nice', '-n', '5'] + cmd, stdout=f, stderr=subprocess.STDOUT)
def lns(name, start, donors, seed, t, focus=None):
    d = f'lns/in_{name}'; os.makedirs(d, exist_ok=True); shutil.copy(start, f'{d}/start.txt')
    for x in donors: shutil.copy(x, d)
    cmd = ['bin/lns_r6', '--tours'] + [f'{d}/{x}' for x in sorted(os.listdir(d))] + ['--start', f'{d}/start.txt', '--seed', str(seed)] + COMMON + ['--time', str(t), '--out', f'lns/{name}_best.txt']
    if focus: cmd += ['--focus'] + focus.split(',')
    shutil.copy(start, f'lns/{name}_best.txt')
    run(cmd, f'lns/{name}.log', t + 60)
    return f'lns/{name}_best.txt'
while time.time() < STOP:
    job = pop()
    if job is None: time.sleep(20); continue
    f = job.split('|'); typ = f[0]; name = f[1]
    seed = zlib.crc32(name.encode()) % 100000 + 1
    try:
        if typ == 'dl':   # dl|NAME|BASE|PEN|rule;rule
            base, pen, rules = f[2], f[3], f[4].split(';'); dlt = int(f[5]) if len(f) > 5 and f[5] else DLT
            log(f'dl {name} base {base} pen {pen} rules {rules}')
            ra = []; [ra.extend(['--rule', r]) for r in rules]
            run(['bin/dlns', '--tours', base, '--start', base, '--seed', str(seed)] + COMMON + ['--time', str(dlt), '--pen', pen] + ra + ['--out', f'y/{name}_best.txt'], f'y/{name}.log', dlt + 60)
            fe = f'y/{name}_best.txt.feas_best.txt'
            if not os.path.exists(fe):
                fb = f'y/{name}_best.txt'
                if not os.path.exists(fb) or score(fb) is None or known(fam.sig_of(fb)):
                    row(name, base, f[4], 'NA', '', '', '', '', 'no feasible'); log(f'dl {name}: no feasible'); continue
                fe = f'y/{name}_pbest.txt'; shutil.copy(fb, fe); name = name + 'P'
                log(f'dl {name}: no feasible; using the partially penalised best {score(fe)}')
            raw = score(fe); sig = fam.sig_of(fe); dd, sd = dist(fe); kn = known(sig)
            if kn:
                row(name, base, f[4], raw, dd, sd, 'skip', '', f'known {kn}'); log(f'dl {name}: raw {raw} known {kn}'); continue
            addknown(f'P92_{name}', sig)
            log(f'dl {name}: raw {raw} dist {dd}/{sd} new sig {sig}; lns {LT}s')
            out = lns(name + '_L1', fe, [], seed + 7, LT)
            sc = score(out); sig2 = fam.sig_of(out); d2, s2 = dist(out); kn2 = known(sig2)
            if not kn2: addknown(f'P92_{name}_L1', sig2)
            row(name, base, f[4], raw, dd, sd, sc, f'{d2}/{s2}', f'final {kn2 or "new"}: {sig2}')
            log(f'dl {name}: lns -> {sc} dist {d2}/{s2} ({kn2 or "new"})')
        elif typ == 'lns':  # lns|NAME|START|SEED|TIME|donor;donor|[focus lo,hi,p]
            start, sd, t = f[2], int(f[3]), int(f[4]); donors = [x for x in f[5].split(';') if x] if len(f) > 5 else []
            focus = f[6] if len(f) > 6 and f[6] else None
            log(f'lns {name} start {start} ({score(start)}) seed {sd} t {t}')
            out = lns(name, start, donors, sd, t, focus)
            sc = score(out); d2, s2 = dist(out); sig2 = fam.sig_of(out)
            row(name, 'lns:' + start, '', score(start), '', '', sc, f'{d2}/{s2}', sig2)
            log(f'lns {name} -> {sc} dist {d2}/{s2}')
    except Exception as e:
        log(f'ERROR {job}: {e!r}')
log('worker exit')
