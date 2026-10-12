"""Control strings (OSC, DCS, APC, PM, SOS) of any length."""
import shutil
import unittest

from harness import TerminalTest


class Strings(TerminalTest):
    def first_row(self, data, extra=''):
        run = self.run_in_terminal(f'''
            import time
            data = {data!r}
            # in pieces, so the string spans many reads
            for i in range(0, len(data), 3000):
                write(data[i:i + 3000])
                time.sleep(0.001)
            {extra}
            result['row'] = dump()['rows'][0]['text'].rstrip()
        ''', timeout=60)
        return run

    @unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
    def test_long_title(self):
        title = 'T' * 100000 + 'END'
        run = self.first_row('a\x1b]2;' + title + '\x07b', extra='''
            import os, subprocess
            result['title'] = subprocess.check_output(
                ['xdotool', 'getwindowname', os.environ['WINDOWID']], text=True).strip()
        ''')
        self.assertEqual(run.result['row'], 'ab')
        self.assertEqual(run.result['title'], 'T' * 100000 + 'END')

    def test_apc_pm_sos_are_ignored(self):
        run = self.first_row('a\x1b_Gf=100;' + 'QUFB' * 20000 + '\x1b\\b\x1b^private\x1b\\c\x1bXstring\x1b\\d')
        self.assertEqual(run.result['row'], 'abcd')

    def test_too_long_is_swallowed(self):
        # longer than STRING_MAX (4 Mi characters): dropped, not printed
        run = self.first_row('a\x1b]2;' + 'x' * (4 * 1024 * 1024 + 10) + '\x1b\\b')
        self.assertEqual(run.result['row'], 'ab')

    def test_long_dcs_is_answered(self):
        names = ';'.join(['544E'] * 2000)   # "TN" 2000 times, 10 kB
        run = self.run_in_terminal(f'''
            result['reply'] = query('\\x1bP+q{names}\\x1b\\\\', r'(?:\\x1bP1\\+r544E=[0-9A-F]+\\x1b\\\\){{2000}}$', timeout=10)
        ''')
        self.assertEqual(run.result['reply'].count('\x1bP1+r544E='), 2000)


if __name__ == '__main__':
    unittest.main()
