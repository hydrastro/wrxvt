"""SGR: attributes and colours, including the colon (ISO 8613-6) forms."""
import unittest

from harness import DEFAULT_BG, DEFAULT_FG, TerminalTest, cell, palette

class Sgr(TerminalTest):
    def cells(self, sequences):
        """Each item: (escape sequence, character); returns each char's cell."""
        text = ''.join(seq + ch for seq, ch in sequences)
        run = self.run_in_terminal(f'''
            write({text!r})
            screen = dump()
            result['screen'] = screen
        ''')
        screen = run.result['screen']
        return [cell(screen, 0, c) for c in range(len(sequences))]

    def test_curly_underline_is_an_underline_not_a_yellow_background(self):
        # "\e[4:3m" used to be read as "\e[43m"
        a, b, c = self.cells([('\x1b[4:3m', 'a'), ('\x1b[m\x1b[4m\x1b[4:0m', 'b'), ('\x1b[m\x1b[4:1m', 'c')])
        self.assertIn('Uline', a['attrs'])
        self.assertEqual(a['bg'], DEFAULT_BG)
        self.assertNotIn('Uline', b['attrs'])     # 4:0 = no underline
        self.assertIn('Uline', c['attrs'])

    def test_subparameters_do_not_swallow_the_next_argument(self):
        (a,) = self.cells([('\x1b[1;4:3;7m', 'a')])
        self.assertEqual({'Bold', 'Uline', 'RVid'} & a['attrs'], {'Bold', 'Uline', 'RVid'})

    def test_colours_with_colons(self):
        a, b, c, d = self.cells([('\x1b[38:5:196m', 'a'), ('\x1b[48:5:21m', 'b'),
                                 ('\x1b[m\x1b[38:2::255:0:0m', 'c'), ('\x1b[38:2:0:255:0m', 'd')])
        self.assertEqual(a['fg'], palette(196))
        self.assertEqual(b['bg'], palette(21))
        # a 24-bit colour gets a palette slot (an exact match is reused);
        # test_truecolour_is_drawn checks the colours on screen
        for x in (c, d):
            self.assertNotEqual(x['fg'], DEFAULT_FG)
        self.assertNotEqual(c['fg'], d['fg'])

    def test_semicolon_colours_still_work(self):
        a, b = self.cells([('\x1b[38;5;82m', 'a'), ('\x1b[38;2;10;20;30m', 'b')])
        self.assertEqual(a['fg'], palette(82))
        self.assertNotEqual(b['fg'], DEFAULT_FG)

    def test_palette_index_out_of_range_is_ignored(self):
        a, b = self.cells([('\x1b[38;5;300m', 'a'), ('\x1b[38:5:1000m', 'b')])
        self.assertEqual(a['fg'], DEFAULT_FG)
        self.assertEqual(b['fg'], DEFAULT_FG)

    def test_truecolour_is_drawn(self):
        run = self.run_in_terminal(r'''
            write('\x1b[48:2::255:0:0m      \x1b[48:2::0:0:255m      \x1b[m')
            shot('colours')
        ''')
        image = run.shots['colours']
        self.assertEqual(image.pixel(10, 8)[:3], (255, 0, 0))
        w = image.width // 60          # cell width (60 columns)
        self.assertEqual(image.pixel(6 * w + 10, 8)[:3], (0, 0, 255))


if __name__ == '__main__':
    unittest.main()
