"""The harness itself, and behaviour wrxvt inherits from rxvt-unicode."""
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

    def test_device_attributes_and_version(self):
        run = self.run_in_terminal(r'''
            result['da1'] = query('\x1b[c', r'c$')
            result['osc702'] = query('\x1b]702;?\x07', r'\x07$')
        ''')
        self.assertEqual(run.result['da1'], '\x1b[?1;2c')
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


if __name__ == '__main__':
    unittest.main()
