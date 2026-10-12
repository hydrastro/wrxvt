"""Throughput: how fast the terminal takes in output (make bench).

    python3 bench.py [--wrxvt PATH] [--compare OTHER] [--mb N]

Each workload is a file catted into a fresh terminal under Xvfb (80x24,
default scrollback); the time is from the start to the end of the cat,
so it includes parsing, the screen and drawing (jump scrolling, as
usual). Prints MB/s. --compare runs another terminal binary (urxvt,
an older wrxvt) on the same files, for comparison.
"""
import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time

from harness import Display, binary


def workloads(mb):
    """name -> bytes, each about mb megabytes"""
    size = int(mb * 1e6)
    rng = random.Random(1)
    words = ['terminal', 'wrxvt', 'output', 'scroll', 'buffer', 'colour', 'unicode', 'test']

    def fill(make):
        parts, n = [], 0
        while n < size:
            p = make()
            parts.append(p)
            n += len(p)
        return b''.join(parts)

    return {
        'ascii': fill(lambda: (' '.join(rng.choice(words) for _ in range(10)) + '\n').encode()),
        'sgr-256': fill(lambda: (''.join('\x1b[38;5;%dm%s ' % (rng.randrange(256), rng.choice(words))
                                         for _ in range(8)) + '\x1b[m\n').encode()),
        'truecolor': fill(lambda: (''.join('\x1b[48;2;%d;%d;%dm  ' % (rng.randrange(256), rng.randrange(256),
                                                                     rng.randrange(256))
                                           for _ in range(39)) + '\x1b[m\n').encode()),
        'unicode': fill(lambda: (''.join(rng.choice(['é', 'ü', 'ß', 'ж', 'λ', '中', '文', '→', '█', 'あ'])
                                         for _ in range(30)) + '\n').encode()),
        'scroll-region': fill(lambda: ('\x1b[5;20r\x1b[20;1H' + ' '.join(rng.choice(words) for _ in range(6))
                                       + '\n\x1b[r').encode()),
    }


def run(terminal, path, display):
    env = {**os.environ, 'DISPLAY': display}
    start = time.monotonic()
    subprocess.run([terminal, '-display', display, '-geometry', '80x24', '+sb',
                    '-fn', 'xft:DejaVu Sans Mono:pixelsize=14', '-e', 'cat', path],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
    return time.monotonic() - start


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--wrxvt', default=os.environ.get('WRXVT') or binary())
    parser.add_argument('--compare', help='another terminal binary to time on the same files')
    parser.add_argument('--mb', type=float, default=10, help='megabytes per workload (10)')
    args = parser.parse_args()

    if not args.wrxvt or not shutil.which('Xvfb'):
        sys.exit('needs a wrxvt (--wrxvt or WRXVT) and Xvfb')

    display = Display.get().name
    terminals = [('wrxvt', args.wrxvt)] + ([('compare', args.compare)] if args.compare else [])

    with tempfile.TemporaryDirectory() as tmp:
        print('%-14s' % 'workload' + ''.join('%14s' % name for name, _ in terminals) + '   (MB/s)')
        for name, data in workloads(args.mb).items():
            path = os.path.join(tmp, name)
            with open(path, 'wb') as f:
                f.write(data)
            rates = []
            for _, terminal in terminals:
                best = min(run(terminal, path, display) for _ in range(3))
                rates.append(len(data) / 1e6 / best)
            print('%-14s' % name + ''.join('%14.1f' % r for r in rates))


if __name__ == '__main__':
    main()
