"""The scrollback in a pager (scrollback:pager)."""
import os
import shutil
import tempfile
import time
import unittest

from harness import TerminalTest


@unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
class Pager(TerminalTest):
    geometry = '30x5'

    def test_whole_scrollback_with_colours(self):
        d = tempfile.mkdtemp(prefix='wrxvt-pager-')
        self.addCleanup(shutil.rmtree, d, ignore_errors=True)
        out, mode = os.path.join(d, 'out'), os.path.join(d, 'mode')
        self.run_in_terminal(r'''
            import os, subprocess
            for i in range(1, 41):
                write(f'line {i}\r\n')
            write('\x1b[1;31mred\x1b[m and ' + 'x' * 40 + '\r\n$ ')
            settle()
            window = os.environ['WINDOWID']
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F9'], check=True, capture_output=True)
            settle()
        ''', args=['-sl', '100', '-keysym.F9', 'scrollback:pager',
                   '-xrm', f'URxvt.scrollbackCommand: f() {{ stat -c %a "$1" >{mode}; cat "$1" >{out}; '
                   'rm -f "$1"; }; f'])
        for _ in range(100):
            if os.path.exists(out) and os.path.getsize(out):
                break
            time.sleep(0.05)
        with open(out, encoding='utf-8') as f:
            text = f.read()
        lines = text.split('\n')
        self.assertEqual(lines[0], 'line 1')
        self.assertEqual(lines[39], 'line 40')
        # colours as SGR, and the wrapped line in one piece
        self.assertEqual(lines[40], '\x1b[0;1;31mred\x1b[0m and ' + 'x' * 40)
        self.assertEqual(lines[41], '$')
        with open(mode) as f:
            self.assertEqual(f.read().strip(), '600')


if __name__ == '__main__':
    unittest.main()
