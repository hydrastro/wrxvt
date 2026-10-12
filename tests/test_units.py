"""The C++ unit tests of the Unicode tables (no terminal needed)."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
CXX = os.environ.get('CXX') or shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')


@unittest.skipUnless(CXX, 'no C++ compiler')
@unittest.skipUnless((HERE.parent / 'src' / 'grapheme.h').exists(), 'no sources next to the tests')
class Units(unittest.TestCase):
    def run_unit(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / 'unit'
            subprocess.run([CXX, '-std=c++17', '-Wall', '-Wextra', '-Werror', str(HERE / source),
                            '-o', str(exe)], check=True, capture_output=True)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_graphemes(self):
        self.run_unit('graphemes.C')

    def test_emoji_sequences(self):
        self.run_unit('emoji-sequences.C')


if __name__ == '__main__':
    unittest.main()
