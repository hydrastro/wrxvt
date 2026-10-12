"""Colour resources."""
import shutil
import subprocess
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


class ColorReset(TerminalTest):
    def test_reset_listed_and_all_palette_colours(self):
        run = self.run_in_terminal(r'''
            q = lambda n: query(f'\x1b]4;{n};?\x07', r'\x07$')
            write('\x1b]4;1;rgb:11/11/11;2;rgb:22/22/22;200;rgb:33/33/33\x07')
            write('\x1b]104;1\x07')
            result['one'] = [q(1), q(2), q(200)]
            write('\x1b]104\x07')
            result['all'] = [q(1), q(2), q(200)]
        ''', args=['-xrm', 'URxvt.color1: rgb:ab/00/00'])
        configured = ['\x1b]4;1;rgb:abab/0000/0000\x07', '\x1b]4;2;rgb:0000/cdcd/0000\x07',
                      '\x1b]4;200;rgb:ffff/0000/d7d7\x07']
        self.assertEqual(run.result['one'], [configured[0], '\x1b]4;2;rgb:2222/2222/2222\x07',
                                             '\x1b]4;200;rgb:3333/3333/3333\x07'])
        self.assertEqual(run.result['all'], configured)

    def test_reset_foreground_and_background(self):
        run = self.run_in_terminal(r'''
            write('\x1b]10;red\x07\x1b]11;blue\x07\x1b]110\x07\x1b]111\x07')
            result['fg'] = query('\x1b]10;?\x07', r'\x07$')
            result['bg'] = query('\x1b]11;?\x07', r'\x07$')
        ''')
        self.assertEqual(run.result['fg'], '\x1b]10;rgb:ffff/ffff/ffff\x07')
        self.assertEqual(run.result['bg'], '\x1b]11;rgb:0000/0000/0000\x07')


@unittest.skipUnless(shutil.which('xrdb'), 'xrdb not installed')
class FollowResources(TerminalTest):
    """The colours change with the X resources (xrdb -merge, a theme)."""

    def setUp(self):
        self.addCleanup(subprocess.run, ['xrdb', '-display', self.display.name, '-remove'])

    def test_new_colours_and_a_scheme_report(self):
        run = self.run_in_terminal(r'''
            import subprocess, time
            q = lambda seq: query(seq, r'\x07$')
            write('\x1b[?2031h')
            result['before'] = [q('\x1b]11;?\x07'), q('\x1b]10;?\x07'), q('\x1b]4;1;?\x07')]
            subprocess.run(['xrdb', '-merge'], input='URxvt.background: #102030\n'
                           'URxvt.foreground: #ff0000\n*.color1: #123456\n', text=True, check=True)
            result['report'] = read_until(r'\x1b\[\?997;\dn', 3)
            result['after'] = [q('\x1b]11;?\x07'), q('\x1b]10;?\x07'), q('\x1b]4;1;?\x07')]
            shot('after')
            # programs can still change them
            write('\x1b]4;1;rgb:11/22/33\x07')
            result['changed'] = q('\x1b]4;1;?\x07')
        ''', args=['-fg', 'white'], colors=False)   # (the background from the resources)
        r = run.result
        self.assertEqual(r['before'][0], '\x1b]11;rgb:ffff/ffff/ffff\x07')    # urxvt's white
        self.assertEqual(r['after'][0], '\x1b]11;rgb:1000/2000/3000\x07')
        self.assertEqual(r['report'], '\x1b[?997;1n')                   # dark now
        self.assertEqual(r['after'][1], '\x1b]10;rgb:ffff/ffff/ffff\x07')  # -fg stays
        self.assertEqual(r['after'][2], '\x1b]4;1;rgb:1200/3400/5600\x07')
        self.assertEqual(r['changed'], '\x1b]4;1;rgb:1111/2222/3333\x07')
        image = run.shots['after']                                       # repainted
        self.assertEqual(image.pixel(image.width - 3, image.height - 3), (0x10, 0x20, 0x30))


if __name__ == '__main__':
    unittest.main()
