"""The kitty keyboard protocol."""
import shutil
import unittest

from harness import TerminalTest

KEYS = r'''
import os, subprocess, time
def keys(*names):
    """What the terminal sends for these xdotool key names (or, starting
    with "keydown", an xdotool command line)."""
    settle()
    termios.tcflush(0, termios.TCIFLUSH)
    window = os.environ['WINDOWID']
    command = list(names) if names[0] == 'keydown' else ['key', '--delay', '20', *names]
    subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                    'windowfocus', window, *command],
                   check=True, capture_output=True)
    return read_until(r'(?!)', timeout=0.4)
'''


class Flags(TerminalTest):
    def test_push_pop_set_query(self):
        run = self.run_in_terminal(r'''
            q = lambda: query('\x1b[?u', r'u$')
            r = [q()]
            write('\x1b[>1u'); r.append(q())
            write('\x1b[>3u'); r.append(q())
            write('\x1b[<u'); r.append(q())
            write('\x1b[=5;1u'); r.append(q())
            write('\x1b[=2;2u'); r.append(q())
            write('\x1b[=1;3u'); r.append(q())
            write('\x1b[<10u'); r.append(q())    # popping too much: all off
            result['r'] = r
        ''')
        self.assertEqual(run.result['r'], [f'\x1b[?{n}u' for n in (0, 1, 3, 1, 5, 7, 6, 0)])

    def test_each_screen_has_its_flags(self):
        run = self.run_in_terminal(r'''
            q = lambda: query('\x1b[?u', r'u$')
            write('\x1b[>1u')
            write('\x1b[?1049h'); result['alt'] = q()
            write('\x1b[>8u');    result['alt2'] = q()
            write('\x1b[?1049l'); result['main'] = q()
        ''')
        self.assertEqual([run.result[k] for k in ('alt', 'alt2', 'main')],
                         ['\x1b[?0u', '\x1b[?8u', '\x1b[?1u'])


@unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
class Encoding(TerminalTest):
    def sent(self, flags, *names):
        run = self.run_in_terminal(KEYS + f'''
write('\\x1b[>{flags}u')
result['sent'] = keys(*{names!r})
''')
        return run.result['sent']

    def test_legacy_without_flags(self):
        self.assertEqual(self.sent(0, 'Escape', 'a', 'F1'), '\x1ba\x1b[11~')

    def test_disambiguate(self):
        cases = [
            ('Escape', '\x1b[27u'),
            ('a', 'a'),
            ('shift+a', 'A'),
            ('ctrl+a', '\x1b[97;5u'),
            ('alt+a', '\x1b[97;3u'),
            ('ctrl+shift+a', '\x1b[97;6u'),
            ('Return', '\r'),
            ('ctrl+Return', '\x1b[13;5u'),
            ('BackSpace', '\x7f'),
            ('shift+Tab', '\x1b[9;2u'),
            ('Up', '\x1b[A'),
            ('ctrl+Up', '\x1b[1;5A'),
            ('F1', '\x1b[P'),
            ('Insert', '\x1b[2~'),
            ('shift+Delete', '\x1b[3;2~'),
            ('ctrl+F1', '\x1b[1;5P'),
            ('alt+F5', '\x1b[15;3~'),
        ]
        for key, expected in cases:
            with self.subTest(key):
                self.assertEqual(self.sent(1, key), expected)

    def test_all_keys(self):
        self.assertEqual(self.sent(8, 'a', 'Return', 'BackSpace'), '\x1b[97u\x1b[13u\x1b[127u')

    def test_alternates_and_text(self):
        # shift press, a with its shifted form, shift release is not reported (no event types)
        self.assertEqual(self.sent(8 | 4, 'shift+a'), '\x1b[57441;2u\x1b[97:65;2u')
        self.assertEqual(self.sent(8 | 16, 'a'), '\x1b[97;;97u')

    def test_event_types(self):
        self.assertEqual(self.sent(8 | 2, 'a'), '\x1b[97u\x1b[97;1:3u')
        # text has no events without "all keys"; modified keys do
        self.assertEqual(self.sent(1 | 2, 'a'), 'a')
        self.assertEqual(self.sent(1 | 2, 'keydown', 'ctrl', 'keydown', 'a', 'keyup', 'a', 'keyup', 'ctrl'),
                         '\x1b[97;5u\x1b[97;5:3u')
        self.assertEqual(self.sent(1 | 2, 'Return'), '\r')

    def test_ctrl_shift_is_not_iso_14755(self):
        self.assertEqual(self.sent(1, 'ctrl+shift+u'), '\x1b[117;6u')


if __name__ == '__main__':
    unittest.main()
