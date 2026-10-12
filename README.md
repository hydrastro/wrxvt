# wrxvt

wrxvt is [rxvt-unicode](http://software.schmorp.de/pkg/rxvt-unicode.html)
(urxvt) with the terminal features programs expect today. It is just as
small and fast and reads the same `URxvt.*` resources, and it runs the same
perl extensions. Your Xresources keep working.

It follows rxvt-unicode's CVS (newer than the 9.31 release) and is
version 9.32. The binaries are `wrxvt`, `wrxvtd` and `wrxvtc`. The
terminal still calls itself `rxvt-unicode-256color` (`TERM`), so remote
machines need nothing new.

## What it adds

Drawing:

- **Underline styles and colours**: `SGR 4:2`-`4:5` (double, curly,
  dotted, dashed), `21`, and `58`/`59` colours. Used for diagnostics in
  Neovim, Helix and Emacs.
- **Faint, crossed-out, overline and invisible text** (`SGR 2, 9, 53, 8`).
- **Colon sub-parameters** in SGR (`38:2::R:G:B`, `4:3`), and renditions
  widened to 64 bits to make room for all of the above.
- **Emoji sequences** (flags, skin tones, ZWJ families) drawn as one
  emoji, with `--enable-emoji-sequences` (needs HarfBuzz).
- **Built-in glyphs** for braille (U+2800-28FF), powerline arrows
  (U+E0B0-E0B7) and sextants (U+1FB00-1FB3B). They join up exactly
  whatever the font.
- **Hyperlinks** (OSC 8): drawn with a dashed underline. Control-click
  opens them with `url-launcher`.
- **Synchronized output** (mode 2026): no tearing when a program redraws
  everything.
- **Sixel graphics**: pictures (img2sixel, chafa, yazi, lsix, gnuplot's
  sixel terminal) that scroll and go into the scrollback like text.

Talking to programs:

- **Kitty keyboard protocol** (`CSI > flags u` ...): unambiguous keys,
  releases, every modifier combination.
- **OSC 52** to set the clipboard and primary selection (reading it needs
  `-insecure`).
- **DECRQSS** and **XTGETTCAP** (so programs can find features without
  trusting `TERM`), and **XTVERSION** (`CSI > q`).
- **Colour scheme reports**: `CSI ? 996 n`, and mode 2031 for updates.
- **SGR-pixel mouse** (mode 1016), plus cell and text-area size in pixels
  (`CSI 16 t`, `CSI 14 t`).
- **Title stack** (`CSI 22/23 t`): the title comes back after vim exits.
- **OSC 104 and 110-119** reset colours. `CSI 0 SP q` restores *your*
  cursor style.
- **Desktop notifications** (OSC 9) through `notifyCommand`
  (`notify-send` by default).
- Control strings of any length (big OSC 52 copies). APC, PM and SOS
  strings are ignored instead of printed.

Shell integration:

- **OSC 7**: the shell's directory is put on the window as `_WRXVT_CWD`,
  so new terminals can open there.
- **OSC 133** prompt marks, with key actions that jump between prompts.

Configuration:

- `color16` to `color255` resources.
- Built-in key actions: `font-size:increase|decrease|reset` (the names
  of the popular perl extension) and `prompt:previous|next`.

Fixes:

- Lines above the prompt survive the window getting shorter (scrollback
  on resize).
- `-keysym.*` and `-xrm` options now work when X has no resource
  database.
- DECSCUSR 0 restores the configured cursor style.
- Title reports end with 7-bit ST.

The [terminfo entry](doc/etc/rxvt-unicode.terminfo) describes the new
features as extended capabilities (`tic -x`).

## Configuration examples

```
! ~/.Xresources
URxvt.font:            xft:DejaVu Sans Mono:pixelsize=16,xft:Noto Color Emoji
URxvt.keysym.C-plus:   font-size:increase
URxvt.keysym.C-minus:  font-size:decrease
URxvt.keysym.C-0:      font-size:reset
URxvt.keysym.C-S-Up:   prompt:previous
URxvt.keysym.C-S-Down: prompt:next
URxvt.url-launcher:    firefox
URxvt.notifyCommand:   notify-send --app-name=wrxvt --
URxvt.color208:        #ff8700
```

Shell integration for bash (prompt marks and the current directory):

```bash
# ~/.bashrc
__wrxvt_prompt() {
  printf '\e]7;file://%s%s\e\\' "$HOSTNAME" "$PWD"
}
PROMPT_COMMAND="__wrxvt_prompt${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
PS1='\[\e]133;A\e\\\]'"$PS1"
```

Open a new terminal in the current one's directory from i3:

```
bindsym $mod+Shift+Return exec wrxvt -cd "$(xprop -id "$(xdotool getactivewindow)" _WRXVT_CWD | cut -d'"' -f2)"
```

## Building

With Nix (flakes):

```sh
nix build            # ./result/bin/wrxvt
nix run              # try it
nix flake check      # the build, a minimal build, and the test suite
nix develop          # autoconf, Xvfb, gdb...; unpacks libev into ./libev
```

The flake has an overlay (`overlays.default`) that provides `pkgs.wrxvt`
for a NixOS configuration.

By hand: rxvt-unicode compiles libev in from `libev/`, a separate CVS
module (any libev 4.33 source works):

```sh
cvs -z3 -d :pserver:anonymous@cvs.schmorp.de/schmorpforge co libev
./configure --enable-everything --enable-256-color --enable-emoji-sequences
make
```

## Tests

`make check` runs the test suite (`tests/`) against the binary just
built. It starts the real terminal under Xvfb and runs a small program
inside it. A perl extension dumps the screen and ImageMagick takes
screenshots. The suite needs Xvfb, Python 3, ImageMagick and xdotool;
tests whose tools are missing are skipped.

```sh
make check
cd tests && WRXVT=../src/rxvt WRXVT_LIBDIR=../src python3 -m unittest test_sgr -v
```

Every feature commit adds tests that fail without it.

## Licence

GPL-3.0-or-later, like rxvt-unicode (see COPYING). The Unicode data in
`src/emoji-properties.h` is under the Unicode licence
(doc/UNICODE-LICENSE.txt).
