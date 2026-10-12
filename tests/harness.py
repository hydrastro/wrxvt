"""Test harness: runs wrxvt under Xvfb with a test program inside it.

Needs Xvfb, Python 3, ImageMagick's `import` (screenshots), xdotool
(window resizing) and a wrxvt built with perl. `make check` runs the
suite against the binary just built; by hand:

    cd tests && WRXVT=../src/rxvt WRXVT_LIBDIR=../src python3 -m unittest -v

Each test sends a small Python program (see inner.py) to run inside a
fresh terminal and gets back the dict it filled in, plus any screenshots
it asked for. Tests that need a missing tool are skipped.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import textwrap
import time
import unittest

HERE = Path(__file__).resolve().parent
FONT = os.environ.get('WRXVT_TEST_FONT', 'xft:DejaVu Sans Mono:pixelsize=16')


def binary():
    candidates = [os.environ.get('WRXVT'), shutil.which('wrxvt')]
    for path in candidates:
        if path and os.access(path, os.X_OK):
            return path
    return None


class Display:
    """One Xvfb for the whole test run."""

    instance = None

    def __init__(self):
        self.number = None
        self.process = None
        for number in range(91, 120):
            if os.path.exists(f'/tmp/.X{number}-lock'):
                continue
            process = subprocess.Popen(['Xvfb', f':{number}', '-screen', '0', '1280x1024x24',
                                        '-nolisten', 'tcp', '-noreset'],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for _ in range(100):
                if os.path.exists(f'/tmp/.X11-unix/X{number}'):
                    self.number, self.process = number, process
                    return
                if process.poll() is not None:
                    break
                time.sleep(0.05)
            process.kill()
        raise RuntimeError('cannot start Xvfb')

    @classmethod
    def get(cls):
        if cls.instance is None:
            cls.instance = cls()
        return cls.instance

    @property
    def name(self):
        return f':{self.number}'


class Image:
    """A screenshot as RGB pixels (read from PPM, no libraries needed)."""

    def __init__(self, path):
        data = Path(path).read_bytes()
        parts = data.split(maxsplit=4)
        assert parts[0] == b'P6', 'expected a binary PPM'
        self.width, self.height, maxval = int(parts[1]), int(parts[2]), int(parts[3])
        assert maxval == 255
        self.pixels = parts[4]

    def pixel(self, x, y):
        i = 3 * (y * self.width + x)
        return tuple(self.pixels[i:i + 3])

    def region(self, x0, y0, x1, y1):
        return [self.pixel(x, y) for y in range(y0, y1) for x in range(x0, x1)]

    def row_has(self, y, x0, x1, predicate):
        return any(predicate(self.pixel(x, y)) for x in range(x0, x1))


class Run:
    def __init__(self, result, shots, screen):
        self.result, self.shots, self.screen = result, shots, screen


class TerminalTest(unittest.TestCase):
    """Base class: self.run_in_terminal(code) -> Run."""

    geometry = '60x12'

    @classmethod
    def setUpClass(cls):
        if not binary():
            raise unittest.SkipTest('set WRXVT to an installed wrxvt (make check does)')
        if not shutil.which('Xvfb'):
            raise unittest.SkipTest('Xvfb not installed')
        cls.display = Display.get()

    def run_in_terminal(self, code, args=(), geometry=None, timeout=20, env=None):
        work = Path(tempfile.mkdtemp(prefix='wrxvt-test-'))
        self.addCleanup(shutil.rmtree, work, ignore_errors=True)
        code_path = work / 'code.py'
        code_path.write_text(textwrap.dedent(code))
        result_path = work / 'result.json'
        environment = {**os.environ, 'DISPLAY': self.display.name, 'LC_ALL': 'C.UTF-8',
                       'HOME': str(work), **(env or {})}
        environment.pop('WINDOWID', None)
        command = [binary(), '-display', self.display.name, '-geometry', geometry or self.geometry,
                   '-fn', FONT, '-bg', 'black', '-fg', 'white', '+sb', '-b', '0', '-w', '0',
                   '-perl-lib', str(HERE / 'perl'), '-pe', 'wrxvt-test', '-name', 'wrxvt-test',
                   *args, '-e', 'python3', str(HERE / 'inner.py'), str(result_path), str(code_path)]
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        shots = {}
        deadline = time.monotonic() + timeout
        try:
            while not result_path.exists():
                for request in work.glob('result.json.shot-*.req'):
                    name = request.name[len('result.json.shot-'):-len('.req')]
                    window = request.read_text().strip()
                    image = work / f'{name}.ppm'
                    subprocess.run(['import', '-display', self.display.name, '-window', window,
                                    '-depth', '8', f'ppm:{image}'], check=True, capture_output=True)
                    shots[name] = Image(image)
                    request.rename(request.with_suffix('.done'))
                if process.poll() is not None and not result_path.exists():
                    output = process.stdout.read().decode(errors='replace')
                    self.fail(f'wrxvt exited ({process.returncode}) before the test ended:\n{output}')
                if time.monotonic() > deadline:
                    self.fail('the test inside the terminal timed out')
                time.sleep(0.02)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            process.stdout.close()
        result = json.loads(result_path.read_text())
        if 'error' in result:
            self.fail('test code failed inside the terminal:\n' + result['error'])
        screens = sorted(work.glob('result.json.screen*'))
        screen = json.loads(screens[-1].read_text()) if screens else None
        return Run(result, shots, screen)


def cell(screen, row, col):
    """One cell of a dump(): its character, colours and attribute names."""
    r = screen['rows'][row]
    rend = r['rend'][col]
    attrs = {name for name, bit in screen['attr'].items() if bit is not None and rend & bit}
    info = {'char': r['text'][col], 'fg': r['fg'][col], 'bg': r['bg'][col], 'attrs': attrs}
    # underline style and colour (see RS_ulStyleShift, RS_ulColor in rxvt.h)
    info['ul_style'] = (rend >> 36) & 7 if 'Uline' in attrs else None
    info['ul_color'] = (rend >> 40) & 0x1ff if rend >> 39 & 1 else None
    return info


DEFAULT_FG, DEFAULT_BG = 0, 1


def palette(n):
    """The colour index of palette colour n (0-255) in a dump."""
    return n + 2


def version():
    """The version wrxvt reports in --help ("... v9.32, based on ...")."""
    import re
    out = subprocess.run([binary(), '--help'], capture_output=True, text=True,
                         env={**os.environ, 'DISPLAY': ''})
    return re.search(r' v(\d+\.\d+)', out.stdout + out.stderr).group(1)
