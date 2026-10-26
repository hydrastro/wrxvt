/*----------------------------------------------------------------------*
 * File:	scrollback.C
 *----------------------------------------------------------------------*
 *
 * The scrollback outside the window: scrollback:pager writes the
 * scrollback and the screen, with their colours, to a private file and
 * opens it with scrollbackCommand (by default less, in a new window), for
 * searching and copying with the pager's keys.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#if ENABLE_RECORD

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "rxvtbuf.h"

// less shows the colours (-R) from the end (+G); the file is removed when
// the pager ends (it may hold anything that was on the screen)
#define SCROLLBACK_COMMAND                                                     \
  RXVTNAME " -title scrollback -e sh -c 'less -R +G -- \"$1\"; rm -f -- "      \
           "\"$1\"' scrollback"

void rxvt_term::scrollback_pager() {
  rxvt_buf text;
  rows_text(text, top_row, nrow, true, "\n", true);

  // trailing empty lines (an unused screen) out
  while (text.len && text.data[text.len - 1] == '\n')
    text.len--;
  text.add('\n');

  const char *dir = getenv("XDG_RUNTIME_DIR");
  if (!dir || access(dir, W_OK))
    dir = getenv("TMPDIR");
  if (!dir || access(dir, W_OK))
    dir = "/tmp";

  char *path = (char *)rxvt_malloc(strlen(dir) + 32);
  sprintf(path, "%s/wrxvt-scrollback-XXXXXX", dir);

  int fd = mkstemp(path); // 0600
  bool ok = fd >= 0;
  for (size_t done = 0; ok && done < text.len;) {
    ssize_t n = write(fd, text.data + done, text.len - done);
    if (n <= 0)
      ok = false;
    else
      done += n;
  }

  if (fd >= 0)
    close(fd);

  if (!ok) {
    rxvt_warn("can't write the scrollback to %s: %s\n", path, strerror(errno));
    if (fd >= 0)
      unlink(path);
    free(path);
    return;
  }

  const char *command =
      rs[Rs_scrollbackCommand] ? rs[Rs_scrollbackCommand] : SCROLLBACK_COMMAND;
  char *full = (char *)rxvt_malloc(strlen(command) + 8);
  sprintf(full, "%s \"$1\"", command);
  spawn(full, path);
  free(full);
  free(path);
}

// The whole scrollback and the screen, as plain text (no colours), with
// the trailing empty lines trimmed. Shared by copy-all and dump.
static void all_text(rxvt_term *term, rxvt_buf &text) {
  term->rows_text(text, term->top_row, term->nrow, false, "\n", true);
  while (text.len && text.data[text.len - 1] == '\n')
    text.len--;
}

/*
 * scrollback:copy-all: everything written so far (the scrollback and the
 * screen) to the clipboard and the primary selection, as plain text.
 */
void rxvt_term::scrollback_copy_all() {
  rxvt_buf text;
  all_text(this, text);
  if (text.data)
    text.data[text.len] = 0;

  wchar_t *w = rxvt_utf8towcs(text.data ? text.data : "", text.len);
  set_selection(w, true, true);
  free(w);
}

/*
 * Write text to a timestamped file <prefix>-YYYYMMDD-HHMMSS.txt under
 * scrollbackDumpDirectory (by default $XDG_STATE_HOME/wrxvt, else
 * ~/.local/state/wrxvt). The path is shown as a desktop notification.
 * Used by scrollback:dump (the whole thing) and prompt:dump-output (the
 * last command).
 */
void rxvt_term::dump_text(const rxvt_buf &text, const char *prefix,
                          const char *what, const char *tmpl) {
  char path[PATH_MAX];
  time_t now = time(0);
  struct tm tm;
  localtime_r(&now, &tm);

  // A per-action template (scrollbackDumpFile / promptDumpFile) sets the
  // whole path and overwrites in place; it is strftime-expanded, and a
  // leading ~ is the home directory. Without one, a fresh timestamped file
  // <dir>/<prefix>-YYYYMMDD-HHMMSS.txt is written under the dump directory.
  bool overwrite = tmpl && *tmpl;
  if (overwrite) {
    char expanded[PATH_MAX];
    if (tmpl[0] == '~' && (tmpl[1] == '/' || tmpl[1] == '\0')) {
      const char *home = getenv("HOME");
      snprintf(expanded, sizeof expanded, "%s%s", home ? home : "", tmpl + 1);
    } else
      snprintf(expanded, sizeof expanded, "%s", tmpl);
    strftime(path, sizeof path, expanded, &tm);
  } else {
    char dir[PATH_MAX];
    const char *set = rs[Rs_scrollbackDumpDirectory];
    const char *state = getenv("XDG_STATE_HOME");
    const char *home = getenv("HOME");
    if (set && *set)
      snprintf(dir, sizeof dir, "%s", set);
    else if (state && *state)
      snprintf(dir, sizeof dir, "%s/wrxvt", state);
    else if (home && *home)
      snprintf(dir, sizeof dir, "%s/.local/state/wrxvt", home);
    else
      snprintf(dir, sizeof dir, ".");
    char fmt[48], stamp[64];
    snprintf(fmt, sizeof fmt, "%s-%%Y%%m%%d-%%H%%M%%S.txt", prefix);
    strftime(stamp, sizeof stamp, fmt, &tm);
    snprintf(path, sizeof path, "%s/%s", dir, stamp);
  }

  // mkdir -p the directory the file lives in (0700 for a fresh default
  // tree; existing directories a template points at are left as they are).
  for (char *p = path + 1; *p; p++)
    if (*p == '/') {
      *p = 0;
      mkdir(path, 0700);
      *p = '/';
    }

  int flags = O_WRONLY | O_CREAT | (overwrite ? O_TRUNC : O_EXCL);
  int fd = open(path, flags, 0600);
  bool ok = fd >= 0;
  for (size_t done = 0; ok && done < text.len;) {
    ssize_t n = write(fd, text.data + done, text.len - done);
    if (n <= 0)
      ok = false;
    else
      done += n;
  }
  if (fd >= 0)
    close(fd);

  if (ok) {
    char title[64];
    snprintf(title, sizeof title, "wrxvt: %s saved", what);
    notify_as(title, path);
  } else {
    rxvt_warn("can't write the %s to %s: %s\n", what, path, strerror(errno));
    if (fd >= 0)
      unlink(path);
  }
}

/*
 * scrollback:dump: everything written so far to a file, console-*.txt.
 */
void rxvt_term::scrollback_dump() {
  rxvt_buf text;
  all_text(this, text);
  text.add('\n');
  dump_text(text, "console", "console", rs[Rs_scrollbackDumpFile]);
}

/*
 * The output of the last command, between the shell's marks (OSC 133 C,
 * where it starts, and A, the next prompt), as plain text. While a command
 * runs, its output so far. Shells that only mark prompts: what is between
 * the last two. Returns false when there is nothing to take.
 */
bool rxvt_term::last_output_text(rxvt_buf &text) {
  if (current_screen != PRIMARY)
    return false;

  int cur = screen.cur.row;
  int prompt = INT_MIN, output = INT_MIN, previous = INT_MIN;

  for (int row = cur; row >= top_row; row--) {
    int f = ROW(row).f;

    if (output == INT_MIN && (f & LINE_OUTPUT))
      output = row;

    if (f & LINE_PROMPT) {
      if (prompt == INT_MIN)
        prompt = row;
      else if (previous == INT_MIN)
        previous = row;
    }

    if (output != INT_MIN && previous != INT_MIN)
      break;
  }

  int begin, end; // rows

  if (output != INT_MIN && output > prompt)
    begin = output, end = cur + 1; // still running
  else if (output != INT_MIN && prompt != INT_MIN &&
           (previous == INT_MIN || output > previous))
    begin = output, end = prompt; // between its C and the prompt after it
  else if (prompt != INT_MIN && previous != INT_MIN)
    begin = previous + 1, end = prompt; // only prompts are marked
  else
    return false;

  rows_text(text, begin, end, false, "\n", true);

  // without the empty lines at the end
  while (text.len &&
         (text.data[text.len - 1] == '\n' || text.data[text.len - 1] == ' '))
    text.len--;
  if (text.data)
    text.data[text.len] = 0;

  return true;
}

/*
 * prompt:copy-output: the last command's output to the clipboard and the
 * primary selection.
 */
void rxvt_term::copy_output() {
  rxvt_buf text;
  if (!last_output_text(text))
    return;
  wchar_t *w = rxvt_utf8towcs(text.data ? text.data : "", text.len);
  set_selection(w, true, true);
  free(w);
}

/*
 * prompt:dump-output: the last command's output to a file, output-*.txt.
 */
void rxvt_term::dump_output() {
  rxvt_buf text;
  if (!last_output_text(text)) {
    notify_as("wrxvt", "no command output to save (needs shell integration)");
    return;
  }
  text.add('\n');
  dump_text(text, "output", "last command", rs[Rs_promptDumpFile]);
}

#endif
