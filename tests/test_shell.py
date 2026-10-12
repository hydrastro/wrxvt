"""Shell integration: OSC 7 (current directory), OSC 133 (prompt marks)."""
import shutil
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
                                'windowfocus', '--sync', window, 'key', k],
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
                            'windowfocus', '--sync', window, 'key', 'F7', 'F7'],
                           check=True, capture_output=True)
            result['top'] = dump()['view_top'].rstrip()
        ''', args=['-sl', '1000', '-keysym.F7', 'prompt:previous'])
        self.assertEqual(run.result['top'], '$ command 1')


if __name__ == '__main__':
    unittest.main()
