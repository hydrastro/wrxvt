"""Shell integration: OSC 7 (current directory), OSC 133 (prompt marks)."""
import os
import shutil
import tempfile
import textwrap
import time
import unittest

from harness import TerminalTest


@unittest.skipUnless(shutil.which('xprop'), 'xprop not installed')
class CurrentDirectory(TerminalTest):
    def test_property(self):
        run = self.run_in_terminal(r'''
            import os, subprocess
            write('\x1b]7;file://somehost/tmp/a%20dir/%C3%A9t%C3%A9\x07')
            settle()
            result['prop'] = subprocess.check_output(
                ['xprop', '-id', os.environ['WINDOWID'], '_WRXVT_CWD'], text=True)
        ''')
        self.assertEqual(run.result['prop'].strip(), '_WRXVT_CWD(UTF8_STRING) = "/tmp/a dir/été"')


@unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
class Prompts(TerminalTest):
    geometry = '40x10'

    def test_jump_between_prompts(self):
        run = self.run_in_terminal(r'''
            import os, subprocess
            def key(k):
                settle()
                # no window manager: the keyboard goes to the window under the pointer
                window = os.environ['WINDOWID']
                subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                                'windowfocus', window, 'key', k],
                               check=True, capture_output=True)
            for n in range(1, 4):
                write(f'\x1b]133;A\x07$ command {n}\r\n')
                write(''.join(f'output {n}.{i}\r\n' for i in range(25)))
            write('\x1b]133;A\x07$ ')
            tops = []
            # (modifiers are unreliable with xdotool on a bare Xvfb: F keys)
            for k in ['F7', 'F7', 'F7', 'F7', 'F8', 'F8', 'F8']:
                key(k)
                tops.append(dump()['view_top'].rstrip())
            result['tops'] = tops
        ''', args=['-sl', '1000', '-keysym.F7', 'prompt:previous', '-keysym.F8', 'prompt:next'])
        self.assertEqual(run.result['tops'], [
            '$ command 3', '$ command 2', '$ command 1',
            '$ command 1',          # no earlier prompt: stays
            '$ command 2', '$ command 3',
            'output 3.16',          # past the last prompt: back to the bottom
        ])

    def test_marks_survive_reflow(self):
        run = self.run_in_terminal(r'''
            import os, subprocess, time
            window = os.environ['WINDOWID']
            for n in range(1, 3):
                write(f'\x1b]133;A\x07$ command {n}\r\n')
                write(''.join(f'output {n}.{i} ' + 'x' * 30 + '\r\n' for i in range(25)))
            write('\x1b]133;A\x07$ ')
            settle()
            # narrower: every output line wraps, rows move
            width = int(subprocess.check_output(['xdotool', 'getwindowgeometry', '--shell', window],
                                                text=True).split('WIDTH=')[1].split()[0])
            subprocess.run(['xdotool', 'windowsize', window, str(width * 3 // 4), 'y'], check=True)
            for _ in range(100):
                if dump()['ncol'] == 30:
                    break
                time.sleep(0.05)
            subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                            'windowfocus', window, 'key', 'F7', 'F7'],
                           check=True, capture_output=True)
            result['top'] = dump()['view_top'].rstrip()
        ''', args=['-sl', '1000', '-keysym.F7', 'prompt:previous'])
        self.assertEqual(run.result['top'], '$ command 1')


@unittest.skipUnless(shutil.which('xdotool') and shutil.which('xclip'), 'xdotool or xclip not installed')
class CopyOutput(TerminalTest):
    geometry = '40x10'

    KEY = r"""
import os, subprocess
def copy_output():
    settle()
    window = os.environ['WINDOWID']
    subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                    'windowfocus', window, 'key', 'F6'], check=True, capture_output=True)
    settle()
    return subprocess.run(['xclip', '-o', '-selection', 'clipboard'],
                          capture_output=True, text=True).stdout
"""

    def copied(self, code):
        run = self.run_in_terminal(self.KEY + textwrap.dedent(code),
                                   args=['-sl', '100', '-keysym.F6', 'prompt:copy-output'])
        return run.result

    def test_between_the_marks(self):
        r = self.copied(r"""
write('\x1b]133;A\x07$ \x1b]133;B\x07ls\r\n\x1b]133;C\x07')
write('one\r\ntwo  \r\n' + 'w' * 50 + '\r\n\x1b]133;D;0\x07')
write('\x1b]133;A\x07$ \x1b]133;B\x07')
result['copied'] = copy_output()
""")
        self.assertEqual(r['copied'], 'one\ntwo\n' + 'w' * 50)

    def test_while_running(self):
        r = self.copied(r"""
write('\x1b]133;A\x07$ make\r\n\x1b]133;C\x07compiling a\r\ncompiling b\r\n')
result['copied'] = copy_output()
""")
        self.assertEqual(r['copied'], 'compiling a\ncompiling b')

    def test_prompts_only(self):
        r = self.copied(r"""
write('\x1b]133;A\x07$ old\r\nold output\r\n')
write('\x1b]133;A\x07$ ls\r\nfile1\r\nfile2\r\n\x1b]133;A\x07$ ')
result['copied'] = copy_output()
""")
        self.assertEqual(r['copied'], 'file1\nfile2')


class LongCommands(TerminalTest):
    def test_notification(self):
        out = tempfile.mktemp(prefix='wrxvt-long-')
        self.addCleanup(lambda: os.path.exists(out) and os.unlink(out))
        self.run_in_terminal(r"""
import time
write('\x1b]133;A\x07[me@host ~]$ \x1b]133;B\x07sleep 2 && false\r\n\x1b]133;C\x07')
time.sleep(1.2)
write('\x1b]133;D;1\x07\x1b]133;A\x07$ ')
# a quick one: no notification
write('\x1b]133;B\x07true\r\n\x1b]133;C\x07\x1b]133;D;0\x07')
settle()
time.sleep(0.5)
""", args=['-xrm', 'URxvt.notifyLongCommand: 1',
           '-xrm', f'URxvt.notifyCommand: printf "%s|%s;" >>{out}'])
        for _ in range(40):
            if os.path.exists(out):
                break
            time.sleep(0.05)
        with open(out) as f:
            self.assertEqual(f.read(), 'Command failed (exit status 1)|sleep 2 && false\nafter 1 s;')


if __name__ == '__main__':
    unittest.main()
