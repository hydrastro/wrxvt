/*----------------------------------------------------------------------*
 * File:	hints.C
 *----------------------------------------------------------------------*
 *
 * Hints: pick a link, a path or a git hash on the screen with the
 * keyboard. hints:open, hints:copy or hints:insert put a label of one or
 * two letters on everything that looks like one; typing a label opens
 * it (url-launcher), copies it, or types it at the cursor. Escape (or a
 * key no label starts with) gives up.
 *
 * Found: hyperlinks (OSC 8), URLs, file paths (with a slash; a
 * ":line:column" after them is left out) and git hashes (7 to 40 hex
 * digits). Relative paths are opened from the shell's directory (OSC 7,
 * else the shell's own).
 *
 * While the labels show, the program's output waits (the terminal reads
 * nothing), so the screen holds still.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#if ENABLE_HINTS

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/keysym.h>

#include "rxvtbuf.h"

#define HINTS_MAX 676 // two letters

static const char hint_letters[] = "asdfjklghqweruiopzxcvbnmty";

struct hint {
  int row, col, cells; // where (ROW () rows), and how many cells
  char *text;          // what it stands for (UTF-8)
  char label[3];
  bool more; // the rest of the hint before, on the next row: no label
};

struct saved_cell {
  int row, col;
  text_t t;
  rend_t r;
};

struct term_hints {
  int action;
  hint *list;
  int count;
  char typed[3];
  saved_cell *saved;
  int nsaved;
};

/*----------------------------------------------------------------------*/
// finding things in a line of text

// a logical line: rows joined where the window wrapped them, a character
// per cell, with where each one is
struct hint_line {
  unicode_t *chars;
  int *row, *col;
  int len;
};

static bool is_space(unicode_t c) {
  return c == ' ' || c == '\t' || c == 0 || c == NOCHAR;
}

static bool in(unicode_t c, const char *set) {
  return c < 128 && c && strchr(set, (int)c);
}

static void add_text(rxvt_buf &b, const hint_line &l, int from, int to) {
  for (int i = from; i < to; i++)
    b.add_utf8(l.chars[i]);
}

// end of a token: the first space or quote
static int token_end(const hint_line &l, int i) {
  while (i < l.len && !is_space(l.chars[i]) && !in(l.chars[i], "\"'`<>|"))
    i++;
  return i;
}

// without punctuation that ends sentences, and without a closing bracket
// that doesn't close anything inside
static int trim_end(const hint_line &l, int from, int to) {
  for (;;) {
    if (to <= from)
      return to;

    unicode_t c = l.chars[to - 1];

    if (in(c, ".,;:!?"))
      to--;
    else if (c == ')' || c == ']' || c == '}') {
      unicode_t open = c == ')' ? '(' : c == ']' ? '[' : '{';
      int depth = 0;
      for (int i = from; i < to; i++)
        depth += l.chars[i] == open ? 1 : l.chars[i] == c ? -1 : 0;
      if (depth >= 0)
        return to;
      to--;
    } else
      return to;
  }
}

/*----------------------------------------------------------------------*/

bool rxvt_term::hints_overlap(int row, int col, int cells) {
  for (int i = 0; i < hints->count; i++) {
    hint &h = hints->list[i];
    if (h.row == row && col < h.col + h.cells && h.col < col + cells)
      return true;
  }

  return false;
}

void rxvt_term::hints_add(int row, int col, int cells, char *text) {
  if (hints->count >= HINTS_MAX || hints_overlap(row, col, cells)) {
    free(text);
    return;
  }

  hint &h = hints->list[hints->count++];
  h.row = row;
  h.col = col;
  h.cells = cells;
  h.text = text;
  h.label[0] = 0;
  h.more = false;
}

// a match from l.chars[from] to [to - 1]: one hint per row it covers
// (all with the same text), so labels and highlighting stay on rows
void rxvt_term::hints_add_match(const hint_line &l, int from, int to) {
  if (to <= from)
    return;

  rxvt_buf text;
  add_text(text, l, from, to);

  int start = from;
  for (int i = from; i <= to; i++)
    if (i == to || l.row[i] != l.row[start]) {
      // only the first part gets a label; the others are highlighted
      int cells = l.col[i - 1] - l.col[start] + 1;
      int before = hints->count;
      hints_add(l.row[start], l.col[start], cells,
                strdup(text.data ? text.data : ""));
      if (start != from && hints->count > before)
        hints->list[hints->count - 1].more = true;
      start = i;
    }
}

void rxvt_term::hints_scan_line(const hint_line &l) {
  // URLs: scheme://...
  for (int i = 0; i + 3 < l.len; i++)
    if (l.chars[i] == ':' && l.chars[i + 1] == '/' && l.chars[i + 2] == '/') {
      int s = i;
      while (s > 0 && l.chars[s - 1] < 128 &&
             (isalnum(l.chars[s - 1]) || in(l.chars[s - 1], "+.-")))
        s--;
      if (i - s < 2 || !isalpha(l.chars[s]))
        continue;

      int e = trim_end(l, s, token_end(l, i + 3));
      if (e > i + 3)
        hints_add_match(l, s, e);
      i = e;
    }

  // paths and git hashes: whole tokens
  for (int i = 0; i < l.len;) {
    if (is_space(l.chars[i]) || in(l.chars[i], "\"'`<>|([{=:")) {
      i++;
      continue;
    }

    int s = i, e = token_end(l, i);
    i = e;

    bool slash = false, url = false, hex = true, digit = false, letter = false;
    for (int k = s; k < e; k++) {
      unicode_t c = l.chars[k];
      slash |= c == '/';
      url |= c == ':' && k + 2 < e && l.chars[k + 1] == '/' &&
             l.chars[k + 2] == '/';
      hex &= (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      digit |= c >= '0' && c <= '9';
      letter |= c >= 'a' && c <= 'f';
    }

    if (url)
      continue;

    if (hex && digit && letter && e - s >= 7 && e - s <= 40) {
      hints_add_match(l, s, e);
      continue;
    }

    if (!slash)
      continue;

    e = trim_end(l, s, e);

    // file.c:12:5 (compilers, grep -n): the path alone
    int k = e;
    for (int part = 0; part < 2; part++) {
      int d = k;
      while (d > s && l.chars[d - 1] >= '0' && l.chars[d - 1] <= '9')
        d--;
      if (d < k && d > s && l.chars[d - 1] == ':')
        k = d - 1;
      else
        break;
    }
    e = k;

    // something more than slashes and dots
    bool real = false;
    for (int k = s; k < e; k++)
      real |= !in(l.chars[k], "/.~");

    if (real)
      hints_add_match(l, s, e);
  }
}

void rxvt_term::hints_find() {
  int first = view_start, last = view_start + nrow; // the rows shown

  // hyperlinks (OSC 8): their address
  if (links)
    for (int row = first; row < last; row++) {
      line_t &l = ROW(row);
      if (!l.valid() || !l.r)
        continue;

      for (int col = 0; col < ncol;) {
        int link = GET_LINK(l.r[col]);
        int start = col++;

        if (!link)
          continue;

        while (col < ncol && GET_LINK(l.r[col]) == link)
          col++;

        hints_add(row, start, col - start, strdup(links[link].uri));
      }
    }

  // the text, a logical line at a time
  int maxlen = (last - first) * ncol + 1;
  hint_line l;
  l.chars = (unicode_t *)rxvt_malloc(maxlen * sizeof(unicode_t));
  l.row = (int *)rxvt_malloc(maxlen * sizeof(int));
  l.col = (int *)rxvt_malloc(maxlen * sizeof(int));

  for (int row = first; row < last;) {
    l.len = 0;

    do {
      line_t &line = ROW(row);

      if (line.valid() && line.t)
        for (int col = 0; col < ncol; col++) {
          text_t t = line.t[col];

          if (t == NOCHAR)
            continue;

#if ENABLE_SIXEL
          if (IS_IMAGE_TILE(t))
            t = ' ';
#endif
#if ENABLE_COMBINING
          if (IS_COMPOSE(t)) {
            compose_char *cc;
            while ((cc = rxvt_composite[t]))
              t = cc->c1; // the base character is enough here
          }
#endif
          l.chars[l.len] = t;
          l.row[l.len] = row;
          l.col[l.len] = col;
          l.len++;
        }
    } while (ROW(row++).is_longer() && row < last);

    hints_scan_line(l);
  }

  free(l.chars);
  free(l.row);
  free(l.col);
}

/*----------------------------------------------------------------------*/

void rxvt_term::hints_set_cell(int row, int col, text_t t, rend_t r) {
  line_t &l = ROW(row);

  if (!l.valid() || !l.t || col >= ncol)
    return;

  l.t[col] = t;
  l.r[col] = r;
  l.touch();
}

// the labels (of the hints still possible) on the screen
void rxvt_term::hints_draw() {
  term_hints *h = hints;

  // the screen as it was
  for (int i = 0; i < h->nsaved; i++)
    hints_set_cell(h->saved[i].row, h->saved[i].col, h->saved[i].t,
                   h->saved[i].r);

  size_t typed = strlen(h->typed);
  rend_t label =
      SET_FGCOLOR(SET_BGCOLOR(DEFAULT_RSTYLE, Color_Yellow), Color_Black);

  for (int i = 0; i < h->count; i++) {
    hint &m = h->list[i];
    // the rest of a hint on the next rows goes with its label
    int k = i;
    while (k > 0 && h->list[k].more)
      k--;
    const char *lab = h->list[k].label;

    if (strncmp(lab, h->typed, typed))
      continue;

    line_t &l = ROW(m.row);
    if (!l.valid() || !l.r)
      continue;

    for (int c = 0; c < m.cells && m.col + c < ncol; c++) {
      rend_t r = l.r[m.col + c];
      hints_set_cell(m.row, m.col + c, l.t[m.col + c],
                     (r & ~RS_ulStyleMask) | RS_Uline);
    }

    if (!m.more)
      for (int c = typed; lab[c] && m.col + c - (int)typed < ncol; c++)
        hints_set_cell(m.row, m.col + c - typed, lab[c],
                       SET_FONT(label, FONTSET(label)->find_font(lab[c])));
  }

  want_refresh = 1;
  scr_touch(false);
  refresh_check();
}

void rxvt_term::hints_start(int action) {
  if (hints)
    hints_end();

  hints = new term_hints;
  memset(hints, 0, sizeof(*hints));
  hints->action = action;
  hints->list = (hint *)rxvt_calloc(HINTS_MAX, sizeof(hint));

  hints_find();

  if (!hints->count) {
    hints_end();
    scr_bell();
    return;
  }

  // in the order of the screen (the rest of a hint stays right after it:
  // it starts a row)
  qsort(hints->list, hints->count, sizeof(hint),
        [](const void *a, const void *b) {
          const hint *x = (const hint *)a, *y = (const hint *)b;
          return x->row != y->row ? (x->row < y->row ? -1 : 1)
                                  : x->col - y->col;
        });

  // labels: the bottom ones (nearest the prompt) get the first letters;
  // two letters each when one isn't enough
  int labelled = 0;
  for (int i = 0; i < hints->count; i++)
    labelled += !hints->list[i].more;

  int n = 0;
  for (int i = hints->count; i--;) {
    hint &m = hints->list[i];
    if (m.more)
      continue;

    if (labelled <= 26) {
      m.label[0] = hint_letters[n];
      m.label[1] = 0;
    } else {
      m.label[0] = hint_letters[n / 26];
      m.label[1] = hint_letters[n % 26];
    }
    n++;
  }

  // what is under the labels and the highlighting, to put back
  int cells = 0;
  for (int i = 0; i < hints->count; i++)
    cells += max(hints->list[i].cells, 2);

  hints->saved = (saved_cell *)rxvt_malloc(cells * sizeof(saved_cell));
  for (int i = 0; i < hints->count; i++) {
    hint &m = hints->list[i];
    line_t &l = ROW(m.row);

    for (int c = 0; c < max(m.cells, 2) && m.col + c < ncol; c++)
      if (l.valid() && l.t) {
        saved_cell &s = hints->saved[hints->nsaved++];
        s.row = m.row;
        s.col = m.col + c;
        s.t = l.t[m.col + c];
        s.r = l.r[m.col + c];
      }
  }

  // the screen holds still meanwhile
  pty_ev.stop();

  hints_draw();
}

void rxvt_term::hints_end() {
  if (!hints)
    return;

  // (from the end: a cell saved twice gets its first value back)
  for (int i = hints->nsaved; i--;)
    hints_set_cell(hints->saved[i].row, hints->saved[i].col, hints->saved[i].t,
                   hints->saved[i].r);

  for (int i = 0; i < hints->count; i++)
    free(hints->list[i].text);

  free(hints->list);
  free(hints->saved);
  delete hints;
  hints = 0;

  if (pty && pty->pty >= 0)
    pty_ev.start(pty->pty, ev::READ);

  want_refresh = 1;
  scr_touch(false);
  refresh_check();
}

// a path as the shell sees it: relative to its directory
char *rxvt_term::hints_path(const char *text) {
  if (*text == '/')
    return strdup(text);

  if (text[0] == '~' && text[1] == '/') {
    const char *home = getenv("HOME");
    char *p = (char *)rxvt_malloc(strlen(home ? home : "") + strlen(text));
    sprintf(p, "%s%s", home ? home : "", text + 1);
    return p;
  }

  char dir[PATH_MAX] = "";

  if (shell_cwd)
    snprintf(dir, sizeof(dir), "%s", shell_cwd);
  else {
    char proc[64];
    snprintf(proc, sizeof(proc), "/proc/%d/cwd", (int)cmd_pid);
    ssize_t n = readlink(proc, dir, sizeof(dir) - 1);
    dir[n > 0 ? n : 0] = 0;
  }

  if (!*dir)
    return strdup(text);

  char *p = (char *)rxvt_malloc(strlen(dir) + strlen(text) + 2);
  sprintf(p, "%s/%s", dir, text);
  return p;
}

void rxvt_term::hints_act(hint &m) {
  char *text = strdup(m.text);
  int action = hints->action;

  hints_end();

  bool hash = strspn(text, "0123456789abcdef") == strlen(text);
  bool url = strstr(text, "://") != 0;

  if (action == HINTS_OPEN && !hash) {
    char *target = url ? strdup(text) : hints_path(text);
    const char *launcher = x_resource("url-launcher");
    if (!launcher || !*launcher)
      launcher = "xdg-open";

    char *command = (char *)rxvt_malloc(strlen(launcher) + 8);
    sprintf(command, "%s \"$1\"", launcher);
    spawn(command, target);
    free(command);
    free(target);
  } else if (action == HINTS_INSERT) {
    wchar_t *w = rxvt_utf8towcs(text);
    char *mb = rxvt_wcstombs(w);
    paste(mb, strlen(mb));
    free(mb);
    free(w);
  } else // copy (and "open" a git hash: copy it)
  {
    wchar_t *w = rxvt_utf8towcs(text);
    set_selection(w, true, true);
    free(w);
  }

  free(text);
}

// a key while the labels show
void rxvt_term::hints_key(XKeyEvent &ev) {
  char buf[16];
  KeySym keysym;
  int len = XLookupString(&ev, buf, sizeof(buf) - 1, &keysym, 0);

  if (IsModifierKey(keysym))
    return;

  if (keysym == XK_BackSpace) {
    size_t n = strlen(hints->typed);
    if (n)
      hints->typed[n - 1] = 0;
    hints_draw();
    return;
  }

  size_t n = strlen(hints->typed);
  char c = len == 1 ? buf[0] : 0;

  if (keysym == XK_Escape || !c || !strchr(hint_letters, c) || n >= 2) {
    hints_end();
    return;
  }

  hints->typed[n] = c;
  hints->typed[n + 1] = 0;

  int possible = 0;
  for (int i = 0; i < hints->count; i++) {
    hint &m = hints->list[i];
    if (m.more)
      continue;

    if (!strcmp(m.label, hints->typed)) {
      hints_act(m);
      return;
    }

    possible += !strncmp(m.label, hints->typed, n + 1);
  }

  if (!possible)
    hints_end();
  else
    hints_draw();
}

#endif
