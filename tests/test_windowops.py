"""Window operations (CSI Ps t): size reports and the title stack."""
import shutil
import unittest

from harness import TerminalTest


class WindowOps(TerminalTest):
    geometry = '30x7'

    def test_sizes(self):
        run = self.run_in_terminal(r'''
            cell = query('\x1b]776;?\x07', r'\x07$').split(';')
            result['fw'], result['fh'] = int(cell[1]), int(cell[2])
            result['16'] = query('\x1b[16t', r't$')
            result['14'] = query('\x1b[14t', r't$')
            result['18'] = query('\x1b[18t', r't$')
        ''', args=['-b', '5'])   # a border, which is not part of the text area
        fw, fh = run.result['fw'], run.result['fh']
        self.assertEqual(run.result['16'], f'\x1b[6;{fh};{fw}t')
        self.assertEqual(run.result['14'], f'\x1b[4;{7 * fh};{30 * fw}t')
        self.assertEqual(run.result['18'], '\x1b[8;7;30t')

    def test_title_report_ends_with_7_bit_st(self):
        run = self.run_in_terminal(r'''
            write('\x1b]2;héllo\x07')
            result['title'] = query('\x1b[21t', r'\x1b\\$')
        ''', args=['-insecure'])
        self.assertEqual(run.result['title'], '\x1b]lhéllo\x1b\\')

    @unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
    def test_title_stack(self):
        run = self.run_in_terminal(r'''
            import os, subprocess
            def title():
                settle()
                return subprocess.check_output(['xdotool', 'getwindowname', os.environ['WINDOWID']],
                                               text=True).strip()
            write('\x1b]2;shell\x07\x1b[22;0t\x1b]2;vim\x07\x1b[22;2t\x1b]2;help\x07')
            result['top'] = title()
            write('\x1b[23;2t')
            result['popped'] = title()
            write('\x1b[23;0t')
            result['restored'] = title()
            write('\x1b[23;0t')        # empty stack: nothing happens
            result['empty'] = title()
        ''')
        self.assertEqual([run.result[k] for k in ('top', 'popped', 'restored', 'empty')],
                         ['help', 'vim', 'shell', 'shell'])


if __name__ == '__main__':
    unittest.main()
