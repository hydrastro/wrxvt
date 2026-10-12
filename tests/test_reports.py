"""What the terminal reports about itself."""
import unittest

from harness import TerminalTest, version

ST = '\x1b\\'


def hexname(name):
    return name.encode().hex().upper()


class Reports(TerminalTest):
    def test_xtversion(self):
        run = self.run_in_terminal(r'''
            result['plain'] = query('\x1b[>q', r'\x1b\\$')
            result['zero'] = query('\x1b[>0q', r'\x1b\\$')
        ''')
        expected = f'\x1bP>|wrxvt({version()})\x1b\\'
        self.assertEqual(run.result['plain'], expected)
        self.assertEqual(run.result['zero'], expected)


class Decrqss(TerminalTest):
    def ask(self, setup, setting):
        run = self.run_in_terminal(f'''
            write({setup!r})
            result['reply'] = query('\\x1bP$q' + {setting!r} + '\\x1b\\\\', r'\\x1b\\\\$')
        ''')
        return run.result['reply']

    def test_default_rendition(self):
        self.assertEqual(self.ask('', 'm'), f'\x1bP1$r0m{ST}')

    def test_attributes_and_colours(self):
        reply = self.ask('\x1b[1;3;4:3;9;31;102m', 'm')
        self.assertEqual(reply, f'\x1bP1$r0;1;3;4:3;9;31;102m{ST}')

    def test_palette_and_24_bit_colours(self):
        reply = self.ask('\x1b[38;5;208;48:2::10:20:30;58:5:33m', 'm')
        self.assertEqual(reply, f'\x1bP1$r0;38:5:208;48:2::10:20:30;58:5:33m{ST}')

    def test_scroll_region(self):
        self.assertEqual(self.ask('\x1b[3;7r', 'r'), f'\x1bP1$r3;7r{ST}')

    def test_cursor_style(self):
        self.assertEqual(self.ask('\x1b[6 q', ' q'), f'\x1bP1$r6 q{ST}')
        self.assertEqual(self.ask('\x1b[3 q', ' q'), f'\x1bP1$r3 q{ST}')

    def test_unknown_setting(self):
        self.assertEqual(self.ask('', 'xyz'), f'\x1bP0$r{ST}')


class Xtgettcap(TerminalTest):
    def ask(self, *names):
        request = ';'.join(hexname(n) for n in names)
        run = self.run_in_terminal(f'''
            result['reply'] = query('\\x1bP+q{request}\\x1b\\\\',
                                    r'(\\x1b\\\\.*){{{len(names)}}}$')
        ''')
        return run.result['reply']

    def test_terminal_name_and_colours(self):
        reply = self.ask('TN', 'Co')
        self.assertEqual(reply,
                         f'\x1bP1+r{hexname("TN")}={hexname("rxvt-unicode-256color")}{ST}'
                         f'\x1bP1+r{hexname("Co")}={hexname("256")}{ST}')

    def test_boolean_and_unknown(self):
        reply = self.ask('Tc', 'nosuchcap')
        self.assertEqual(reply, f'\x1bP1+r{hexname("Tc")}{ST}\x1bP0+r{hexname("nosuchcap")}{ST}')

    def test_long_value(self):
        reply = self.ask('Setulc')
        value = bytes.fromhex(reply.split('=')[1][:-2]).decode()
        self.assertTrue(value.startswith('\x1b[58:2::'), value)


class DcsParsing(TerminalTest):
    def test_unknown_strings_are_swallowed(self):
        run = self.run_in_terminal(r'''
            write('a\x1bPxyz\x1b\\b\x1bP1;2|data\x07c\x1bP\x1b\\d')
            result['row'] = dump()['rows'][0]['text'].rstrip()
        ''')
        self.assertEqual(run.result['row'], 'abcd')


if __name__ == '__main__':
    unittest.main()
