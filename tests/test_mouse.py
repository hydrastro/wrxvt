"""Mouse reports."""
import shutil
import textwrap
import unittest

from harness import TerminalTest

CLICK = r'''
import os, subprocess, time
def click(x, y):
    window = os.environ['WINDOWID']
    subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y),
                    'click', '1'], check=True)
'''


@unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
class Mouse(TerminalTest):
    def report(self, modes):
        run = self.run_in_terminal(textwrap.indent(CLICK, ' ' * 12) + f'''
            write('{modes}')
            settle()
            termios.tcflush(0, termios.TCIFLUSH)
            click(57, 41)
            result['reply'] = read_until(r'm$', timeout=3)
        ''')
        return run.result['reply']

    def test_sgr_cells(self):
        # 16 px DejaVu Sans Mono: cells are about 10x19 pixels
        reply = self.report('\\x1b[?1000h\\x1b[?1006h')
        self.assertRegex(reply, r'^\x1b\[<0;\d;\d+M\x1b\[<0;\d;\d+m$')

    def test_sgr_pixels(self):
        reply = self.report('\\x1b[?1000h\\x1b[?1016h')
        self.assertEqual(reply, '\x1b[<0;58;42M\x1b[<0;58;42m')

    def test_mode_query(self):
        run = self.run_in_terminal(r'''
            result['off'] = query('\x1b[?1016$p')
            write('\x1b[?1016h')
            result['on'] = query('\x1b[?1016$p')
        ''')
        self.assertEqual(run.result['off'], '\x1b[?1016;2$y')
        self.assertEqual(run.result['on'], '\x1b[?1016;1$y')


if __name__ == '__main__':
    unittest.main()
