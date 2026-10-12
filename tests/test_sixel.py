"""Sixel graphics."""
import unittest

from harness import TerminalTest


def red(p):
    return p[0] > 200 and p[1] + p[2] < 60


def square(width, bands, rgb='100;0;0', start='\x1bPq'):
    """A sixel picture: width x 6*bands pixels of one colour."""
    return (start + f'#1;2;{rgb}#1' + '-'.join(f'!{width}~' for _ in range(bands))
            + '\x1b\\')


class Sixel(TerminalTest):
    geometry = '40x10'

    def cell_size(self):
        return r'''
            fw, fh = [int(n) for n in query('\x1b]776;?\x07', r'\x07$').split(';')[1:3]]
            result['cell'] = [fw, fh]
        '''

    def test_picture_at_the_cursor(self):
        run = self.run_in_terminal(self.cell_size() + f'''
            write('ab' + {square(30, 6)!r} + 'X')
            result['cursor'] = dump()['cursor']
            shot('picture')
        ''')
        fw, fh = run.result['cell']
        image = run.shots['picture']
        # the picture starts after "ab", at the top
        r, g, b = image.pixel(2 * fw + 5, 5)
        self.assertGreater(r, 200)
        self.assertLess(g + b, 40)
        self.assertEqual(image.pixel(2 * fw + 35, 5)[0] < 100, True)  # right of it: not red
        # the cursor went below the picture, to the column it starts in
        rows = -(-36 // fh)
        self.assertEqual(run.result['cursor'], [rows, 3])

    def test_scrolls_like_text(self):
        run = self.run_in_terminal(self.cell_size() + f'''
            write({square(20, 3)!r})
            write('\\r\\n' * 12)         # scroll it off the top
            shot('gone')
        ''')
        image = run.shots['gone']
        self.assertFalse(any(red(p) for p in image.region(0, 0, 40, 40)))

    def test_moves_up_with_the_text(self):
        run = self.run_in_terminal(self.cell_size() + f'''
            write({square(20, 6)!r})    # 36 pixels: two rows at the top
            write('\\x1b[10H\\n')        # the last row, and one line more
            shot('moved')
        ''')
        fw, fh = run.result['cell']
        image = run.shots['moved']
        self.assertTrue(red(image.pixel(5, 5)))              # the rest of it is at the top
        self.assertFalse(red(image.pixel(5, fh + 2)))        # one row less of it

    def test_text_overwrites_tiles(self):
        run = self.run_in_terminal(self.cell_size() + f'''
            write({square(20, 3)!r})
            write('\\x1b[H' + ' ' * 40)
            shot('erased')
        ''')
        image = run.shots['erased']
        self.assertFalse(any(red(p) for p in image.region(0, 0, 20, 10)))

    def test_hls_and_raster_attributes(self):
        # hue 120 in the VT340's HLS is red; the raster attributes give the size
        run = self.run_in_terminal(self.cell_size() + r'''
            write('\x1bPq"1;1;24;12#2;1;120;50;100#2!24~-!24~\x1b\\')
            shot('hls')
        ''')
        r, g, b = run.shots['hls'].pixel(3, 3)
        self.assertGreater(r, 200)
        self.assertLess(g + b, 40)

    def test_device_attributes_and_graphics_query(self):
        run = self.run_in_terminal(r'''
            result['da'] = query('\x1b[c', r'c$')
            result['colors'] = query('\x1b[?1;1;0S', r'S$')
        ''')
        self.assertIn(';4', run.result['da'])
        self.assertEqual(run.result['colors'], '\x1b[?1;0;1024S')


if __name__ == '__main__':
    unittest.main()
