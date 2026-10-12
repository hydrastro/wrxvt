"""Hints: pick links, paths and hashes on the screen with the keyboard."""
import os
import shutil
import tempfile
import time
import unittest

from harness import TerminalTest

# (while the labels show the terminal reads nothing: no settle () then)
KEYS = r'''
import os, subprocess, time
def key(*keys, wait=True):
    if wait:
        settle()
    window = os.environ['WINDOWID']
    subprocess.run(['xdotool', 'mousemove', '--window', window, '5', '5',
                    'windowfocus', window, 'key', *keys], check=True, capture_output=True)
    time.sleep(0.3)
def labels_shot(name):
    request = f'{RESULT}.shot-{name}'
    with open(request + '.req', 'w') as f:
        f.write(os.environ.get('WINDOWID', ''))
    wait_file(request + '.done', 30)
def clipboard():
    settle()
    return subprocess.run(['xclip', '-o', '-selection', 'clipboard'],
                          capture_output=True, text=True).stdout
def rows():
    return [r['text'] for r in dump()['rows']]
SCREEN = ('see https://example.com/a_(b) now.\r\n'
          'error in src/main.C:12:5: oops\r\n'
          'commit 3f50350abc done\r\n'
          'a \x1b]8;;http://x.org/page\x1b\\LINK\x1b]8;;\x1b\\ here\r\n'
          'ok\r\n')
'''



@unittest.skipUnless(shutil.which('xdotool') and shutil.which('xclip'), 'xdotool or xclip not installed')
class Hints(TerminalTest):
    geometry = '50x8'
    ARGS = ['-keysym.F5', 'hints:copy', '-keysym.F6', 'hints:open', '-keysym.F7', 'hints:insert']

    def test_labels_and_copy(self):
        run = self.run_in_terminal(KEYS + r'''
write(SCREEN)
before = rows()
key('F5')
labels_shot('labels')
key('f', wait=False)            # the top one: the URL
result['copied'] = clipboard()
result['restored'] = rows() == before
for k, name in (('d', 'path'), ('s', 'hash'), ('a', 'link')):
    key('F5')
    key(k, wait=False)
    result[name] = clipboard()
''', args=self.ARGS)
        r = run.result
        self.assertEqual(r['copied'], 'https://example.com/a_(b)')
        self.assertTrue(r['restored'])
        self.assertEqual(r['path'], 'src/main.C')
        self.assertEqual(r['hash'], '3f50350abc')
        self.assertEqual(r['link'], 'http://x.org/page')
        # yellow labels where each starts, nothing yellow elsewhere on their rows
        image = run.shots['labels']
        cw, ch = image.width // 50, image.height // 8
        def yellow(row, col):
            return any(r > 150 and g > 150 and b < 80
                       for r, g, b in image.region(col * cw, row * ch, (col + 1) * cw, (row + 1) * ch))
        self.assertEqual([yellow(0, 4), yellow(1, 9), yellow(2, 7), yellow(3, 2)], [True] * 4)
        self.assertEqual([yellow(0, 30), yellow(1, 25), yellow(4, 0)], [False] * 3)

    def test_escape_and_output_waits(self):
        run = self.run_in_terminal(KEYS + r'''
write(SCREEN)
before = rows()
key('F5')
write('more output\r\n')     # waits while the labels show
key('Escape', wait=False)
after = rows()
result['back'] = after[:5] == before[:5]
result['output'] = any('more output' in r for r in after)
''', args=self.ARGS)
        self.assertTrue(run.result['back'])
        self.assertTrue(run.result['output'])

    def test_open_relative_path_and_insert(self):
        out = tempfile.mktemp(prefix='wrxvt-open-')
        self.addCleanup(lambda: os.path.exists(out) and os.unlink(out))
        run = self.run_in_terminal(KEYS + r'''
write('\x1b]7;file://host/home/me/project\x07')
write(SCREEN)
key('F6')
key('d', wait=False)
key('F7')
key('s', wait=False)
result['typed'] = read_until('abc', 2)
''', args=self.ARGS + ['-xrm', f'URxvt.url-launcher: printf %s >{out}'])
        for _ in range(40):
            if os.path.exists(out):
                break
            time.sleep(0.05)
        with open(out) as f:
            self.assertEqual(f.read(), '/home/me/project/src/main.C')
        self.assertEqual(run.result['typed'], '3f50350abc')


if __name__ == '__main__':
    unittest.main()
