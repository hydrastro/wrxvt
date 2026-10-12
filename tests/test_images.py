"""Pictures from image files: the kitty graphics protocol and iTerm2's."""
import base64
import struct
import unittest
import zlib

from harness import TerminalTest


def red(p):
    return p[0] > 200 and p[1] + p[2] < 60


def png(width, height, rgb=(255, 0, 0)):
    """A PNG of one colour."""
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + bytes(rgb) * width for _ in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def b64(data):
    return base64.b64encode(data).decode()


CELL = r'''
fw, fh = [int(n) for n in query('\x1b]776;?\x07', r'\x07$').split(';')[1:3]]
result['cell'] = [fw, fh]
'''


class Kitty(TerminalTest):
    geometry = '40x10'

    def test_raw_rgb(self):
        data = b64(b'\xff\x00\x00' * 20 * 20)
        run = self.run_in_terminal(CELL + f'''
result['reply'] = query('\\x1b_Ga=T,f=24,s=20,v=20,i=7;{data}\\x1b\\\\', r'\\x1b\\\\$')
result['cursor'] = dump()['cursor']
shot('kitty')
''')
        fw, fh = run.result['cell']
        self.assertEqual(run.result['reply'], '\x1b_Gi=7;OK\x1b\\')
        self.assertTrue(red(run.shots['kitty'].pixel(5, 5)))
        # the cursor is after the picture, on its last row
        self.assertEqual(run.result['cursor'], [(20 - 1) // fh, -(-20 // fw)])

    def test_png_in_chunks_and_cells(self):
        data = b64(png(8, 8))
        chunks = [data[i:i + 16] for i in range(0, len(data), 16)]
        seq = ''.join(f'\x1b_G{"a=T,f=100,c=4,r=2," if i == 0 else ""}m={int(i < len(chunks) - 1)};{c}\x1b\\'
                      for i, c in enumerate(chunks))
        run = self.run_in_terminal(CELL + f'''
write({seq!r})
result['cursor'] = dump()['cursor']
shot('png')
''')
        fw, fh = run.result['cell']
        image = run.shots['png']
        self.assertTrue(red(image.pixel(4 * fw - 2, 2 * fh - 2)))    # stretched to 4x2 cells
        self.assertFalse(red(image.pixel(4 * fw + 2, 2)))
        self.assertEqual(run.result['cursor'], [1, 4])

    def test_query_transmit_put_delete(self):
        data = b64(png(4, 4))
        run = self.run_in_terminal(CELL + f'''
result['query'] = query('\\x1b_Ga=q,i=31,f=100;{data}\\x1b\\\\', r'\\x1b\\\\$')
result['t'] = query('\\x1b_Ga=t,f=100,i=5;{data}\\x1b\\\\', r'\\x1b\\\\$')
result['p'] = query('\\x1b_Ga=p,i=5,c=2,r=1,C=1\\x1b\\\\', r'\\x1b\\\\$')
shot('put')
write('\\x1b_Ga=d,d=I,i=5\\x1b\\\\')
shot('deleted')
result['missing'] = query('\\x1b_Ga=p,i=5\\x1b\\\\', r'\\x1b\\\\$')
''')
        self.assertEqual(run.result['query'], '\x1b_Gi=31;OK\x1b\\')
        self.assertEqual(run.result['t'], '\x1b_Gi=5;OK\x1b\\')
        self.assertEqual(run.result['p'], '\x1b_Gi=5;OK\x1b\\')
        fw, fh = run.result['cell']
        self.assertTrue(red(run.shots['put'].pixel(fw + 2, 3)))
        self.assertFalse(any(red(p) for p in run.shots['deleted'].region(0, 0, 3 * fw, fh)))
        self.assertTrue(run.result['missing'].startswith('\x1b_Gi=5;ENOENT'))

    def test_quiet_and_no_id(self):
        data = b64(png(4, 4))
        run = self.run_in_terminal(f'''
result['quiet'] = query('\\x1b_Ga=T,f=100,i=9,q=1;{data}\\x1b\\\\', r'\\x1b\\\\$', timeout=0.5)
result['noid'] = query('\\x1b_Ga=T,f=100;{data}\\x1b\\\\', r'\\x1b\\\\$', timeout=0.5)
''')
        self.assertEqual(run.result['quiet'], '')
        self.assertEqual(run.result['noid'], '')


class ITerm(TerminalTest):
    geometry = '40x10'

    def test_inline_image(self):
        data = b64(png(30, 30))
        run = self.run_in_terminal(CELL + f'''
write('\\x1b]1337;File=name=eC5wbmc=;size=99;width=3;inline=1:{data}\\x07')
result['cursor'] = dump()['cursor']
shot('iterm')
''')
        fw, fh = run.result['cell']
        image = run.shots['iterm']
        self.assertTrue(red(image.pixel(3 * fw - 2, 2)))       # three cells wide
        self.assertFalse(red(image.pixel(3 * fw + 2, 2)))
        rows = -(-3 * fw // fh)                                 # square: as tall as wide
        self.assertEqual(run.result['cursor'], [rows, 0])

    def test_not_inline_is_ignored(self):
        data = b64(png(30, 30))
        run = self.run_in_terminal(f'''
write('\\x1b]1337;File=name=eC5wbmc=:{data}\\x07')
result['cursor'] = dump()['cursor']
''')
        self.assertEqual(run.result['cursor'], [0, 0])


class Memory(TerminalTest):
    geometry = '60x10'

    def test_oldest_pictures_go(self):
        # 100x100 pixels: 40000 bytes each; room for two in 0.08 MiB
        data = b64(png(100, 100))
        run = self.run_in_terminal(CELL + f"""
for n, col in enumerate((1, 21, 41)):
    write('\\x1b[1;%dH\\x1b_Ga=T,f=100,C=1,q=2;{data}\\x1b\\\\' % col)
    shot('after%d' % n)
""", args=['-xrm', 'URxvt.imageMemory: 0.08'])
        fw, fh = run.result['cell']
        first, second = run.shots['after1'], run.shots['after2']
        # two fit
        self.assertTrue(red(first.pixel(5, 5)) and red(first.pixel(20 * fw + 5, 5)))
        # the third one pushed the first out: its cells are blank now
        self.assertFalse(red(second.pixel(5, 5)))
        self.assertTrue(red(second.pixel(20 * fw + 5, 5)) and red(second.pixel(40 * fw + 5, 5)))


if __name__ == '__main__':
    unittest.main()
