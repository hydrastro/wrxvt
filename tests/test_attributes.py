"""Faint, strikethrough, overline and concealed text (SGR 2, 9, 53, 8)."""
import unittest

from harness import TerminalTest, cell

BLOCK = '█'      # a full block: every pixel of the cell in the text colour


def white(p):
    return min(p[:3]) > 200


class Attributes(TerminalTest):
    def dumped(self, text):
        run = self.run_in_terminal(f'''
            write({text!r})
            result['screen'] = dump()
        ''')
        return run.result['screen']

    def test_sgr_sets_and_resets_the_new_attributes(self):
        screen = self.dumped('\x1b[2ma\x1b[22mb\x1b[1;2mc\x1b[22md'
                             '\x1b[9me\x1b[29mf\x1b[53mg\x1b[55mh\x1b[8mi\x1b[28mj')
        attrs = [cell(screen, 0, c)['attrs'] for c in range(10)]
        self.assertEqual(attrs[0], {'Faint'})
        self.assertEqual(attrs[1], set())
        self.assertEqual(attrs[2], {'Bold', 'Faint'})
        self.assertEqual(attrs[3], set())                 # 22 ends bold and faint
        self.assertEqual(attrs[4], {'Strike'})
        self.assertEqual(attrs[5], set())
        self.assertEqual(attrs[6], {'Overline'})
        self.assertEqual(attrs[7], set())
        self.assertEqual(attrs[8], {'Conceal'})
        self.assertEqual(attrs[9], set())

    def test_sgr_0_resets_everything(self):
        screen = self.dumped('\x1b[2;9;53;8m\x1b[0mx')
        self.assertEqual(cell(screen, 0, 0)['attrs'], set())

    def test_erased_cells_lose_the_attributes(self):
        screen = self.dumped('\x1b[9;53mab\x1b[1G\x1b[K')
        self.assertEqual(cell(screen, 0, 0)['attrs'], set())

    def test_faint_text_is_drawn_halfway_to_the_background(self):
        run = self.run_in_terminal(f'''
            write('\\x1b[2m' + {BLOCK!r} * 3)
            shot('faint')
        ''')
        r, g, b = run.shots['faint'].pixel(5, 8)
        for v in (r, g, b):
            self.assertTrue(110 <= v <= 145, (r, g, b))

    def test_concealed_text_is_invisible(self):
        run = self.run_in_terminal(f'''
            write('\\x1b[8m' + {BLOCK!r} * 3)
            shot('conceal')
        ''')
        image = run.shots['conceal']
        self.assertFalse(any(white(p) for p in image.region(0, 0, 30, 18)))

    def test_strike_and_overline_are_drawn(self):
        run = self.run_in_terminal(r'''
            write('\x1b[9m' + ' ' * 4 + '\x1b[m\r\n\x1b[53m' + ' ' * 4)
            shot('lines')
        ''')
        image = run.shots['lines']
        height = image.height // 12
        def white_rows(top):
            return [y for y in range(top, top + height)
                    if all(white(image.pixel(x, y)) for x in range(2, 30))]
        strike, over = white_rows(0), white_rows(height)
        self.assertTrue(strike, 'no strike line')
        self.assertTrue(0.3 * height < strike[0] < 0.8 * height, strike)
        self.assertEqual(over[0], height)             # the cell's first pixel row


if __name__ == '__main__':
    unittest.main()
