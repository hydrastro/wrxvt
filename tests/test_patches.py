"""The resize fix and emoji sequences."""
import shutil
import unittest

from harness import FONT, TerminalTest


class Resize(TerminalTest):
    geometry = '40x12'

    @unittest.skipUnless(shutil.which('xdotool'), 'xdotool not installed')
    def test_shrinking_keeps_the_lines_above_the_prompt(self):
        run = self.run_in_terminal(r'''
            import os, subprocess, time
            write('\r\n'.join(f'line {n}' for n in range(1, 9)) + '\r\n$ ')
            before = dump()
            # 12 rows -> 9 rows: there are 3 empty rows below the prompt
            window = os.environ['WINDOWID']
            height = int(subprocess.check_output(['xdotool', 'getwindowgeometry', '--shell', window],
                                                 text=True).split('HEIGHT=')[1].split()[0])
            subprocess.run(['xdotool', 'windowsize', window, 'x', str(height * 9 // 12)], check=True)
            for _ in range(100):
                after = dump()
                if after['nrow'] == 9:
                    break
                time.sleep(0.05)
            result['rows'] = [r['text'].rstrip() for r in after['rows']]
            result['nrow'] = after['nrow']
        ''')
        self.assertEqual(run.result['nrow'], 9)
        self.assertEqual(run.result['rows'][0], 'line 1')
        self.assertEqual(run.result['rows'][8], '$')


class EmojiSequences(TerminalTest):
    def test_flag_is_drawn_as_one_emoji(self):
        run = self.run_in_terminal(r'''
            write('\U0001F1EE\U0001F1F9')       # regional indicators I + T: the Italian flag
            shot('flag')
        ''', args=['-fn', FONT + ',xft:Noto Color Emoji:pixelsize=16'])
        image = run.shots['flag']
        cells = image.region(0, 0, 40, 20)
        green = any(g > 100 and r < 100 and b < 100 for r, g, b in cells)
        red = any(r > 150 and g < 80 and b < 80 for r, g, b in cells)
        # the flag's green and red, not two letters in boxes
        self.assertTrue(green and red, 'flag colours not found')


if __name__ == '__main__':
    unittest.main()
