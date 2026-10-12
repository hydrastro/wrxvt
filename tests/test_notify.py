"""Desktop notifications (OSC 9)."""
import os
import tempfile
import time
import unittest

from harness import TerminalTest


class Notify(TerminalTest):
    def notification(self, sequence):
        out = tempfile.mktemp(prefix='wrxvt-notify-')
        self.addCleanup(lambda: os.path.exists(out) and os.unlink(out))
        self.run_in_terminal(f'''
            import time
            write({sequence!r})
            settle()
            time.sleep(0.5)
        ''', args=['-xrm', f'URxvt.notifyCommand: printf "%s|%s" >{out}'])
        for _ in range(40):
            if os.path.exists(out):
                break
            time.sleep(0.05)
        if not os.path.exists(out):
            return None
        with open(out) as f:
            return f.read()

    def test_notification_with_the_title(self):
        self.assertEqual(self.notification('\x1b]2;my title\x07\x1b]9;build done; 0 errors\x07'),
                         'my title|build done; 0 errors')

    def test_arguments_are_not_shell_code(self):
        self.assertEqual(self.notification('\x1b]2;t\x07\x1b]9;$(echo no) `x`\x07'),
                         't|$(echo no) `x`')

    def test_progress_reports_are_not_notifications(self):
        self.assertIsNone(self.notification('\x1b]9;4;1;50\x07'))


if __name__ == '__main__':
    unittest.main()
