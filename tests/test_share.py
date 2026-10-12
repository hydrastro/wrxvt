"""Sharing the terminal on a web page (share:start, -share)."""
import shutil
import textwrap
import unittest

from harness import TerminalTest

# runs inside the terminal: a small HTTP and WebSocket client, and the
# links read from the window (xprop _WRXVT_SHARE)
CLIENT = r'''
import base64, hashlib, json, os, socket, struct, subprocess, time

def links():
    deadline = time.monotonic() + 5
    while True:
        out = subprocess.run(['xprop', '-id', os.environ['WINDOWID'], '_WRXVT_SHARE'],
                             capture_output=True, text=True).stdout
        if '"' in out:
            value = out.split('"', 1)[1].rsplit('"', 1)[0]
            return value.split('\\n')
        if time.monotonic() > deadline:
            raise TimeoutError('no _WRXVT_SHARE: ' + out)
        time.sleep(0.05)

def title():
    out = subprocess.run(['xprop', '-id', os.environ['WINDOWID'], '_NET_WM_NAME'],
                         capture_output=True, text=True).stdout
    return out.split('"', 1)[1].rsplit('"', 1)[0] if '"' in out else out

def split(url):
    rest = url[len('http://'):]
    hostport, key = rest.split('/#')
    host, port = hostport.rsplit(':', 1)
    return host, int(port), key

def http(url, path, headers=''):
    host, port, _ = split(url)
    s = socket.create_connection((host, port), 5)
    s.sendall(f'GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\n{headers}\r\n'.encode())
    data = b''
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        data += chunk
    s.close()
    head, _, body = data.partition(b'\r\n\r\n')
    return head.decode(), body

class WS:
    def __init__(self, url, origin=True):
        host, port, self.key = split(url)
        self.s = socket.create_connection((host, port), 5)
        nonce = base64.b64encode(os.urandom(16)).decode()
        extra = f'Origin: http://{host}:{port}\r\n' if origin is True else (origin or '')
        self.s.sendall((f'GET /ws HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n'
                        f'Connection: Upgrade\r\nSec-WebSocket-Key: {nonce}\r\n'
                        f'Sec-WebSocket-Version: 13\r\n{extra}\r\n').encode())
        self.buf = b''
        while b'\r\n\r\n' not in self.buf:
            chunk = self.s.recv(4096)
            if not chunk:
                break
            self.buf += chunk
        head, _, self.buf = self.buf.partition(b'\r\n\r\n')
        self.head = head.decode()
        accept = base64.b64encode(hashlib.sha1(
            (nonce + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        self.ok = self.head.startswith('HTTP/1.1 101') and f'Sec-WebSocket-Accept: {accept}' in self.head

    def send(self, obj, opcode=1):
        data = json.dumps(obj).encode() if not isinstance(obj, bytes) else obj
        mask = os.urandom(4)
        n = len(data)
        head = bytes([0x80 | opcode])
        if n < 126:
            head += bytes([0x80 | n])
        elif n < 65536:
            head += bytes([0x80 | 126]) + struct.pack('>H', n)
        else:
            head += bytes([0x80 | 127]) + struct.pack('>Q', n)
        self.s.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _need(self, n):
        while len(self.buf) < n:
            chunk = self.s.recv(65536)
            if not chunk:
                raise EOFError
            self.buf += chunk

    def recv(self):
        """the next message: a dict, or ('close', code)"""
        self._need(2)
        opcode, n = self.buf[0] & 15, self.buf[1] & 127
        pos = 2
        if n == 126:
            self._need(4); n = struct.unpack('>H', self.buf[2:4])[0]; pos = 4
        elif n == 127:
            self._need(10); n = struct.unpack('>Q', self.buf[2:10])[0]; pos = 10
        self._need(pos + n)
        data, self.buf = self.buf[pos:pos + n], self.buf[pos + n:]
        if opcode == 8:
            return ('close', struct.unpack('>H', data[:2])[0] if len(data) >= 2 else None)
        return json.loads(data)

    def login(self):
        self.send({'type': 'auth', 'token': self.key})
        return self.recv()

    def screen(self, want=None, timeout=5):
        """the screen as text rows, after the messages that come within the
        time (or until a row contains `want`)"""
        self.s.settimeout(0.3)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                m = self.recv()
            except (socket.timeout, TimeoutError):
                if want is None:
                    break
                continue
            if isinstance(m, tuple):
                break
            if m.get('type') == 'screen':
                self.rows = getattr(self, 'rows', {})
                for i, runs in m['lines']:
                    self.rows[i] = runs
                self.cursor = m['cursor']
                if 'title' in m:
                    self.title = m['title']
                if want and any(want in ''.join(r[0] for r in runs) for runs in self.rows.values()):
                    break
        self.s.settimeout(5)
        return {i: ''.join(r[0] for r in runs) for i, runs in self.rows.items()}
'''


@unittest.skipUnless(shutil.which('xprop'), 'xprop not installed')
class Share(TerminalTest):
    geometry = '40x8'

    def share(self, code, args=()):
        return self.run_in_terminal(CLIENT + textwrap.dedent(code), args=['-share', '-xrm', 'URxvt.notifyCommand:',
                                                         *args])

    def test_the_page(self):
        run = self.share(r'''
            view, control = links()
            result['view'], result['control'] = view, control
            head, body = http(view, '/')
            result['head'], result['page'] = head, body.decode()
            result['missing'] = http(view, '/etc/passwd')[0].split('\r\n')[0]
        ''')
        r = run.result
        self.assertRegex(r['view'], r'^http://127\.0\.0\.1:\d+/#[0-9a-f]{32}$')
        self.assertRegex(r['control'], r'^http://127\.0\.0\.1:\d+/#[0-9a-f]{32}$')
        self.assertNotEqual(r['view'], r['control'])
        self.assertIn('200 OK', r['head'])
        self.assertIn("default-src 'none'", r['head'])
        self.assertIn('new WebSocket', r['page'])
        self.assertNotIn(r['view'].split('#')[1], r['page'])   # the page holds no key
        self.assertEqual(r['missing'], 'HTTP/1.1 404 Not Found')

    def test_watching(self):
        run = self.share(r'''
            write('\x1b[2J\x1b[H\x1b]2;my title\x07plain \x1b[1;31mred\x1b[m été 中!')
            view, control = links()
            ws = WS(view)
            result['handshake'] = ws.ok
            result['hello'] = ws.login()
            rows = ws.screen('red')
            result['row0'] = rows[0]
            result['runs'] = ws.rows[0]
            result['cursor'] = ws.cursor
            result['page_title'] = ws.title
            result['title'] = title()
            result['report'] = query('\x1b[21t')
            # a watcher can't type
            ws.send({'type': 'text', 'data': 'nope'})
            result['typed'] = read_until('nope', 0.5)
            write(' more')
            result['row0_later'] = ws.screen('more')[0]
        ''', args=['-insecure'])   # (insecure: title reports)
        r = run.result
        self.assertTrue(r['handshake'])
        self.assertEqual(r['hello']['type'], 'hello')
        self.assertEqual(r['hello']['role'], 'view')
        self.assertEqual((r['hello']['cols'], r['hello']['rows']), (40, 8))
        self.assertEqual(r['row0'].rstrip(), 'plain red été 中!')
        red = [run for run in r['runs'] if run[0] == 'red'][0]
        self.assertEqual(red[3] & 1, 1)                 # bold
        self.assertEqual(red[1], '#ff0000')             # bold: urxvt's bright red
        wide = [run for run in r['runs'] if run[0] == '中'][0]
        self.assertEqual(wide[3] & 128, 128)
        self.assertEqual(r['cursor'][:2], [0, 17])
        self.assertEqual(r['page_title'], 'my title')
        self.assertEqual(r['title'], '[shared, 1 watching] my title')
        self.assertIn('my title', r['report'])
        self.assertNotIn('shared', r['report'])         # programs see their own title
        self.assertEqual(r['typed'], '')
        self.assertEqual(r['row0_later'].rstrip(), 'plain red été 中! more')

    def test_control(self):
        run = self.share(r'''
            write('\x1b[?1h')            # application cursor keys
            view, control = links()
            ws = WS(control)
            result['role'] = ws.login()['role']
            ws.send({'type': 'text', 'data': 'hé'})
            ws.send({'type': 'key', 'key': 'ArrowUp', 'shift': False, 'ctrl': False, 'alt': False})
            ws.send({'type': 'key', 'key': 'ArrowLeft', 'shift': False, 'ctrl': True, 'alt': False})
            ws.send({'type': 'key', 'key': 'Delete', 'shift': False, 'ctrl': False, 'alt': False})
            ws.send({'type': 'key', 'key': 'Enter', 'shift': False, 'ctrl': False, 'alt': False})
            result['typed'] = read_until('\r', 2)
            write('\x1b[?2004h')         # bracketed paste
            # (one line: confirm-paste would ask about several, as for any paste)
            ws.send({'type': 'paste', 'data': 'pasted text'})
            result['pasted'] = read_until('201~', 2)
        ''')
        r = run.result
        self.assertEqual(r['role'], 'control')
        self.assertEqual(r['typed'], 'hé\x1bOA\x1b[1;5D\x1b[3~\r')
        self.assertEqual(r['pasted'], '\x1b[200~pasted text\x1b[201~')

    def test_mouse(self):
        run = self.share(r'''
            view, control = links()
            ws = WS(control)
            ws.login()
            # a program that wants the mouse gets clicks and the wheel
            write('\x1b[?1000h\x1b[?1006h')
            settle()
            ws.send({'type': 'mouse', 'button': 1, 'row': 2, 'col': 3})
            ws.send({'type': 'mouse', 'button': 5, 'row': 0, 'col': 0})
            ws.send({'type': 'mouse', 'button': 4, 'row': 99, 'col': -5})   # clamped
            result['reported'] = read_until('64;1;8M', 2)
            # one that doesn't, on the alternate screen: the wheel is arrow keys
            write('\x1b[?1000l\x1b[?1006l\x1b[?1049h')
            settle()
            ws.send({'type': 'mouse', 'button': 5, 'row': 1, 'col': 1})
            ws.send({'type': 'mouse', 'button': 1, 'row': 1, 'col': 1})   # nothing
            result['scrolled'] = read_until('BBB', 2)
            # and on the normal screen, nothing (the page has no scrollback)
            write('\x1b[?1049l')
            settle()
            ws.send({'type': 'mouse', 'button': 4, 'row': 1, 'col': 1})
            result['primary'] = read_until('never', 0.5)
            # a watcher's mouse does nothing
            write('\x1b[?1000h\x1b[?1006h')
            settle()
            watcher = WS(view)
            watcher.login()
            watcher.send({'type': 'mouse', 'button': 1, 'row': 1, 'col': 1})
            result['watcher'] = read_until('never', 0.5)
        ''')
        r = run.result
        self.assertEqual(r['reported'], '\x1b[<0;4;3M\x1b[<0;4;3m\x1b[<65;1;1M\x1b[<64;1;8M')
        self.assertEqual(r['scrolled'], '\x1b[B\x1b[B\x1b[B')
        self.assertEqual(r['primary'], '')
        self.assertEqual(r['watcher'], '')

    def test_control_action_starts_sharing(self):
        run = self.run_in_terminal(CLIENT + textwrap.dedent(r'''
            import subprocess
            settle()
            window = os.environ['WINDOWID']
            result['before'] = subprocess.run(['xprop', '-id', window, '_WRXVT_SHARE'],
                                              capture_output=True, text=True).stdout
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F9'], check=True, capture_output=True)
            settle()
            view, control = links()
            result['control'] = control
            result['clipboard'] = subprocess.run(['xclip', '-o', '-selection', 'clipboard'],
                                                 capture_output=True, text=True, timeout=5).stdout
        '''), args=['-keysym.F9', 'share:control', '-xrm', 'URxvt.notifyCommand:'])
        r = run.result
        self.assertIn('not found', r['before'])
        self.assertEqual(r['clipboard'], r['control'])

    def test_wrong_key_and_foreign_pages(self):
        run = self.share(r'''
            write('\x1b]2;t\x07')
            view, control = links()
            ws = WS(view.split('#')[0] + '#' + '0' * 32)
            result['wrong'] = [ws.login(), ws.recv()]
            other = WS(view, origin='Origin: http://evil.example\r\n')
            result['foreign'] = other.head.split('\r\n')[0]
            result['title'] = title()
        ''')
        r = run.result
        self.assertEqual(r['wrong'][0]['type'], 'error')
        self.assertEqual(r['wrong'][1], ['close', 1008])
        self.assertEqual(r['foreign'], 'HTTP/1.1 400 Bad Request')
        self.assertEqual(r['title'], '[shared] t')

    def test_stop(self):
        run = self.share(r'''
            import subprocess
            write('\x1b]2;t\x07')
            view, control = links()
            ws = WS(view)
            ws.login()
            ws.screen()
            settle()
            window = os.environ['WINDOWID']
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F9'], check=True, capture_output=True)
            m = ws.recv()
            while m.get('type') == 'screen':
                m = ws.recv()
            result['bye'] = m
            result['after'] = subprocess.run(['xprop', '-id', window, '_WRXVT_SHARE'],
                                             capture_output=True, text=True).stdout
            result['title'] = title()
            try:
                http(view, '/')
                result['still'] = True
            except OSError:
                result['still'] = False
        ''', args=['-keysym.F9', 'share:stop'])
        r = run.result
        self.assertEqual(r['bye']['type'], 'bye')
        self.assertIn('not found', r['after'])
        self.assertEqual(r['title'], 't')
        self.assertFalse(r['still'])

    def test_only_local_addresses_without_insecure(self):
        run = self.run_in_terminal(r'''
            import os, subprocess, time
            time.sleep(0.5)
            result['prop'] = subprocess.run(['xprop', '-id', os.environ['WINDOWID'], '_WRXVT_SHARE'],
                                            capture_output=True, text=True).stdout
        ''', args=['-share', '-xrm', 'URxvt.shareAddress: 0.0.0.0', '-xrm', 'URxvt.notifyCommand:'])
        self.assertIn('not found', run.result['prop'])


if __name__ == '__main__':
    unittest.main()
