"""The terminfo entry agrees with what the terminal answers (XTGETTCAP)."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from harness import TerminalTest

SOURCE = Path(__file__).resolve().parent.parent / 'doc' / 'etc' / 'rxvt-unicode.terminfo'
# the capabilities both describe
NAMES = ['Smulx', 'Setulc', 'ol', 'smxx', 'rmxx', 'Smol', 'Rmol', 'Ss', 'Se', 'Sync', 'Ms',
         'BE', 'BD', 'PS', 'PE', 'fe', 'fd', 'XR', 'Tc']


@unittest.skipUnless(SOURCE.exists() and shutil.which('tic') and shutil.which('infocmp'),
                     'needs the source tree, tic and infocmp')
class Terminfo(TerminalTest):
    def test_matches_xtgettcap(self):
        compiled = tempfile.mkdtemp(prefix='wrxvt-terminfo-')
        self.addCleanup(shutil.rmtree, compiled, ignore_errors=True)
        subprocess.run(['tic', '-x', '-o', compiled, str(SOURCE)], check=True, capture_output=True)
        listing = subprocess.run(['infocmp', '-x', '-1', '-q', 'rxvt-unicode-256color'],
                                 env={**os.environ, 'TERMINFO': compiled},
                                 capture_output=True, text=True, check=True).stdout
        terminfo = {}
        for line in listing.splitlines():
            line = line.strip().rstrip(',')
            name, _, value = line.partition('=')
            if name in NAMES:
                terminfo[name] = value if _ else True

        request = ';'.join(n.encode().hex() for n in NAMES)
        run = self.run_in_terminal(f'''
            result['reply'] = query('\\x1bP+q{request}\\x1b\\\\',
                                    r'(?:\\x1bP[01]\\+r[0-9A-Fa-f=]*\\x1b\\\\){{{len(NAMES)}}}$')
        ''')
        answered = {}
        for part in run.result['reply'].split('\x1b\\'):
            if part.startswith('\x1bP1+r'):
                name, _, value = part[5:].partition('=')
                answered[bytes.fromhex(name).decode()] = bytes.fromhex(value).decode() if _ else True

        def unescape(v):
            if v is True:
                return v
            return v.replace('\\E', '\x1b').replace('\\007', '\x07').replace('^G', '\x07')

        for name in NAMES:
            with self.subTest(name):
                self.assertIn(name, terminfo)
                self.assertEqual(unescape(terminfo[name]), answered.get(name))


if __name__ == '__main__':
    unittest.main()
