#!/usr/bin/env python3
"""Knight Moves 5 job runner: queue, scheduler, verifier/collector, leaderboard, status.

Stdlib only.  See RUN.md.  Subcommands:
  add      append a job to the queue file
  run      scheduler: keep <= N cores busy with queued jobs (adopts jobs of a previous scheduler)
  status   running jobs, CPU use, queue summary, best so far
  collect  scan every pool/best/NEW_RECORD file, re-verify with tools/check_tour.py, update leaderboard
  cancel   stop a running/pending job        retry   reset a finished/failed job to pending
"""
import argparse, fcntl, hashlib, json, os, re, shlex, signal, subprocess, sys, time
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
TOOLS = os.path.join(REPO, 'tools')
RECORD = 8123
CLK = os.sysconf('SC_CLK_TCK')

MOVE_RE = re.compile(r'\(([\d/]+),\s*([a-h][1-8])\)')          # exactly the checker's regex
LOOSE_RE = re.compile(r'\(\s*[-\d./eE+]+\s*,\s*[a-h][1-8]\s*\)')  # to detect waits the checker can't parse
SKIP_EXT = {'.cpp', '.cc', '.c', '.h', '.hpp', '.py', '.pyc', '.o', '.so', '.a', '.sh', '.json', '.ipynb',
            '.png', '.jpg', '.gif', '.svg', '.pdf', '.pt', '.onnx', '.bin', '.npy', '.npz', '.gz', '.zip',
            '.html', '.js', '.css', '.cmake', '.lock', '.pid', '.rc'}
SMALL_FILE = 1 << 20      # files below this are always re-read whole when changed
TOPK = 25                 # candidates kept per file (plus everything >= record)


# ----------------------------------------------------------------------------- utilities
def now():
    return time.time()


def ts(t=None):
    return time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(t or now()))


def hms(s):
    s = int(max(0, s))
    return f'{s // 3600}:{s // 60 % 60:02d}:{s % 60:02d}'


def fmt_score(fr):
    fr = Fraction(fr)
    return str(fr.numerator) if fr.denominator == 1 else f'{fr} (~{float(fr):.3f})'


class Paths:
    def __init__(self, a):
        self.root = os.path.abspath(a.root)
        self.out = os.path.abspath(a.out)
        self.tmp = os.path.abspath(a.tmp)
        self.queue = os.path.abspath(a.queue)
        self.logs = os.path.join(self.tmp, 'logs')
        self.state = os.path.join(self.tmp, 'state.json')
        self.state_lock = os.path.join(self.tmp, 'state.lock')
        self.sched_lock = os.path.join(self.tmp, 'scheduler.lock')
        self.cache = os.path.join(self.tmp, 'collect_cache.json')
        self.collect_lock = os.path.join(self.tmp, 'collect.lock')
        self.alerts = os.path.join(self.tmp, 'alerts.log')
        self.history = os.path.join(self.tmp, 'best_history.log')
        self.leaderboard = os.path.join(self.out, 'LEADERBOARD.md')
        self.best = os.path.join(self.out, 'global_best.txt')
        os.makedirs(self.logs, exist_ok=True)
        os.makedirs(self.out, exist_ok=True)


def load_json(path, default):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return default


def save_json(path, obj):
    tmp = path + '.tmp'
    with open(tmp, 'w') as f:
        json.dump(obj, f, indent=1, sort_keys=True)
    os.replace(tmp, path)


def write_atomic(path, text):
    tmp = path + '.tmp'
    with open(tmp, 'w') as f:
        f.write(text)
    os.replace(tmp, path)


class Lock:
    def __init__(self, path, block=True):
        self.path, self.block, self.f = path, block, None

    def __enter__(self):
        self.f = open(self.path, 'a+')
        flags = fcntl.LOCK_EX | (0 if self.block else fcntl.LOCK_NB)
        try:
            fcntl.flock(self.f, flags)
        except BlockingIOError:
            self.f.close()
            self.f = None
        return self.f is not None

    def __exit__(self, *e):
        if self.f:
            fcntl.flock(self.f, fcntl.LOCK_UN)
            self.f.close()


# ----------------------------------------------------------------------------- /proc helpers
def proc_stat(pid):
    """(state, ppid, pgid, sid, utime+stime ticks, rss pages) or None."""
    try:
        with open(f'/proc/{pid}/stat') as f:
            s = f.read()
    except OSError:
        return None
    rest = s[s.rindex(')') + 2:].split()
    return rest[0], int(rest[1]), int(rest[2]), int(rest[3]), int(rest[11]) + int(rest[12]), int(rest[21])


def all_procs():
    out = {}
    for d in os.listdir('/proc'):
        if d.isdigit():
            st = proc_stat(int(d))
            if st:
                out[int(d)] = st
    return out


def session_pids(sid, procs=None):
    procs = procs if procs is not None else all_procs()
    return [p for p, st in procs.items() if st[3] == sid and st[0] != 'Z']


def alive_session(sid):
    st = proc_stat(sid)
    return st is not None and st[0] != 'Z' and st[3] == sid


def signal_session(sid, sig):
    n = 0
    for p in session_pids(sid):
        try:
            os.kill(p, sig)
            n += 1
        except OSError:
            pass
    return n


def cpu_total():
    with open('/proc/stat') as f:
        v = [int(x) for x in f.readline().split()[1:]]
    idle = v[3] + (v[4] if len(v) > 4 else 0)
    return sum(v), idle


def mem_info():
    m = {}
    with open('/proc/meminfo') as f:
        for line in f:
            k, v = line.split(':')
            m[k] = int(v.split()[0])
    return m.get('MemTotal', 0) / 2**20, m.get('MemAvailable', 0) / 2**20


# ----------------------------------------------------------------------------- queue
JOB_DEFAULTS = dict(threads=1, time=3600, grace=60, nice=5, cwd='.', pool=None, agent=None, method=None,
                    retries=0, retry_delay=30, retry_on_timeout=False, priority=0, after=[], env={})
INT_KEYS = ('threads', 'time', 'grace', 'nice', 'retries', 'retry_delay', 'priority')


def parse_queue_line(line):
    line = line.strip()
    if not line or line.startswith('#'):
        return None
    if line.startswith('{'):
        job = json.loads(line)
    else:  # shorthand:  name=x threads=1 time=600 cwd=subagents/nrpa pool=tmp/pool.txt -- command ...
        if ' -- ' not in line:
            raise ValueError("shorthand line needs 'key=value ... -- command'")
        head, cmd = line.split(' -- ', 1)
        job = {'cmd': cmd.strip()}
        for tok in shlex.split(head):
            if '=' not in tok:
                raise ValueError(f'bad token {tok!r}')
            k, v = tok.split('=', 1)
            job[k] = v
    if 'command' in job and 'cmd' not in job:
        job['cmd'] = job.pop('command')
    if 'time_limit' in job:
        job['time'] = job.pop('time_limit')
    if 'dir' in job:
        job['cwd'] = job.pop('dir')
    if not job.get('name') or not job.get('cmd'):
        raise ValueError('job needs name and cmd')
    if not re.fullmatch(r'[A-Za-z0-9_.+-]+', job['name']):
        raise ValueError(f'bad job name {job["name"]!r} (use [A-Za-z0-9_.+-])')
    for k, v in JOB_DEFAULTS.items():
        job.setdefault(k, json.loads(json.dumps(v)))
    for k in INT_KEYS:
        job[k] = int(job[k])
    if isinstance(job['retry_on_timeout'], str):
        job['retry_on_timeout'] = job['retry_on_timeout'].lower() in ('1', 'true', 'yes')
    if isinstance(job['after'], str):
        job['after'] = [x for x in job['after'].split(',') if x]
    return job


def read_queue(P, warn=None):
    jobs, order = {}, []
    if not os.path.exists(P.queue):
        return jobs, order
    with open(P.queue) as f:
        for i, line in enumerate(f, 1):
            try:
                job = parse_queue_line(line)
            except Exception as e:  # noqa
                if warn is not None and (i, line) not in warn:
                    warn.add((i, line))
                    print(f'[{ts()}] queue line {i} ignored: {e}', flush=True)
                continue
            if job is None:
                continue
            if job['name'] in jobs:
                if warn is not None and ('dup', job['name']) not in warn:
                    warn.add(('dup', job['name']))
                    print(f'[{ts()}] queue line {i}: duplicate job name {job["name"]} ignored', flush=True)
                continue
            job['line'] = i
            jobs[job['name']] = job
            order.append(job['name'])
    return jobs, order


def job_dir(P, job):
    return os.path.normpath(os.path.join(P.root, job['cwd']))


def job_pool(P, job):
    if not job.get('pool'):
        return None
    return os.path.normpath(os.path.join(job_dir(P, job), job['pool']))


def job_agent(P, job):
    if job.get('agent'):
        return job['agent']
    for p in (job_dir(P, job), job_pool(P, job)):
        if p:
            rel = os.path.relpath(p, P.root).split(os.sep)
            if len(rel) >= 2 and rel[0] == 'subagents':
                return rel[1]
    return 'runner'


def fill(job, P):
    vals = dict(threads=job['threads'], time=job['time'], name=job['name'], root=P.root,
                pool=job_pool(P, job) or '', logdir=P.logs, cwd=job_dir(P, job))

    class D(dict):
        def __missing__(self, k):
            return '{' + k + '}'
    return job['cmd'].format_map(D(vals))


# ----------------------------------------------------------------------------- scheduler
WRAPPER = r'''
echo "### runner job $RUNNER_JOB attempt $RUNNER_ATTEMPT start $(date '+%F %T') host $(hostname)"
echo "### cwd: $RUNNER_CWD"
echo "### cmd: $RUNNER_CMD"
echo "### threads=$RUNNER_THREADS time=$RUNNER_TIME grace=$RUNNER_GRACE nice=$RUNNER_NICE"
cd "$RUNNER_CWD" || { echo 126 > "$RUNNER_RC.tmp"; mv "$RUNNER_RC.tmp" "$RUNNER_RC"; exit 126; }
nice -n "$RUNNER_NICE" timeout -s INT -k 30 "$((RUNNER_TIME + RUNNER_GRACE))" bash -c "$RUNNER_CMD" < /dev/null
rc=$?
echo "### end $(date '+%F %T') rc=$rc"
echo $rc > "$RUNNER_RC.tmp"; mv "$RUNNER_RC.tmp" "$RUNNER_RC"
exit $rc
'''


class Scheduler:
    def __init__(self, P, a):
        self.P, self.a = P, a
        self.cores = a.cores or os.cpu_count()
        self.children = {}            # name -> Popen (only jobs launched by this process)
        self.warned = set()
        self.stop = False
        self.collector = None
        self.last_collect = 0.0
        self.want_collect = True

    def log(self, msg):
        print(f'[{ts()}] {msg}', flush=True)

    def launch(self, job, st):
        P = self.P
        st['attempts'] = st.get('attempts', 0) + 1
        att = st['attempts']
        logf = os.path.join(P.logs, f'{job["name"]}.{att}.log')
        rcf = os.path.join(P.logs, f'{job["name"]}.{att}.rc')
        for f in (rcf, rcf + '.tmp'):
            if os.path.exists(f):
                os.remove(f)
        link = os.path.join(P.logs, f'{job["name"]}.log')
        try:
            if os.path.islink(link) or os.path.exists(link):
                os.remove(link)
            os.symlink(os.path.basename(logf), link)
        except OSError:
            pass
        pool = job_pool(P, job)
        if pool:
            os.makedirs(os.path.dirname(pool), exist_ok=True)
        env = dict(os.environ)
        env.update({k: str(v) for k, v in job['env'].items()})
        env.update(RUNNER_JOB=job['name'], RUNNER_ATTEMPT=str(att), RUNNER_CWD=job_dir(P, job),
                   RUNNER_CMD=fill(job, P), RUNNER_THREADS=str(job['threads']), RUNNER_TIME=str(job['time']),
                   RUNNER_GRACE=str(job['grace']), RUNNER_NICE=str(job['nice']), RUNNER_RC=rcf,
                   RUNNER_POOL=pool or '', OMP_NUM_THREADS=str(job['threads']))
        with open(logf, 'ab') as lf:
            p = subprocess.Popen(['bash', '-c', WRAPPER], stdout=lf, stderr=subprocess.STDOUT,
                                 stdin=subprocess.DEVNULL, env=env, start_new_session=True, cwd=P.root)
        self.children[job['name']] = p
        st.update(status='running', pid=p.pid, start=now(), end=None, rc=None, log=logf, rcfile=rcf,
                  threads=job['threads'], time=job['time'], grace=job['grace'], cancel=False,
                  agent=job_agent(P, job), pool=pool)
        self.log(f'start {job["name"]} (attempt {att}, {job["threads"]} thr, {job["time"]}s, pid {p.pid}) '
                 f'log {os.path.relpath(logf, P.root)}')

    def reap(self, name, job, st):
        """Check a running job; returns True if it finished."""
        rcf = st.get('rcfile')
        p = self.children.get(name)
        if p is not None:
            p.poll()
        rc = None
        if rcf and os.path.exists(rcf):
            try:
                rc = int(open(rcf).read().strip())
            except ValueError:
                rc = -1
        elif alive_session(st['pid']):
            # hard safety net: wrapper should already have enforced the limit
            if now() - st['start'] > st['time'] + st['grace'] + 120:
                self.log(f'{name}: over hard limit, killing session')
                signal_session(st['pid'], signal.SIGKILL)
            return False
        else:
            if p is not None and p.returncode is None:
                return False
            rc = -9  # wrapper vanished without writing rc (killed)
        self.children.pop(name, None)
        if p is not None and p.returncode is None:
            try:
                p.wait(timeout=1)
            except subprocess.TimeoutExpired:
                pass
        st['rc'], st['end'] = rc, now()
        dur = hms(st['end'] - st['start'])
        timed_out = rc in (124, 137) and st['end'] - st['start'] >= st['time']
        retries = job['retries'] if job else 0
        if st.get('cancel'):
            st['status'] = 'cancelled'
        elif rc == 0 or (timed_out and not (job and job['retry_on_timeout'])):
            st['status'] = 'done' if rc == 0 else 'timeout'
        elif st['attempts'] <= retries:
            st['status'] = 'pending'
            st['not_before'] = now() + (job['retry_delay'] if job else 30)
            self.log(f'{name} failed rc={rc} after {dur}; retry {st["attempts"]}/{retries} '
                     f'in {job["retry_delay"]}s')
            self.want_collect = True
            return True
        else:
            st['status'] = 'failed'
        self.log(f'{name} {st["status"]} rc={rc} after {dur}')
        self.want_collect = True
        return True

    def tick(self):
        P = self.P
        jobs, order = read_queue(P, self.warned)
        with Lock(P.state_lock):
            state = load_json(P.state, {'jobs': {}})
            S = state['jobs']
            for name in order:
                S.setdefault(name, {'status': 'pending', 'attempts': 0})
            # reap
            for name, st in S.items():
                if st.get('status') == 'running':
                    self.reap(name, jobs.get(name), st)
            # cancels requested while pending
            for name, st in S.items():
                if st.get('status') == 'pending' and st.get('cancel'):
                    st['status'] = 'cancelled'
            used = sum(st.get('threads', 1) for st in S.values() if st.get('status') == 'running')
            free = self.cores - used
            if self.a.respect_load:
                ext = os.getloadavg()[0] - used
                free = min(free, int(self.cores - used - max(0.0, ext) + 0.25))
            # launch: priority desc, then queue order; backfill smaller jobs into gaps
            cands = [n for n in order if S[n].get('status') == 'pending'
                     and S[n].get('not_before', 0) <= now()
                     and all(S.get(d, {}).get('status') == 'done' for d in jobs[n]['after'])]
            cands.sort(key=lambda n: (-jobs[n]['priority'], jobs[n]['line']))
            for n in cands:
                if self.stop:
                    break
                need = jobs[n]['threads']
                if need <= free or (used == 0 and need > self.cores):
                    self.launch(jobs[n], S[n])
                    free -= need
                    used += need
            state['scheduler'] = {'pid': os.getpid(), 'cores': self.cores, 'heartbeat': now(),
                                  'queue': P.queue}
            save_json(P.state, state)
        return jobs, S

    def maybe_collect(self, force=False):
        if self.collector is not None:
            if self.collector.poll() is None:
                return
            self.collector = None
        if force or self.want_collect or now() - self.last_collect >= self.a.collect_every:
            self.want_collect = False
            self.last_collect = now()
            cmd = ['nice', '-n', '10', sys.executable, os.path.abspath(__file__), '--root', self.P.root,
                   '--out', self.P.out, '--tmp', self.P.tmp, '--queue', self.P.queue, 'collect', '--quiet',
                   '--record', str(self.a.record)]
            self.collector = subprocess.Popen(cmd)

    def run(self):
        P = self.P
        lock = Lock(P.sched_lock, block=False)
        if not lock.__enter__():
            sys.exit('another scheduler is running for this tmp dir (see `status`)')

        def on_sig(sig, frm):
            self.stop = True
        signal.signal(signal.SIGINT, on_sig)
        signal.signal(signal.SIGTERM, on_sig)
        self.log(f'scheduler pid {os.getpid()} cores={self.cores} queue={os.path.relpath(P.queue, P.root)}')
        with Lock(P.state_lock):
            state = load_json(P.state, {'jobs': {}})
            adopted = [n for n, st in state['jobs'].items() if st.get('status') == 'running']
        if adopted:
            self.log(f'adopting {len(adopted)} running job(s) from previous scheduler: {", ".join(adopted)}')
        try:
            while not self.stop:
                jobs, S = self.tick()
                self.maybe_collect()
                running = [n for n, st in S.items() if st.get('status') == 'running']
                pending = [n for n in jobs if S.get(n, {}).get('status') == 'pending']
                if self.a.once:
                    break
                if self.a.exit_when_idle and not running:
                    blocked = [n for n in pending if any(S.get(d, {}).get('status') in ('failed', 'cancelled',
                               'timeout') for d in jobs[n]['after'])]
                    if len(pending) == len(blocked):
                        if blocked:
                            self.log(f'blocked by failed deps: {", ".join(blocked)}')
                        break
                time.sleep(self.a.poll)
        finally:
            if self.stop and self.a.kill_on_exit:
                self.log('stopping: sending SIGINT to running jobs')
                with Lock(P.state_lock):
                    state = load_json(P.state, {'jobs': {}})
                    for n, st in state['jobs'].items():
                        if st.get('status') == 'running':
                            st['cancel'] = True
                            signal_session(st['pid'], signal.SIGINT)
                    save_json(P.state, state)
                t0 = now()
                while now() - t0 < 30 and any(alive_session(st['pid']) for st in state['jobs'].values()
                                              if st.get('status') == 'running'):
                    time.sleep(0.5)
                self.tick()
            elif self.stop:
                self.log('scheduler stopped; running jobs keep going (next `run` adopts them, '
                         '`cancel --all` stops them)')
            if self.collector is not None:
                self.collector.wait()
            if not self.stop or self.a.exit_when_idle:
                self.maybe_collect(force=True)
                if self.collector is not None:
                    self.collector.wait()
            lock.__exit__()


# ----------------------------------------------------------------------------- collector
def norm_tour(moves):
    return ', '.join(f'({w}, {q})' for w, q in moves)


def tour_hash(text):
    return hashlib.sha1(text.encode()).hexdigest()[:16]


def parse_tour_line(line):
    """Return (score Fraction, normalized text) | ('bad', reason) | None."""
    ms = MOVE_RE.findall(line)
    if len(ms) < 20:
        return None
    if len(LOOSE_RE.findall(line)) != len(ms):
        return 'bad', 'wait format not parseable by checker'
    moves = [(Fraction(w), q) for w, q in ms]
    if moves[-1][1] != 'h8':
        return 'partial', None
    return sum((w for w, _ in moves), Fraction(0)), norm_tour(moves)


def discover(P, jobs):
    """List of (path, agent, method)."""
    root = P.root
    own = {P.leaderboard, P.best, P.cache, P.state}
    pools = {}
    for j in jobs.values():
        pp = job_pool(P, j)
        if pp:
            pools[os.path.realpath(pp)] = (job_agent(P, j), j.get('method') or j['name'])
    found = {}

    def add(path):
        rp = os.path.realpath(path)
        if rp in found or path in own or os.path.basename(path).startswith('RECORD_ALERT'):
            return
        ext = os.path.splitext(path)[1].lower()
        if ext in SKIP_EXT or path.endswith(('.tmp', '~')):
            return
        if rp in pools:
            found[rp] = (path, *pools[rp])
            return
        rel = os.path.relpath(path, root).split(os.sep)
        if rel[0] == 'subagents' and len(rel) >= 3:
            agent, method = rel[1], '/'.join(rel[2:])
        elif rel[0] == 'runs':
            agent, method = 'runs/' + (rel[1] if len(rel) > 2 else ''), '/'.join(rel[1:])
        elif rel[0] == 'data':
            agent, method = 'data', '/'.join(rel[2:]) if len(rel) > 2 else rel[-1]
        else:
            agent, method = 'other', '/'.join(rel)
        found[rp] = (path, agent, method)

    def walk(d, depth=8):
        if not os.path.isdir(d):
            return
        for dp, dns, fns in os.walk(d):
            dns[:] = [x for x in dns if not x.startswith(('.', '__pycache__'))
                      and dp.count(os.sep) - d.count(os.sep) < depth
                      and os.path.join(dp, x) != P.logs]
            for fn in fns:
                add(os.path.join(dp, fn))
    for pp in pools:
        if os.path.isfile(pp):
            add(pp)
    sub = os.path.join(root, 'subagents')
    if os.path.isdir(sub):
        for a in sorted(os.listdir(sub)):
            if os.path.isdir(os.path.join(sub, a)):
                walk(os.path.join(sub, a))
    walk(os.path.join(root, 'runs'))
    walk(os.path.join(root, 'data', 'tours'))
    for extra in P.extra_scan:
        if os.path.isdir(extra):
            walk(extra)
        elif os.path.isfile(extra):
            add(extra)
    return list(found.values())


def scan_file(path, entry, tours):
    """Update a file's cache entry in place. Returns number of new tour lines."""
    try:
        stt = os.stat(path)
    except OSError:
        return 0
    if entry.get('size') == stt.st_size and entry.get('mtime') == stt.st_mtime:
        return 0
    with open(path, 'rb') as f:
        head = f.read(8192)
        if b'\0' in head:
            entry.update(size=stt.st_size, mtime=stt.st_mtime, binary=True)
            return 0
        fp_head = hashlib.sha1(head[:4096]).hexdigest()
        incremental = (stt.st_size >= SMALL_FILE and entry.get('offset') and stt.st_size >= entry['offset']
                       and entry.get('fp_head') == fp_head)
        if incremental:
            f.seek(entry['offset'])
        else:
            entry.update(offset=0, n_tours=0, n_partial=0, n_bad=0, n_ge7500=0, max=None, top=[])
            f.seek(0)
        data = f.read()
    start = entry['offset']
    if stt.st_size >= SMALL_FILE and not data.endswith(b'\n') and now() - stt.st_mtime < 10:
        cut = data.rfind(b'\n') + 1   # leave a line still being written for next time
        data = data[:cut]
    entry['offset'] = start + len(data)
    entry.update(size=stt.st_size, mtime=stt.st_mtime, fp_head=fp_head, binary=False)
    top = {h: Fraction(s) for s, h in entry['top']}
    new = 0
    for raw in data.split(b'\n'):
        if b'(' not in raw:
            continue
        r = parse_tour_line(raw.decode('utf-8', 'replace'))
        if r is None:
            continue
        if r[0] == 'partial':
            entry['n_partial'] += 1
            continue
        if r[0] == 'bad':
            entry['n_bad'] += 1
            continue
        sc, text = r
        new += 1
        entry['n_tours'] += 1
        if sc >= 7500:
            entry['n_ge7500'] += 1
        if entry['max'] is None or sc > Fraction(entry['max']):
            entry['max'] = str(sc)
        h = tour_hash(text)
        if h in top:
            continue
        if len(top) < TOPK or sc > min(top.values()) or sc >= RECORD:
            top[h] = sc
            tours[h] = {'tour': text, 'score': str(sc)}
            if len(top) > TOPK:   # drop the lowest one below the record
                lo = min((v, k) for k, v in top.items())
                if lo[0] < RECORD:
                    del top[lo[1]]
    entry['top'] = sorted(([str(v), k] for k, v in top.items()), key=lambda x: -Fraction(x[0]))
    return new


_checker = None


def checker():
    global _checker
    if _checker is None:
        sys.path.insert(0, TOOLS)
        import check_tour  # noqa: E402  (tools/check_tour.py, exact Fractions)
        _checker = check_tour
    return _checker


def verify(text):
    """Re-verify with tools/check_tour.py (imported). Returns dict."""
    import sim  # noqa: E402 (tools/sim.py, on sys.path via checker())
    ck = checker()
    moves = [(int(a) if '/' not in a else Fraction(a), b) for a, b in MOVE_RE.findall(text)]
    ok, T, msg = sim.simulate(sim.MAIN_ROWS, moves)          # cheap pre-filter, same function the checker uses
    if not ok:
        return {'ok': False, 'T': str(T), 'msg': msg}
    ok, T, mv, bad = ck.check(text, show=False)               # the official check (incl. necessity test)
    good = bool(ok) and mv <= 3 and bad == []
    reason = 'ok' if good else (f'maxvisits={mv}' if mv > 3 else f'unnecessary waits at moves {bad}')
    return {'ok': good, 'T': str(T), 'msg': reason, 'maxvisits': mv}


def verify_cli(text):
    """Independent second check via the checker's command line (used before announcing a record)."""
    r = subprocess.run([sys.executable, os.path.join(TOOLS, 'check_tour.py'), text], capture_output=True,
                       text=True, cwd=TOOLS, timeout=600)
    out = (r.stdout + r.stderr).strip()
    good = 'ok=True' in out and re.search(r'maxvisits=[0-3]\b', out) and 'unnecessary=[]' in out
    return bool(good), out


def collect(P, a):
    with Lock(P.collect_lock, block=True):
        return _collect(P, a)


def _collect(P, a):
    t0 = now()
    jobs, _ = read_queue(P)
    cache = load_json(P.cache, {})
    files = cache.setdefault('files', {})
    tours = cache.setdefault('tours', {})
    ver = cache.setdefault('verified', {})
    alerted = set(cache.setdefault('alerted', []))
    prev_best = cache.get('best')
    if a.full:
        files.clear()
    seen = set()
    n_new = 0
    for path, agent, method in discover(P, jobs):
        key = os.path.relpath(path, P.root)
        seen.add(key)
        e = files.setdefault(key, {})
        e['agent'], e['method'] = agent, method
        n_new += scan_file(path, e, tours)
    for k in list(files):
        if k not in seen:
            del files[k]
    # candidate list (dedup by tour hash, remember every source)
    srcs = {}
    for key, e in files.items():
        for s, h in e.get('top', []):
            srcs.setdefault(h, []).append((key, e['agent'], e['method']))
    cand = sorted(srcs, key=lambda h: -Fraction(tours[h]['score']))
    checks, budget_t = 0, a.max_seconds
    over_budget = False

    def get(h):
        nonlocal checks, over_budget
        if h in ver:
            return ver[h]
        if checks >= a.max_checks or now() - t0 > budget_t:
            over_budget = True
            return None
        checks += 1
        ver[h] = verify(tours[h]['tour'])
        ver[h]['when'] = ts()
        return ver[h]
    # global best: verify every candidate (descending) down to and including ties with the best verified
    best_h, invalid = None, []
    for h in cand:
        sc = Fraction(tours[h]['score'])
        if best_h is not None and sc < Fraction(tours[best_h]['score']):
            break
        v = get(h)
        if v is None:
            continue
        if v['ok']:
            if best_h is None:
                best_h = h
        else:
            invalid.append(h)
    ties = [h for h in cand if best_h and tours[h]['score'] == tours[best_h]['score'] and ver.get(h, {}).get('ok')]
    # best per (agent, method) and per agent
    groups = {}
    for h in cand:
        for key, agent, method in srcs[h]:
            groups.setdefault((agent, method), []).append(h)
    gbest = {}
    for g, hs in groups.items():
        hs = sorted(set(hs), key=lambda h: -Fraction(tours[h]['score']))
        gbest[g] = None
        for h in hs:
            v = get(h)
            if v and v['ok']:
                gbest[g] = h
                break
    # alerts: verified tours beating the record (double-checked with the checker CLI)
    alerts = []
    for h in cand:
        sc = Fraction(tours[h]['score'])
        if sc <= a.record:
            break
        if ver.get(h, {}).get('ok') and h not in alerted:
            good, out = verify_cli(tours[h]['tour'])
            ver[h]['cli'] = out
            if not good:
                ver[h].update(ok=False, msg='CLI checker disagrees: ' + out)
                continue
            alerted.add(h)
            alerts.append(h)
            fn = os.path.join(P.out, f'RECORD_ALERT_{tours[h]["score"].replace("/", "_")}_{h[:8]}.txt')
            with open(fn, 'w') as f:
                f.write(f'{tours[h]["score"]}\n{tours[h]["tour"]}\n')
                f.write(f'# verified {ts()} by subagents/runner (tools/check_tour.py): {out}\n')
                for key, agent, method in srcs[h]:
                    f.write(f'# source: {key} (agent {agent}, method {method})\n')
            banner(P, h, tours[h], srcs[h], out, fn)
    cache['alerted'] = sorted(alerted)
    # outputs
    if best_h:
        bt = tours[best_h]
        cur = (prev_best or {}).get('hash')
        if cur != best_h and (prev_best is None or Fraction(bt['score']) >= Fraction(prev_best['score'])):
            write_atomic(P.best, f'{bt["score"]}\n{bt["tour"]}\n')
            with open(P.history, 'a') as f:
                f.write(f'{ts()}\t{bt["score"]}\t{best_h}\t{srcs[best_h][0][0]}\n')
            if prev_best is None or Fraction(bt['score']) > Fraction(prev_best['score']):
                print(f'[{ts()}] global best now {fmt_score(bt["score"])} ({srcs[best_h][0][1]}: '
                      f'{srcs[best_h][0][0]})', flush=True)
            cache['best'] = {'hash': best_h, 'score': bt['score'], 'source': srcs[best_h][0][0],
                             'agent': srcs[best_h][0][1], 'when': ts()}
    # prune tours that no file references any more (keep verified results)
    keep = set(srcs) | alerted | ({cache['best']['hash']} if cache.get('best') else set())
    for h in list(tours):
        if h not in keep:
            del tours[h]
    cache['last_collect'] = {'when': ts(), 'files': len(files), 'new_tours': n_new, 'checks': checks,
                             'seconds': round(now() - t0, 2), 'incomplete': over_budget}
    save_json(P.cache, cache)
    write_leaderboard(P, a, cache, srcs, cand, best_h, ties, invalid, gbest, groups)
    if not a.quiet:
        lc = cache['last_collect']
        b = cache.get('best')
        print(f'collect: {lc["files"]} files, {n_new} new tour lines, {checks} checks, {lc["seconds"]}s'
              + (' (budget hit; rest next time)' if over_budget else ''))
        print(f'global best verified: {fmt_score(b["score"]) if b else "-"}'
              + (f'  [{b["agent"]}: {b["source"]}]' if b else '') + f'   record {a.record}')
        if invalid:
            print(f'{len(invalid)} candidate(s) >= best FAILED verification (see LEADERBOARD.md)')
        print(f'wrote {os.path.relpath(P.leaderboard, P.root)} and {os.path.relpath(P.best, P.root)}')
    return cache


def banner(P, h, t, srcs, out, fn):
    line = '!' * 78
    msg = [line, line,
           f'!!!  NEW RECORD CANDIDATE VERIFIED: {fmt_score(t["score"])} minutes (record {RECORD_ARG[0]})',
           f'!!!  checker: {out}',
           *[f'!!!  source: {k} (agent {ag}, method {m})' for k, ag, m in srcs],
           f'!!!  saved:  {os.path.relpath(fn, P.root)}', line, line]
    txt = '\a\n' + '\n'.join(msg) + '\n'
    sys.stdout.write(txt)
    sys.stdout.flush()
    sys.stderr.write(txt)
    with open(P.alerts, 'a') as f:
        f.write(f'{ts()}\t{t["score"]}\t{h}\t{fn}\n')


RECORD_ARG = [RECORD]


def write_leaderboard(P, a, cache, srcs, cand, best_h, ties, invalid, gbest, groups):
    tours, ver, files = cache['tours'], cache['verified'], cache['files']
    L = ['# Leaderboard (auto-generated by subagents/runner/runner.py collect)', '',
         f'Updated {ts()}. Record to beat: **{a.record}**. Every score below was re-verified with '
         '`tools/check_tour.py` (exact Fractions: legal, <= 3 visits, no unnecessary waits).', '']
    if best_h:
        b = tours[best_h]
        beat = Fraction(b['score']) > a.record
        L += [f'**Global best verified: {fmt_score(b["score"])}**' + ('  -- **NEW RECORD!**' if beat else ''),
              f'(`{os.path.relpath(P.best, P.root)}`; sources: '
              + ', '.join(f'`{k}`' for k, _, _ in srcs[best_h][:6]) + ')', '']
        if len(ties) > 1:
            L += [f'{len(ties)} distinct verified tours share this score.', '']
    else:
        L += ['No verified tour found yet.', '']
    # per agent
    agents = {}
    for (ag, m), h in gbest.items():
        agents.setdefault(ag, []).append((m, h))
    stats = {}
    for k, e in files.items():
        s = stats.setdefault(e['agent'], [0, 0, 0])
        s[0] += 1
        s[1] += e.get('n_tours', 0)
        s[2] += e.get('n_ge7500', 0)
    rows = []
    for ag, lst in agents.items():
        good = [(Fraction(tours[h]['score']), m, h) for m, h in lst if h]
        top_claim = max((Fraction(tours[h]['score']) for (g, _), hs in groups.items() if g == ag for h in hs),
                        default=None)
        if good:
            sc, m, h = max(good)
            rows.append((sc, ag, m, top_claim))
        else:
            rows.append((Fraction(-1), ag, '-', top_claim))
    rows.sort(key=lambda r: (-r[0], r[1]))
    L += ['## Best per agent', '', '| agent | best verified | from | complete tours seen | >= 7500 | top claimed |',
          '|---|---:|---|---:|---:|---:|']
    for sc, ag, m, tc in rows:
        st = stats.get(ag, [0, 0, 0])
        flag = '' if tc is None or sc < 0 or tc <= sc else ' (higher claim invalid)'
        L.append(f'| {ag} | {fmt_score(sc) if sc >= 0 else "-"} | `{m}` | {st[1]} | {st[2]} | '
                 f'{fmt_score(tc) if tc is not None else "-"}{flag} |')
    L += ['', '## Best per agent / method (source)', '', '| agent | method / file | best verified | tours | max claimed |',
          '|---|---|---:|---:|---:|']
    gm = {}
    for k, e in files.items():
        g = gm.setdefault((e['agent'], e['method']), [0, None])
        g[0] += e.get('n_tours', 0)
        if e.get('max') is not None and (g[1] is None or Fraction(e['max']) > g[1]):
            g[1] = Fraction(e['max'])
    for (ag, m) in sorted(gbest, key=lambda g: (-(Fraction(tours[gbest[g]]['score']) if gbest[g] else -1), g)):
        h = gbest[(ag, m)]
        n, mx = gm.get((ag, m), [0, None])
        L.append(f'| {ag} | `{m}` | {fmt_score(tours[h]["score"]) if h else "-"} | {n} | '
                 f'{fmt_score(mx) if mx is not None else "-"} |')
    bad = [h for h in cand if ver.get(h) and not ver[h]['ok']]
    if bad:
        L += ['', '## Candidates that FAILED re-verification (highest first)', '',
              '| claimed (sum of waits) | reason | source |', '|---:|---|---|']
        for h in bad[:20]:
            L.append(f'| {fmt_score(tours[h]["score"])} | {ver[h]["msg"]} | `{srcs[h][0][0]}` |')
    lc = cache.get('last_collect', {})
    L += ['', f'_Scanned {lc.get("files", 0)} files under subagents/*/, runs/, data/tours/ and queue pools; '
          f'{len(ver)} distinct tours verified so far'
          + ('; verification budget hit, more next run' if lc.get('incomplete') else '') + '._', '']
    alerts = load_alerts(P)
    if alerts:
        L += ['## Record alerts', ''] + [f'- {x}' for x in alerts] + ['']
    write_atomic(P.leaderboard, '\n'.join(L))


def load_alerts(P):
    try:
        with open(P.alerts) as f:
            return [l.strip() for l in f if l.strip()]
    except OSError:
        return []


# ----------------------------------------------------------------------------- status / control
def cmd_status(P, a):
    jobs, order = read_queue(P)
    with Lock(P.state_lock):
        state = load_json(P.state, {'jobs': {}})
    S = state['jobs']
    sch = state.get('scheduler', {})
    with Lock(P.sched_lock, block=False) as free:
        alive = not free
    print(f'scheduler: {"RUNNING pid " + str(sch.get("pid")) if alive else "not running"}'
          + (f', cores={sch.get("cores")}, heartbeat {int(now() - sch["heartbeat"])}s ago' if alive and sch else ''))
    # CPU sample
    procs1 = all_procs()
    c1 = cpu_total()
    time.sleep(a.sample)
    procs2 = all_procs()
    c2 = cpu_total()
    dt_all = max(1, c2[0] - c1[0])
    busy = 1 - (c2[1] - c1[1]) / dt_all
    ncpu = os.cpu_count()
    la = os.getloadavg()
    mt, ma = mem_info()
    print(f'machine: {ncpu} cpus, busy {busy * 100:.0f}% (~{busy * ncpu:.1f} cores), load {la[0]:.2f} '
          f'{la[1]:.2f} {la[2]:.2f}, mem avail {ma:.1f}/{mt:.1f} GB')
    running = [n for n, st in S.items() if st.get('status') == 'running']
    tot_thr = 0
    if running:
        print(f'\nrunning ({len(running)}):')
        print(f'  {"name":28s} {"thr":>3s} {"elapsed":>8s} {"limit":>8s} {"cpu%":>6s} {"rss MB":>7s}  last log line')
    for n in running:
        st = S[n]
        sid = st['pid']
        pids = session_pids(sid, procs2)
        t1 = sum(procs1[p][4] for p in pids if p in procs1)
        t2 = sum(procs2[p][4] for p in pids)
        cpu = (t2 - t1) / CLK / a.sample * 100
        rss = sum(procs2[p][5] for p in pids) * os.sysconf('SC_PAGE_SIZE') / 2**20
        tot_thr += st.get('threads', 1)
        last = tail(st.get('log'))
        print(f'  {n:28s} {st.get("threads", 1):3d} {hms(now() - st["start"]):>8s} {hms(st["time"]):>8s} '
              f'{cpu:6.0f} {rss:7.0f}  {last[:70]}' + ('' if pids else '  [NO PROCESSES]'))
    counts = {}
    for n in set(order) | set(S):
        s = S.get(n, {}).get('status', 'pending') if n in S else 'pending (new)'
        counts[s] = counts.get(s, 0) + 1
    print(f'\njobs: ' + ', '.join(f'{k} {v}' for k, v in sorted(counts.items())) + f'; threads in use {tot_thr}')
    fin = sorted((st.get('end') or 0, n) for n, st in S.items() if st.get('status') in
                 ('done', 'failed', 'timeout', 'cancelled'))
    if fin:
        print('recently finished:')
        for e, n in fin[-a.recent:]:
            st = S[n]
            print(f'  {n:28s} {st["status"]:9s} rc={st.get("rc")} att={st.get("attempts")} '
                  f'{hms((st.get("end") or 0) - (st.get("start") or 0))}  {ts(e)}')
    pend = [n for n in order if S.get(n, {}).get('status', 'pending') == 'pending']
    if pend:
        print(f'pending: {", ".join(pend[:12])}' + (' ...' if len(pend) > 12 else ''))
    cache = load_json(P.cache, {})
    b = cache.get('best')
    lc = cache.get('last_collect', {})
    print(f'\nbest verified so far: {fmt_score(b["score"]) + "  [" + b["agent"] + ": " + b["source"] + "]" if b else "-"}'
          f'   (record {a.record}; last collect {lc.get("when", "never")})')
    al = load_alerts(P)
    if al:
        print(f'!!! {len(al)} RECORD ALERT(S): ' + '; '.join(al[-3:]))


def tail(path, n=1):
    if not path:
        return ''
    try:
        with open(path, 'rb') as f:
            f.seek(0, 2)
            sz = f.tell()
            f.seek(max(0, sz - 4096))
            lines = [l for l in f.read().decode('utf-8', 'replace').replace('\r', '\n').split('\n') if l.strip()]
        return lines[-1].strip() if lines else ''
    except OSError:
        return ''


def cmd_add(P, a):
    job = {'name': a.name, 'cmd': ' '.join(shlex.quote(x) for x in a.command) if len(a.command) > 1
           else a.command[0], 'threads': a.threads, 'time': a.time, 'nice': a.nice, 'cwd': a.cwd}
    for k in ('pool', 'agent', 'method'):
        if getattr(a, k):
            job[k] = getattr(a, k)
    if a.retries:
        job['retries'] = a.retries
    if a.after:
        job['after'] = a.after.split(',')
    if a.priority:
        job['priority'] = a.priority
    parse_queue_line(json.dumps(job))  # validate
    jobs, _ = read_queue(P)
    if a.name in jobs:
        sys.exit(f'job {a.name} already in queue')
    with open(P.queue, 'a') as f:
        f.write(json.dumps(job) + '\n')
    print(f'queued {a.name}')


def cmd_cancel(P, a):
    jobs, order = read_queue(P)
    with Lock(P.state_lock):
        state = load_json(P.state, {'jobs': {}})
        S = state['jobs']
        names = [n for n, st in S.items() if st.get('status') in ('running', 'pending')] if a.all else a.names
        for n in names:
            st = S.setdefault(n, {'status': 'pending', 'attempts': 0})
            st['cancel'] = True
            if st.get('status') == 'running':
                k = signal_session(st['pid'], signal.SIGINT)
                print(f'{n}: SIGINT sent to {k} process(es); scheduler marks it cancelled')
            elif st.get('status') == 'pending':
                st['status'] = 'cancelled'
                print(f'{n}: cancelled (was pending)')
            else:
                print(f'{n}: status {st.get("status")}, nothing to do')
        save_json(P.state, state)


def cmd_retry(P, a):
    with Lock(P.state_lock):
        state = load_json(P.state, {'jobs': {}})
        for n in a.names:
            st = state['jobs'].get(n)
            if not st or st.get('status') == 'running':
                print(f'{n}: not resettable ({st.get("status") if st else "unknown"})')
                continue
            st.update(status='pending', cancel=False, not_before=0, attempts=0)
            print(f'{n}: reset to pending')
        save_json(P.state, state)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=REPO, help='repo root to scan (default: this repo)')
    ap.add_argument('--out', default=HERE, help='where LEADERBOARD.md / global_best.txt go')
    ap.add_argument('--tmp', default=os.path.join(HERE, 'tmp'), help='state, logs, caches')
    ap.add_argument('--queue', default=os.path.join(HERE, 'queue.txt'))
    ap.add_argument('--record', type=int, default=RECORD, help='alert when a verified tour beats this')
    ap.add_argument('--scan', action='append', default=[], help='extra file/dir for the collector')
    sp = ap.add_subparsers(dest='cmd', required=True)
    r = sp.add_parser('run', help='scheduler loop')
    r.add_argument('--cores', type=int, default=0, help='max threads in use (default nproc)')
    r.add_argument('--poll', type=float, default=2.0)
    r.add_argument('--collect-every', type=float, default=300, help='seconds between collector runs')
    r.add_argument('--exit-when-idle', action='store_true', help='exit when nothing is pending/running')
    r.add_argument('--once', action='store_true', help='one scheduling pass, then exit')
    r.add_argument('--kill-on-exit', action='store_true', help='on Ctrl-C/SIGTERM also stop running jobs')
    r.add_argument('--respect-load', action='store_true',
                   help='also count load from other processes (loadavg) when deciding free cores')
    s = sp.add_parser('status')
    s.add_argument('--sample', type=float, default=1.0, help='seconds to sample CPU use')
    s.add_argument('--recent', type=int, default=8)
    c = sp.add_parser('collect')
    c.add_argument('--full', action='store_true', help='rescan all files from scratch')
    c.add_argument('--quiet', action='store_true')
    c.add_argument('--max-checks', type=int, default=400, help='max new verifications per run')
    c.add_argument('--max-seconds', type=float, default=600, help='verification time budget per run')
    ad = sp.add_parser('add', help='append a job: add NAME [opts] -- command ...')
    ad.add_argument('name')
    ad.add_argument('--threads', type=int, default=1)
    ad.add_argument('--time', type=int, default=3600, help='program budget (s); hard kill at time+grace')
    ad.add_argument('--nice', type=int, default=5)
    ad.add_argument('--cwd', default='.', help='working dir, relative to repo root')
    ad.add_argument('--pool', help='output pool path (relative to cwd)')
    ad.add_argument('--agent')
    ad.add_argument('--method')
    ad.add_argument('--retries', type=int, default=0)
    ad.add_argument('--after', help='comma-separated job names that must finish (done) first')
    ad.add_argument('--priority', type=int, default=0)
    ad.add_argument('command', nargs='+')
    ca = sp.add_parser('cancel')
    ca.add_argument('names', nargs='*')
    ca.add_argument('--all', action='store_true')
    rt = sp.add_parser('retry')
    rt.add_argument('names', nargs='+')
    a = ap.parse_args()
    RECORD_ARG[0] = a.record
    P = Paths(a)
    P.extra_scan = [os.path.abspath(x) for x in a.scan]
    if a.cmd == 'run':
        Scheduler(P, a).run()
    elif a.cmd == 'status':
        a.record = a.record
        cmd_status(P, a)
    elif a.cmd == 'collect':
        collect(P, a)
    elif a.cmd == 'add':
        cmd_add(P, a)
    elif a.cmd == 'cancel':
        cmd_cancel(P, a)
    elif a.cmd == 'retry':
        cmd_retry(P, a)


if __name__ == '__main__':
    main()
