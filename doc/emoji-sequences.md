# Emoji sequence shaping

Optional, off by default. Enable with:

```
./configure --enable-xft --enable-unicode3 --enable-combining --enable-emoji-sequences
make
```

It needs HarfBuzz, FreeType 2.11+ and an Xft color font with the relevant glyphs.
The feature defaults off. Configure rejects missing prerequisites; enabling
it links with the C++ driver because the bounded shaping cache uses the C++
standard library.

Unicode 17 property tables replace broad hand-maintained ranges. The joining
rules cover regional-indicator pairs, modifier bases, VS16, keycaps, emoji
tags and Extended_Pictographic/Extend/ZWJ sequences. VS15 selects the ordinary
text path. Positive-width components join only on the same row and with
matching rendition; a row boundary falls back to ordinary rendering.

HarfBuzz supplies glyph IDs, advances and offsets. Shaped origins are
transformed using the Xft face matrix, including scaled bitmap color fonts;
the locked FreeType face is restored afterward. The drawing buffer is sized
by glyphs, so a shaped run is not truncated to the number of terminal cells.

This is **emoji shaping, not a general grapheme-width implementation**.
Terminal columns retain the existing sum of `wcwidth` results; a family emoji
may occupy more than two columns even when represented by one visual glyph.
Changing that policy requires coordinated decisions about applications,
readline, ncurses and tmux. The original codepoints remain in urxvt's
composition chain. Verify selection/copying with your applications.

At most 64 codepoints are shaped per sequence. Each font caches at most 256
successful or failed lookups and clears that cache when full or when the font
is closed. Unsupported/oversized sequences use urxvt's ordinary component
fallback. This bounds the new shaping cache; it does not redesign urxvt's
existing global composition table.

## Regression checks

```
c++ -std=c++17 -Wall -Wextra -Werror tests/emoji-sequences.C -o test-emoji
./test-emoji
```

The generated table can be reproduced with Python 3 from the two Unicode
17.0.0 inputs linked in `tools/update-emoji-properties.py`. Their SHA256
digests are recorded in `src/emoji-properties.h`; Unicode's data license is
in `doc/UNICODE-LICENSE.txt`. No network access is needed at build time.

Before a fork release, test your actual Xft/font setup: flags, modifiers,
families, keycaps, tag flags, VS15/VS16, repeated ZWJs, combining text,
foreground/background changes, right-edge wrapping, selection and copying,
font resizing, tmux and alternate-screen applications. The separate resize
patch also needs growth/shrink tests with scrollback and colored blank rows.
