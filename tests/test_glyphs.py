"""Glyphs the terminal draws itself: braille, powerline, sextants."""
import unittest

from harness import TerminalTest


class BuiltinGlyphs(TerminalTest):
    def draw(self, char):
        run = self.run_in_terminal(f'''
            reply = query('\\x1b]776;?\\x07', r'\\x07$')
            result['cell'] = [int(n) for n in reply.split(';')[1:3]]
            write({char!r})
            shot('glyph')
        ''', args=['-cr', 'black'])   # the cursor (next cell) out of the way
        w, h = run.result['cell']
        image = run.shots['glyph']
        return lambda fx, fy: sum(image.pixel(int(fx * w), int(fy * h))) > 300

    def test_braille_dot_1(self):
        lit = self.draw('⠁')
        self.assertTrue(lit(0.25, 0.125))    # dot 1: top left
        self.assertFalse(lit(0.75, 0.125))   # dot 4: top right
        self.assertFalse(lit(0.25, 0.875))   # dot 7: bottom left

    def test_braille_all_dots(self):
        lit = self.draw('⣿')
        for col in (0.25, 0.75):
            for row in (0.125, 0.375, 0.625, 0.875):
                self.assertTrue(lit(col, row), (col, row))
        self.assertFalse(lit(0.5, 0.25))     # between the dots

    def test_powerline_arrow(self):
        lit = self.draw('')
        self.assertTrue(lit(0.1, 0.5))
        self.assertTrue(lit(0.8, 0.5))       # the tip
        self.assertFalse(lit(0.8, 0.1))      # above the slope

    def test_sextant(self):
        lit = self.draw('\U0001fb00')        # the top left sixth
        self.assertTrue(lit(0.25, 0.15))
        self.assertFalse(lit(0.75, 0.15))
        self.assertFalse(lit(0.25, 0.5))


if __name__ == '__main__':
    unittest.main()
