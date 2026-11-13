# push.py LINE [LINE...]: insert jobs at the front of queue.txt (flock)
import sys, fcntl
with open('queue.lock', 'w') as lk:
    fcntl.flock(lk, fcntl.LOCK_EX)
    L = [l for l in open('queue.txt').read().split('\n') if l.strip()]
    open('queue.txt', 'w').write('\n'.join(sys.argv[1:] + L) + '\n')
