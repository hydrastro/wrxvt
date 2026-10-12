"""Hyperlinks (OSC 8)."""
import os
import shutil
import tempfile
import unittest

from harness import TerminalTest, cell

OPEN = '\x1b]8;;{}\x1b\\'
CLOSE = '\x1b]8;;\x1b\\'


class Hyperlinks(TerminalTest):
    def screen(self, text):
        run = self.run_in_terminal(f'''
            write({text!r})
            dump()
        ''')
        return run.screen

    def test_link_spans_its_text(self):
        screen = self.screen('a ' + OPEN.format('https://example.org/') + 'link' + CLOSE + ' b')
        links = [cell(screen, 0, c)['link'] for c in range(9)]
        self.assertEqual(links[0:2], [0, 0])
        self.assertTrue(links[2] > 0)
        self.assertEqual(set(links[2:6]), {links[2]})
        self.assertEqual(links[6:9], [0, 0, 0])

    def test_same_id_same_link_different_uri_different_link(self):
        screen = self.screen(
            '\x1b]8;id=x;https://a/\x1b\\A' + CLOSE + ' '
            '\x1b]8;id=x;https://a/\x1b\\A' + CLOSE + ' '
            + OPEN.format('https://b/') + 'B' + CLOSE)
        a1, a2, b = (cell(screen, 0, c)['link'] for c in (0, 2, 4))
        self.assertEqual(a1, a2)
        self.assertNotEqual(a1, b)

    def test_sgr_reset_keeps_the_link_erase_drops_it(self):
        screen = self.screen(OPEN.format('https://x/') + 'ab\x1b[0mcd\x1b[K' + CLOSE)
        self.assertTrue(cell(screen, 0, 3)['link'] > 0)  # d, after SGR 0
        self.assertEqual(cell(screen, 0, 6)['link'], 0)  # erased with the link open

    def test_invalid_uri_is_ignored(self):
        screen = self.screen(OPEN.format('has space') + 'x' + CLOSE)
        self.assertEqual(cell(screen, 0, 0)['link'], 0)

    def test_dashed_underline(self):
        run = self.run_in_terminal(f'''
            write({OPEN.format('https://x/') + 'xxxxxxxxxx' + CLOSE + 'xxxxxxxxxx'!r})
            shot('links')
        ''')
        image = run.shots['links']
        # below the baseline: the linked x's have a dashed line, the others nothing
        lit = lambda x0, x1: sum(1 for y in range(16, 19) for x in range(x0, x1)
                                 if sum(image.pixel(x, y)) > 300)
        self.assertGreater(lit(0, 90), 20)
        self.assertLess(lit(0, 90), 3 * 90 * 3 // 4)     # dashed, not solid
        self.assertEqual(lit(100, 190), 0)

    @unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
    def test_control_click_opens(self):
        out = tempfile.mktemp(prefix='wrxvt-link-')
        self.addCleanup(lambda: os.path.exists(out) and os.unlink(out))
        run = self.run_in_terminal(f'''
            import os, subprocess, time
            write({OPEN.format('https://example.org/a?b=c') + 'click here' + CLOSE!r})
            settle()
            window = os.environ['WINDOWID']
            subprocess.run(['xdotool', 'mousemove', '--window', window, '20', '8',
                            'keydown', 'ctrl', 'click', '1', 'keyup', 'ctrl'],
                           check=True, capture_output=True)
            for _ in range(100):
                if os.path.exists({out!r}) and os.path.getsize({out!r}):
                    break
                time.sleep(0.05)
        ''', args=['-xrm', f'URxvt.url-launcher: printf %s >{out}'])
        with open(out) as f:
            self.assertEqual(f.read(), 'https://example.org/a?b=c')


if __name__ == '__main__':
    unittest.main()
