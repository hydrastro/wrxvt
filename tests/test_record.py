"""Recording asciinema files (record:start, -record)."""
import json
import os
import shutil
import stat
import tempfile
import unittest

from harness import TerminalTest


def read_cast(path):
    with open(path, encoding='utf-8') as f:
        lines = f.read().splitlines()
    return json.loads(lines[0]), [json.loads(line) for line in lines[1:]]


class Record(TerminalTest):
    geometry = '30x6'

    def cast_path(self):
        d = tempfile.mkdtemp(prefix='wrxvt-cast-')
        self.addCleanup(shutil.rmtree, d, ignore_errors=True)
        return os.path.join(d, 'demo.cast')

    def test_record_from_the_start(self):
        path = self.cast_path()
        run = self.run_in_terminal(r'''
            import os, subprocess, time
            write('\x1b]2;my demo\x07')
            write('hello \x1b[31mred\x1b[m\r\n')
            settle()
            time.sleep(0.3)
            # a character cut in two by the pty
            write(b'caf\xc3')
            time.sleep(0.3)             # (no settle: its query would come in between)
            write(b'\xa9 \xe4\xb8\xad\r\n')
            settle()
            out = subprocess.run(['xprop', '-id', os.environ['WINDOWID'], '_NET_WM_NAME'],
                                 capture_output=True, text=True).stdout
            result['title'] = out
        ''', args=['-record', path, '-xrm', 'URxvt.notifyCommand:'])
        header, events = read_cast(path)
        self.assertEqual(header['version'], 2)
        self.assertEqual((header['width'], header['height']), (30, 6))
        self.assertTrue(header['env']['TERM'].startswith('rxvt-unicode'))
        self.assertEqual(oct(stat.S_IMODE(os.stat(path).st_mode)), '0o600')
        self.assertTrue(all(e[1] == 'o' for e in events))
        times = [e[0] for e in events]
        self.assertEqual(times, sorted(times))
        output = ''.join(e[2] for e in events)
        self.assertIn('hello \x1b[31mred\x1b[m\r\n', output)
        self.assertIn('café 中\r\n', output)
        self.assertNotIn('�', output)
        self.assertGreater(times[-1] - times[0], 0.2)
        if shutil.which('xprop'):
            self.assertIn('[rec] my demo', run.result['title'])

    def test_start_in_the_middle(self):
        home = tempfile.mkdtemp(prefix='wrxvt-home-')
        self.addCleanup(shutil.rmtree, home, ignore_errors=True)
        run = self.run_in_terminal(r'''
            import os, subprocess
            def key(k):
                settle()
                window = os.environ['WINDOWID']
                subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                                'windowfocus', window, 'key', k], check=True, capture_output=True)
            write('before \x1b[1;44mblue\x1b[m\r\nsecond line')
            key('F9')                   # start
            write('\r\nafter')
            key('F9')                   # stop
            write('\r\nnot recorded')
            settle()
        ''', args=['-keysym.F9', 'record:toggle', '-xrm', f'URxvt.recordDirectory: {home}',
                   '-xrm', 'URxvt.notifyCommand:'])
        casts = [f for f in os.listdir(home) if f.endswith('.cast')]
        self.assertEqual(len(casts), 1)
        self.assertRegex(casts[0], r'^wrxvt-\d{8}-\d{6}\.cast$')
        header, events = read_cast(os.path.join(home, casts[0]))
        first = events[0][2]
        # the first frame draws the screen as it was
        self.assertTrue(first.startswith('\x1b[H\x1b[2J'))
        self.assertIn('before \x1b[0;1;44mblue\x1b[m\r\nsecond line', first)
        self.assertTrue(first.endswith('\x1b[2;12H'))
        output = ''.join(e[2] for e in events)
        self.assertIn('after', output)
        self.assertNotIn('not recorded', output)

    def test_resize(self):
        path = self.cast_path()
        self.run_in_terminal(r'''
            import os, subprocess, time
            settle()
            subprocess.run(['xdotool', 'windowsize', '--usehints', os.environ['WINDOWID'], '40', '8'],
                           check=True, capture_output=True)
            for _ in range(50):
                if query('\x1b[18t', r't').startswith('\x1b[8;8;40t'):
                    break
                time.sleep(0.05)
            write('resized')
            settle()
        ''', args=['-record', path, '-xrm', 'URxvt.notifyCommand:'])
        header, events = read_cast(path)
        resizes = [e[2] for e in events if e[1] == 'r']
        self.assertEqual(resizes, ['40x8'])
        r = [i for i, e in enumerate(events) if e[1] == 'r'][0]
        self.assertIn('resized', ''.join(e[2] for e in events[r:]))


if __name__ == '__main__':
    unittest.main()
