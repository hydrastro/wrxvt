"""Underline styles (SGR 4:N, 21) and colours (SGR 58, 59)."""
import unittest

from harness import TerminalTest, cell, palette

SINGLE, DOUBLE, CURLY, DOTTED, DASHED = range(5)


class Underline(TerminalTest):
    geometry = '40x6'

    def cells(self, text, n):
        run = self.run_in_terminal(f'''
            write({text!r})
            result['screen'] = dump()
        ''')
        return [cell(run.result['screen'], 0, c) for c in range(n)]

    def test_styles(self):
        cells = self.cells('\x1b[4ma\x1b[4:2mb\x1b[4:3mc\x1b[4:4md\x1b[4:5me'
                           '\x1b[4:1mf\x1b[21mg\x1b[4:0mh\x1b[4:3m\x1b[24mi\x1b[4:9mj', 10)
        self.assertEqual([c['ul_style'] for c in cells],
                         [SINGLE, DOUBLE, CURLY, DOTTED, DASHED, SINGLE, DOUBLE, None, None, DASHED])

    def test_colours(self):
        cells = self.cells('\x1b[4;58:5:196ma\x1b[58;5;21mb\x1b[58:2::1:2:3mc\x1b[59md'
                           '\x1b[58:5:9m\x1b[0;4me', 5)
        self.assertEqual(cells[0]['ul_color'], palette(196))
        self.assertEqual(cells[1]['ul_color'], palette(21))
        self.assertIsNotNone(cells[2]['ul_color'])
        self.assertIsNone(cells[3]['ul_color'])       # 59: back to the text colour
        self.assertIsNone(cells[4]['ul_color'])       # SGR 0 resets it

    def underline_rows(self, image, x0, x1, row_height, colour=None):
        """Pixel rows in the first text row with underline pixels in x0..x1."""
        def lit(p):
            return p != (0, 0, 0) if colour is None else p == colour
        return [y for y in range(row_height // 2, row_height)
                if any(lit(image.pixel(x, y)) for x in range(x0, x1))]

    def test_drawing(self):
        run = self.run_in_terminal(r'''
            for seq in ('4', '4:2', '4:3', '4:4', '4:5'):
                write(f'\x1b[{seq}m' + ' ' * 6 + '\x1b[m\r\n')
            write('\x1b[4;58:2::255:0:0m' + ' ' * 6 + '\x1b[m\x1b[4;58;5;33m' + ' ' * 6)
            shot('styles')
        ''')
        image = run.shots['styles']
        h, w = image.height // 6, image.width // 40
        rows = [[y - n * h for y in range(n * h, (n + 1) * h)
                 if any(image.pixel(x, y) != (0, 0, 0) for x in range(2, 5 * w))] for n in range(5)]
        single, double, curly, dotted, dashed = rows
        self.assertTrue(single)
        self.assertEqual(len(single), len(dotted))                  # same thickness
        self.assertGreater(len(double), len(single))                # two lines
        self.assertGreater(min(double[1:]) - min(double), 0)
        self.assertGreater(len(curly), len(single))                 # a wave spans rows
        # dotted and dashed lines have gaps along the row
        for n in (3, 4):
            y = n * h + rows[n][0]
            lit = [image.pixel(x, y) != (0, 0, 0) for x in range(0, 6 * w)]
            self.assertIn(False, lit)
            self.assertIn(True, lit)
        # colours: red, and palette colour 33 (#0087ff) of the 256-colour palette
        y = 5 * h + single[0]
        self.assertEqual(image.pixel(w, y), (255, 0, 0))
        self.assertEqual(image.pixel(7 * w, y), (0, 0x87, 0xff))


if __name__ == '__main__':
    unittest.main()
