"""Colour resources."""
import unittest

from harness import TerminalTest


class PaletteResources(TerminalTest):
    def test_color16_to_255(self):
        run = self.run_in_terminal(r'''
            for n in (16, 208, 255):
                result[str(n)] = query(f'\x1b]4;{n};?\x07', r'\x07$')
        ''', args=['-xrm', 'URxvt.color16: #102030', '-xrm', 'URxvt.color208: rgb:ab/cd/ef',
                   '-xrm', 'URxvt.color255: white'])
        # X reads #RRGGBB as the high bits (#10 is 0x1000)
        self.assertEqual(run.result['16'], '\x1b]4;16;rgb:1000/2000/3000\x07')
        self.assertEqual(run.result['208'], '\x1b]4;208;rgb:abab/cdcd/efef\x07')
        self.assertEqual(run.result['255'], '\x1b]4;255;rgb:ffff/ffff/ffff\x07')

    def test_command_line_option(self):
        run = self.run_in_terminal(r'''
            result['100'] = query('\x1b]4;100;?\x07', r'\x07$')
        ''', args=['--color100', '#010203'])
        self.assertEqual(run.result['100'], '\x1b]4;100;rgb:0100/0200/0300\x07')


if __name__ == '__main__':
    unittest.main()
