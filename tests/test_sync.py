"""Synchronized output (mode 2026)."""
import unittest

from harness import TerminalTest

TIMEOUT = 0.5   # SYNC_OUTPUT_TIMEOUT in rxvt.h


def drawn(image):
    """Is there text in cells 1-3 of the top row? (cell 0 has the cursor)"""
    return any(sum(p) > 300 for p in image.region(12, 0, 36, 16))


class SyncOutput(TerminalTest):
    def test_supported(self):
        run = self.run_in_terminal(r'''
            result['off'] = query('\x1b[?2026$p')
            write('\x1b[?2026h')
            result['on'] = query('\x1b[?2026$p')
            write('\x1b[?2026l')
        ''')
        self.assertEqual(run.result['off'], '\x1b[?2026;2$y')
        self.assertEqual(run.result['on'], '\x1b[?2026;1$y')

    def test_update_appears_when_complete(self):
        run = self.run_in_terminal(r'''
            import time
            start = time.monotonic()
            write('\x1b[?2026hHELD')
            shot('during')
            result['elapsed'] = time.monotonic() - start
            write('\x1b[?2026l')
            shot('after')
        ''')
        self.assertTrue(drawn(run.shots['after']))
        if run.result['elapsed'] >= TIMEOUT:
            self.skipTest('the screenshot took longer than the time limit')
        self.assertFalse(drawn(run.shots['during']), 'drawn before the update was complete')

    def test_time_limit(self):
        run = self.run_in_terminal(r'''
            import time
            write('\x1b[?2026hNEVER ENDED')
            time.sleep(1.0)
            shot('later')
        ''')
        self.assertTrue(drawn(run.shots['later']))


if __name__ == '__main__':
    unittest.main()
