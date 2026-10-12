"""OSC 52: programs setting and reading the selections."""
import base64
import shutil
import subprocess
import unittest

from harness import TerminalTest


def b64(text):
    return base64.b64encode(text.encode()).decode()


@unittest.skipUnless(shutil.which('xclip'), 'xclip not installed')
class Clipboard(TerminalTest):
    def test_set_clipboard_and_primary(self):
        run = self.run_in_terminal(f'''
            import subprocess, os
            write('\\x1b]52;c;{b64("copied – with ünïcode")}\\x07')
            write('\\x1b]52;p;{b64("primary text")}\\x1b\\\\')
            settle()
            def out(sel):
                return subprocess.run(['xclip', '-o', '-selection', sel],
                                      capture_output=True, text=True, timeout=5).stdout
            result['clipboard'] = out('clipboard')
            result['primary'] = out('primary')
        ''')
        self.assertEqual(run.result['clipboard'], 'copied – with ünïcode')
        self.assertEqual(run.result['primary'], 'primary text')

    def test_read_needs_insecure(self):
        run = self.run_in_terminal(f'''
            write('\\x1b]52;c;{b64("secret")}\\x07')
            result['reply'] = query('\\x1b]52;c;?\\x07', r'\\x1b\\\\$', timeout=1)
        ''')
        self.assertEqual(run.result['reply'], '')

    def test_read_own_selection(self):
        run = self.run_in_terminal(f'''
            write('\\x1b]52;c;{b64("mine")}\\x07')
            result['reply'] = query('\\x1b]52;c;?\\x07', r'\\x1b\\\\$')
        ''', args=['-insecure'])
        self.assertEqual(run.result['reply'], f'\x1b]52;c;{b64("mine")}\x1b\\')

    def test_read_another_programs_selection(self):
        run = self.run_in_terminal(r'''
            import subprocess
            subprocess.run(['xclip', '-i', '-selection', 'clipboard'], input=b'from xclip',
                           check=True, timeout=5)
            result['reply'] = query('\x1b]52;c;?\x07', r'\x1b\\$', timeout=5)
        ''', args=['-insecure'])
        self.assertEqual(run.result['reply'], f'\x1b]52;c;{b64("from xclip")}\x1b\\')

    def test_invalid_base64_is_ignored(self):
        run = self.run_in_terminal(f'''
            write('\\x1b]52;c;{b64("kept")}\\x07')
            write('\\x1b]52;c;not*base64\\x07')
            result['reply'] = query('\\x1b]52;c;?\\x07', r'\\x1b\\\\$')
        ''', args=['-insecure'])
        self.assertEqual(run.result['reply'], f'\x1b]52;c;{b64("kept")}\x1b\\')


if __name__ == '__main__':
    unittest.main()
