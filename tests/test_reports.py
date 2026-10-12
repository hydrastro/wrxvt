"""What the terminal reports about itself."""
import unittest

from harness import TerminalTest, version


class Reports(TerminalTest):
    def test_xtversion(self):
        run = self.run_in_terminal(r'''
            result['plain'] = query('\x1b[>q', r'\x1b\\$')
            result['zero'] = query('\x1b[>0q', r'\x1b\\$')
        ''')
        expected = f'\x1bP>|wrxvt({version()})\x1b\\'
        self.assertEqual(run.result['plain'], expected)
        self.assertEqual(run.result['zero'], expected)


if __name__ == '__main__':
    unittest.main()
