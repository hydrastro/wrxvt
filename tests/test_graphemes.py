"""Grapheme clusters (mode 2027): a cluster in one cell, two if wide."""
import unittest

from harness import TerminalTest

FAMILY = '\U0001F468‍\U0001F469‍\U0001F467'   # man ZWJ woman ZWJ girl
KSSI = 'क्षि'                         # Devanagari ksa + i: one conjunct
FLAG = '\U0001F1EE\U0001F1F9'                             # I + T: the Italian flag
HEART = '❤️'                                    # heart, emoji form


def cells(row):
    """the cells of a dumped row, up to the last non-blank one"""
    c = list(row['cells'])
    while c and c[-1] == ' ':
        c.pop()
    return c


class Graphemes(TerminalTest):
    geometry = '20x4'

    def test_mode_report(self):
        run = self.run_in_terminal(r'''
            result['off'] = query('\x1b[?2027$p', r'\$y')
            write('\x1b[?2027h')
            result['on'] = query('\x1b[?2027$p', r'\$y')
            write('\x1b[?2027l')
            result['off_again'] = query('\x1b[?2027$p', r'\$y')
        ''')
        r = run.result
        self.assertEqual(r['off'], '\x1b[?2027;2$y')
        self.assertEqual(r['on'], '\x1b[?2027;1$y')
        self.assertEqual(r['off_again'], '\x1b[?2027;2$y')

    def test_clusters_take_one_cell(self):
        text = f'a{KSSI}b\r\n{FAMILY}c\r\n{FLAG}d{HEART}e'
        run = self.run_in_terminal(f'''
            write('\\x1b[?2027h' + {text!r})
            s = dump()
            result['rows'] = [r['cells'] for r in s['rows'][:3]]
            result['cursor'] = s['cursor']
        ''')
        rows = [cells({'cells': row}) for row in run.result['rows']]
        self.assertEqual(rows[0], ['a', KSSI, 'b'])
        self.assertEqual(rows[1], [FAMILY, '', 'c'])                 # wide: two cells
        self.assertEqual(rows[2], [FLAG, '', 'd', HEART, '', 'e'])   # widened by the pair, by VS16
        self.assertEqual(run.result['cursor'], [2, 6])

    def test_without_the_mode(self):
        text = f'{KSSI}|{HEART}|\u1100\u1100|'
        run = self.run_in_terminal(f'''
            write({text!r})
            result['cells'] = dump()['rows'][0]['cells']
        ''')
        c = cells({'cells': run.result['cells']})
        # wcwidth as always: the conjunct's parts in their own cells (the
        # virama joins as a combining character, the vowel sign doesn't),
        # the heart narrow (VS16 joins it, combining), two jamo
        self.assertEqual(c, ['\u0915\u094d', '\u0937', '\u093f', '|', HEART, '|', '\u1100', '', '\u1100', '', '|'])

    def test_hangul_jamo(self):
        run = self.run_in_terminal(r'''
            write('\x1b[?2027hᄀ각x')
            result['cells'] = dump()['rows'][0]['cells']
        ''')
        # L L V T: one syllable block, two cells
        self.assertEqual(cells({'cells': run.result['cells']}), ['ᄀ각', '', 'x'])

    def test_end_of_line(self):
        run = self.run_in_terminal(r'''
            write('\x1b[?2027h' + 'x' * 19 + 'क्ष')
            s = dump()
            result['last'] = s['rows'][0]['cells'][19]
            result['next_row'] = s['rows'][1]['cells'][0]
            write('y')
            result['wrapped'] = dump()['rows'][1]['cells'][0]
        ''')
        r = run.result
        # the conjunct stays in the last cell, no wrap; the next letter wraps
        self.assertEqual(r['last'], 'क्ष')
        self.assertEqual(r['next_row'], ' ')
        self.assertEqual(r['wrapped'], 'y')

    def test_widening_needs_room(self):
        run = self.run_in_terminal(r'''
            write('\x1b[?2027h' + 'x' * 19 + '❤️')
            s = dump()
            result['last'] = s['rows'][0]['cells'][19]
            result['cursor'] = s['cursor']
        ''')
        # no room for a second cell: it stays narrow, in the last column
        self.assertEqual(run.result['last'], HEART)
        self.assertEqual(run.result['cursor'], [0, 19])


if __name__ == '__main__':
    unittest.main()
