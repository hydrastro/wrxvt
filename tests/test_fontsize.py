"""The font-size actions."""
import shutil
import unittest

from harness import TerminalTest


@unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
class FontSize(TerminalTest):
    def test_increase_decrease_reset(self):
        run = self.run_in_terminal(r'''
            import os, subprocess
            def cell():
                return [int(n) for n in query('\x1b]776;?\x07', r'\x07$').split(';')[1:3]]
            def key(*keys):
                settle()
                window = os.environ['WINDOWID']
                subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                                'windowfocus', window, 'key', *keys],
                               check=True, capture_output=True)
                settle()
            sizes = [cell()]
            key('F7', 'F7', 'F7', 'F7')
            sizes.append(cell())
            key('F8')
            sizes.append(cell())
            key('F9')
            sizes.append(cell())
            result['sizes'] = sizes
            result['font'] = query('\x1b]50;?\x07', r'\x07$')
        ''', args=['-insecure', '-keysym.F7', 'font-size:increase',
                   '-keysym.F8', 'font-size:decrease', '-keysym.F9', 'font-size:reset'])
        start, bigger, smaller, reset = run.result['sizes']
        self.assertGreater(bigger[1], start[1])
        self.assertLess(smaller[1], bigger[1])
        self.assertEqual(reset, start)
        self.assertIn('pixelsize=16', run.result['font'])

    def test_step_resource(self):
        run = self.run_in_terminal(r'''
            import os, subprocess
            settle()
            window = os.environ['WINDOWID']
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F7'], check=True, capture_output=True)
            settle()
            result['font'] = query('\x1b]50;?\x07', r'\x07$')
        ''', args=['-insecure', '-keysym.F7', 'font-size:increase', '-xrm', 'URxvt.font-size.step: 3'])
        self.assertIn('pixelsize=19', run.result['font'])


if __name__ == '__main__':
    unittest.main()
