"""The harness itself, and behaviour wrxvt inherits from rxvt-unicode."""
import shutil
import unittest

from harness import TerminalTest


class Basics(TerminalTest):
    def test_text_reaches_the_screen(self):
        run = self.run_in_terminal('''
            write('hello\\r\\nworld')
            screen = dump()
            result['rows'] = [r['text'].rstrip() for r in screen['rows'][:2]]
        ''')
        self.assertEqual(run.result['rows'], ['hello', 'world'])

    def test_del_is_ignored(self):
        run = self.run_in_terminal(r'''
            write('a\x7fb\x7f\x7fc')
            result['row'] = dump()['rows'][0]['text']
            result['cursor'] = dump()['cursor']
        ''')
        self.assertEqual(run.result['row'].rstrip(), 'abc')
        self.assertEqual(run.result['cursor'], [0, 3])

    def test_perl_sees_composed_characters(self):
        # x + combining acute (no precomposed form) is one composed cell;
        # extensions get its characters (they used to get "?")
        run = self.run_in_terminal(r'''
            write('x\u0301y \U0001F600')
            result['cells'] = dump()['rows'][0]['cells'][:5]
        ''')
        self.assertEqual(run.result['cells'], ['x\u0301', 'y', ' ', '\U0001F600', ''])

    def test_device_attributes_and_version(self):
        run = self.run_in_terminal(r'''
            result['da1'] = query('\x1b[c', r'c$')
            result['osc702'] = query('\x1b]702;?\x07', r'\x07$')
        ''')
        self.assertEqual(run.result['da1'], '\x1b[?62;4;22c')
        # programs read the major and first minor digit of the version
        self.assertEqual(run.result['osc702'], '\x1b]702;rxvt-unicode;wrxvt-test;9;3\x07')

    def test_screenshot(self):
        run = self.run_in_terminal(r'''
            write('\x1b[41m' + ' ' * 10 + '\x1b[m')
            shot('red')
        ''')
        image = run.shots['red']
        r, g, b = image.pixel(3, 3)
        self.assertGreater(r, 150)
        self.assertLess(g + b, 60)


class KeysymOptions(TerminalTest):
    @unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
    def test_keysym_from_the_command_line(self):
        # Xvfb has no resource database: the options must still work
        run = self.run_in_terminal(r'''
            import os, subprocess
            settle()
            termios.tcflush(0, termios.TCIFLUSH)
            window = os.environ['WINDOWID']
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F9'],
                           check=True, capture_output=True)
            result['typed'] = read_until('HELLO', timeout=3)
        ''', args=['-keysym.F9', 'string:HELLO'])
        self.assertEqual(run.result['typed'], 'HELLO')

    def test_exits_cleanly(self):
        # the options' database is the display's then, and is freed with it
        import os
        import subprocess
        from harness import binary
        done = subprocess.run([binary(), '-display', self.display.name, '-xrm', 'URxvt.a: b',
                               '-keysym.F9', 'string:x', '-e', 'true'],
                              env={**os.environ, 'DISPLAY': self.display.name},
                              capture_output=True, timeout=20)
        self.assertEqual(done.returncode, 0, done.stderr)


if __name__ == '__main__':
    unittest.main()
