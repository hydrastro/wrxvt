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
- **24-bit colours that stay put**: rxvt-unicode recycled colour slots,
  so text already on the screen could change colour when new colours
  came; now slots are only reused once nothing shows their colour.
- **Colon sub-parameters** in SGR (`38:2::R:G:B`, `4:3`), and renditions
  widened to 64 bits to make room for all of the above.
- **Emoji sequences** (flags, skin tones, ZWJ families) drawn as one
  emoji, with `--enable-emoji-sequences` (needs HarfBuzz), which also
  draws other clusters (Indic conjuncts, marks) with the font's shaping.
- **Grapheme clusters** (mode 2027): a program that sets it gets each
  cluster (a letter and its marks, an Indic conjunct, a flag, an emoji
  sequence) in one cell, or two if wide, by Unicode's rules.
- **Built-in glyphs** for braille (U+2800-28FF), powerline arrows
  (U+E0B0-E0B7) and sextants (U+1FB00-1FB3B). They join up exactly
  whatever the font.
- **Hyperlinks** (OSC 8): drawn with a dashed underline. Control-click
  opens them with `url-launcher`.
- **Hints** (`hints:open`, `hints:copy`, `hints:insert`): letters on
  every link, path and git hash on the screen; type one to open, copy or
  insert it, without the mouse.
- **Synchronized output** (mode 2026): no tearing when a program redraws
  everything.
- **Pictures**: sixel graphics, the kitty graphics protocol and iTerm2
  inline images (img2sixel, chafa, `kitty +kitten icat`, timg, yazi,
  ranger, imgcat...). They scroll and go into the scrollback like text,
  within a memory limit (`imageMemory`, 256 MiB): the oldest go first.
  Kitty's Unicode placeholders work as well (`kitten icat
  --unicode-placeholder`, timg and yazi in tmux): pictures as text that
  tmux and vim move around.

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
- **Desktop notifications** (OSC 9, kitty's OSC 99 with titles and
  bodies, and OSC 777 notify) through `notifyCommand` (`notify-send` by
  default).
- Control strings of any length (big OSC 52 copies). APC, PM and SOS
  strings are ignored instead of printed.

Sharing:

- **Share the terminal on a web page** (`share:start`, `-share`): a
  link to watch and a link that can also type (`share:control`), each
  with its own random key, for pair programming, teaching, demos, or
  your own phone. The typing page takes any keyboard (input methods and
  phone keyboards included), has a bar with Esc, Tab, Ctrl, Alt and the
  arrows, and passes clicks and the wheel to programs that use the mouse.
  Local by default; HTTPS for the network (`shareCertificate: auto` makes
  a certificate, `--enable-share-tls`); the title says when someone is
  watching.
- **The scrollback in a pager** (`scrollback:pager`): everything, with
  its colours, in `less` in a new window.
- **Recording** (`record:toggle`, `-record FILE`): asciinema files of
  what is shown (not what is typed), for `asciinema play`, the web player
  or agg.

Mouse:

- **Drag and drop**: files dropped on the window become their paths,
  quoted for the shell; dropped text is pasted.

Shell integration:

- **OSC 7**: the shell's directory is put on the window as `_WRXVT_CWD`,
  so new terminals can open there.
- **OSC 133** prompt marks, with key actions that jump between prompts
  and copy the last command's output, and a notification when a long
  command ends while you're elsewhere (`notifyLongCommand`).

Configuration:

- **Colours follow the X resources**: after `xrdb -merge` (or a theme
  switch) open windows take the new colours, and programs using mode
  2031 hear that dark became light.
- `color16` to `color255` resources.
- Built-in key actions, no perl needed: `font-size:increase|decrease|reset`
  (the names of the popular perl extension), `prompt:previous|next|copy-output`,
  `hints:open|copy|insert`, `scrollback:pager`, `share:start|stop|toggle|control`
  and `record:start|stop|toggle`.

Fixes:

- Lines above the prompt survive the window getting shorter (scrollback
  on resize).
- `-keysym.*` and `-xrm` options now work when X has no resource
  database.
- DECSCUSR 0 restores the configured cursor style.
- Title reports end with 7-bit ST.
- A font of one pixel (or `font-size:decrease` pressed long enough) no
  longer hangs the terminal at 100% CPU on the next character that needs
  a fallback font.
- DEL (0x7f) is ignored, as in other terminals, not shown.
- No crash on exit with `-xrm`, nor when perl can't start; the display's
  resource database is freed once.

## Not in wrxvt (yet)

Planned at some point, and why not now:

- **Grapheme clusters (mode 2027)**: emoji sequences are drawn as one
  emoji already; full clustering (Indic scripts, Hangul) needs Unicode's
  segmentation rules in the cell model.
- **Detachable sessions in the daemon, tmux control mode**: tmux does
  both today; wrxvt's pictures and keyboard protocol work through tmux 3.4.
- **Native tabs and splits**: i3 (or any tiling window manager) does it.
- **A native scrollback search**: the perl `searchable-scrollback`
  extension and `scrollback:pager` cover it.
- **Other renderers** (OpenGL, KMS/framebuffer without X, Wayland), BiDi,
  ligatures: each a project of its own; XRender stays, and is fast.
- **Unicode placeholders for kitty pictures, text sizing (OSC 66), file
  transfer**: few programs send them.

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
URxvt.keysym.C-S-F12:  share:toggle
URxvt.keysym.C-S-F11:  share:control
URxvt.keysym.C-S-F10:  record:toggle
URxvt.color208:        #ff8700
```

Shell integration for bash (prompt and command marks, the current
directory):

```bash
# ~/.bashrc (at the end, so that this runs first and sees the exit status)
__wrxvt_prompt() {
  printf '\e]133;D;%s\e\\\e]7;file://%s%s\e\\' "$?" "$HOSTNAME" "$PWD"
}
PROMPT_COMMAND="__wrxvt_prompt${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
PS0+='\e]133;C\e\\'
PS1='\[\e]133;A\e\\\]'"$PS1"'\[\e]133;B\e\\\]'
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
./configure --enable-everything --enable-256-color --enable-emoji-sequences --enable-share-tls
make
```

`--enable-emoji-sequences` needs HarfBuzz and FreeType 2.11, and
`--enable-share-tls` (HTTPS for sharing) OpenSSL 1.1.1 or later; both
are optional and not part of `--enable-everything`.

## Tests

`make check` runs the test suite (`tests/`) against the binary just
built. It starts the real terminal under Xvfb and runs a small program
inside it. A perl extension dumps the screen and ImageMagick takes
screenshots. The suite needs Xvfb, Python 3, ImageMagick and xdotool;
tests whose tools are missing are skipped.

```sh
make check
cd tests && WRXVT=../src/rxvt WRXVT_LIBDIR=../src python3 -m unittest test_sgr -v
make bench                       # MB/s for plain text, colours, Unicode, scrolling
make bench COMPARE=urxvt         # and the same files in another terminal
```

Every feature commit adds tests that fail without it. `test_robustness`
feeds the terminal random bytes and escape sequences with absurd
parameters (seeded, so failures repeat) and checks it still answers;
`nix flake check` runs the whole suite a second time on a build with
AddressSanitizer and UBSan (`checks.*.tests-asan`), where any report
fails the check.

## Licence

GPL-3.0-or-later, like rxvt-unicode (see COPYING). The Unicode data in
`src/emoji-properties.h` is under the Unicode licence
(doc/UNICODE-LICENSE.txt).
