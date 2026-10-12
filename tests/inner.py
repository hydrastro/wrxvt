"""Runs inside a wrxvt window during the tests (see harness.py).

    python3 inner.py RESULT_PATH CODE_PATH

Executes the test code with these helpers in scope, then writes the
`result` dict as JSON to RESULT_PATH:

    write(s)              send text or escape sequences to the terminal
    query(seq, end, t)    send seq, return the reply up to `end` (a regex)
    dump()                the screen as the terminal sees it (text, rendition
                          and line flags per row), through the wrxvt-test
                          perl extension
    shot(name)            ask the harness for a screenshot, and wait for it
    result                dict for anything the test wants to check
"""
import json
import os
import re
import select
import sys
import termios
import time
import tty

RESULT, CODE = sys.argv[1], sys.argv[2]
result = {}


def write(s):
    data = s.encode('utf-8') if isinstance(s, str) else s
    while data:
        data = data[os.write(1, data):]


def read_until(end, timeout=2.0):
    """Bytes from the terminal until regex `end` matches, or the timeout."""
    pattern = re.compile(end.encode() if isinstance(end, str) else end, re.S)
    data = b''
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ready, _, _ = select.select([0], [], [], max(0.0, deadline - time.monotonic()))
        if ready:
            chunk = os.read(0, 4096)
            if not chunk:
                break
            data += chunk
            if pattern.search(data):
                break
    return data.decode('utf-8', 'replace')


def query(seq, end=r'\x1b\\|\x07|\x9c|[a-zA-Z~]$', timeout=2.0):
    """Send seq and return what the terminal answers."""
    termios.tcflush(0, termios.TCIFLUSH)
    write(seq)
    return read_until(end, timeout)


def wait_file(path, timeout=10.0):
    deadline = time.monotonic() + timeout
    while not os.path.exists(path):
        if time.monotonic() > deadline:
            raise TimeoutError(path)
        time.sleep(0.02)


def settle():
    """Let the terminal process and draw everything written so far: a
    device status report is answered only after earlier output."""
    query('\x1b[5n', r'\x1b\[0n', 2.0)
    time.sleep(0.15)


_dumps = 0


def dump():
    global _dumps
    settle()
    _dumps += 1
    path = f'{RESULT}.screen{_dumps}'
    write(f'\x1b]777;wrxvt-test;dump;{path}\x07')
    wait_file(path)
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def shot(name):
    settle()
    request = f'{RESULT}.shot-{name}'
    with open(request + '.req', 'w') as f:
        f.write(os.environ.get('WINDOWID', ''))
    wait_file(request + '.done', 30)


old = termios.tcgetattr(0)
tty.setraw(0)
try:
    with open(CODE, encoding='utf-8') as f:
        exec(compile(f.read(), CODE, 'exec'))
except Exception as error:  # report, don't hide the failure
    import traceback
    result['error'] = traceback.format_exc()
    result['exception'] = repr(error)
finally:
    termios.tcsetattr(0, termios.TCSADRAIN, old)
    with open(RESULT + '.tmp', 'w', encoding='utf-8') as f:
        json.dump(result, f)
    os.rename(RESULT + '.tmp', RESULT)
