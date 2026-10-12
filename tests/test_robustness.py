"""Garbage in: the terminal must keep working, whatever programs send.

Random bytes, and escape sequences with random (often absurd) parameters
of every kind the parser knows: CSI, DCS (sixel, DECRQSS, XTGETTCAP),
OSC (colours, titles, hyperlinks, clipboard, notifications...), APC
(kitty graphics), broken UTF-8, huge numbers. After each batch the
terminal has to answer a status report. Seeded, so a failure repeats.

Run it with an ASan/UBSan build for more: nix build .#checks.x86_64-linux.tests-asan
"""
import random
import unittest

from harness import TerminalTest

GENERATOR = r'''
import random

def number(rng):
    return rng.choice(['', '0', '1', '2', str(rng.randrange(256)), str(rng.randrange(70000)),
                       '4294967296', '99999999999999999999', '-1'])

def params(rng):
    seps = [';', ':']
    return ''.join(number(rng) + rng.choice(seps) for _ in range(rng.randrange(20)))[:-1] if rng.random() < .9 else \
        ';'.join(number(rng) for _ in range(rng.randrange(300)))

def text(rng, n):
    alphabet = 'abc;:=,./?#\\x07\x1b\x9c\xffé中\U0001f600‍️́ '
    return ''.join(rng.choice(alphabet) for _ in range(n))

def sequence(rng):
    kind = rng.randrange(12)
    if kind == 0:   # CSI with a private prefix and an intermediate
        return '\x1b[' + rng.choice(['', '?', '>', '<', '=']) + params(rng) + \
            rng.choice(['', ' ', '$', '"', "'", '*']) + chr(rng.randrange(0x40, 0x7f))
    if kind == 1:   # SGR
        return '\x1b[' + params(rng) + 'm'
    if kind == 2:   # OSC
        op = rng.choice(['0', '2', '4', '7', '8', '9', '10', '11', '12', '52', '99', '104', '110',
                         '133', '777', '1337', '701', '702', '704', '710', '720', number(rng)])
        return '\x1b]' + op + ';' + text(rng, rng.randrange(80)) + rng.choice(['\x07', '\x1b\\', ''])
    if kind == 3:   # sixel
        body = ''.join(rng.choice('#;0123456789!"?@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz{}~$-')
                       for _ in range(rng.randrange(400)))
        return '\x1bP' + params(rng) + 'q' + body + '\x1b\\'
    if kind == 4:   # other DCS
        return '\x1bP' + rng.choice(['$q', '+q', '=1s', '>|', '']) + text(rng, rng.randrange(40)) + '\x1b\\'
    if kind == 5:   # kitty graphics
        keys = ','.join(rng.choice('aftsvxyXYwhcrCimqopdUz') + '=' + rng.choice([number(rng), 'T', 't', 'p', 'd', 'q', 'a'])
                        for _ in range(rng.randrange(8)))
        payload = ''.join(rng.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=')
                          for _ in range(rng.randrange(200)))
        return '\x1b_G' + keys + ';' + payload + '\x1b\\'
    if kind == 6:   # iTerm2 file
        return '\x1b]1337;File=' + rng.choice(['inline=1', 'inline=1;width=' + number(rng), '']) + ':' + \
            text(rng, rng.randrange(60)) + '\x07'
    if kind == 7:   # broken UTF-8 and C1
        return bytes(rng.randrange(256) for _ in range(rng.randrange(40))).decode('latin-1')
    if kind == 8:   # cursor movement to absurd places, scrolling regions
        return '\x1b[%s;%sH\x1b[%s;%sr\x1b[%sL\x1b[%sM\x1b[%s@\x1b[%sP' % tuple(number(rng) for _ in range(8))
    if kind == 9:   # modes
        return '\x1b[?' + number(rng) + rng.choice('hlsr')
    if kind == 10:  # text, wide and combining
        return text(rng, rng.randrange(200))
    return '\x1b' + chr(rng.randrange(0x20, 0x7f))   # ESC + anything
'''


class Garbage(TerminalTest):
    geometry = '50x12'

    def test_random_sequences(self):
        for seed in range(32):
            with self.subTest(seed=seed):
                run = self.run_in_terminal(GENERATOR + f'''
rng = random.Random({seed})
ok = 0
for batch in range(40):
    chunk = ''.join(sequence(rng) for _ in range(60))
    write(chunk.encode('utf-8', 'surrogateescape'))
    # back to a known state (out of any string, out of VT52 mode, the
    # window's size), then a status report must come back (maybe with
    # other answers: a selection report comes when its owner replies)
    write('\\x18\\x1b\\\\\\x1b<\\x1b[8;12;50t\\x1b[!p\\x1b[?1049l\\x1b[r\\x1b[m')
    if '\\x1b[0n' in query('\\x1b[5n', r'\\x1b\\[0n', 5):
        ok += 1
result['ok'] = ok
''', timeout=120)
                self.assertEqual(run.result['ok'], 40)

    def test_random_bytes(self):
        run = self.run_in_terminal(r'''
import random
rng = random.Random(1)
ok = 0
for batch in range(20):
    write(bytes(rng.randrange(256) for _ in range(20000)))
    write('\x18\x1b\\\x1b<\x1b[!p\x1b[?1049l\x1b[m')
    if '\x1b[0n' in query('\x1b[5n', r'\x1b\[0n', 5):
        ok += 1
result['ok'] = ok
''', timeout=120)
        self.assertEqual(run.result['ok'], 20)


if __name__ == '__main__':
    unittest.main()
