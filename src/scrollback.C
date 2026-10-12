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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rxvtbuf.h"

// less shows the colours (-R) from the end (+G); the file is removed when
// the pager ends (it may hold anything that was on the screen)
#define SCROLLBACK_COMMAND \
  RXVTNAME " -title scrollback -e sh -c 'less -R +G -- \"$1\"; rm -f -- \"$1\"' scrollback"

void
rxvt_term::scrollback_pager ()
{
  rxvt_buf text;
  rows_text (text, top_row, nrow, true, "\n", true);

  // trailing empty lines (an unused screen) out
  while (text.len && text.data[text.len - 1] == '\n')
    text.len--;
  text.add ('\n');

  const char *dir = getenv ("XDG_RUNTIME_DIR");
  if (!dir || access (dir, W_OK))
    dir = getenv ("TMPDIR");
  if (!dir || access (dir, W_OK))
    dir = "/tmp";

  char *path = (char *)rxvt_malloc (strlen (dir) + 32);
  sprintf (path, "%s/wrxvt-scrollback-XXXXXX", dir);

  int fd = mkstemp (path); // 0600
  bool ok = fd >= 0;
  for (size_t done = 0; ok && done < text.len; )
    {
      ssize_t n = write (fd, text.data + done, text.len - done);
      if (n <= 0)
        ok = false;
      else
        done += n;
    }

  if (fd >= 0)
    close (fd);

  if (!ok)
    {
      rxvt_warn ("can't write the scrollback to %s: %s\n", path, strerror (errno));
      if (fd >= 0)
        unlink (path);
      free (path);
      return;
    }

  const char *command = rs[Rs_scrollbackCommand] ? rs[Rs_scrollbackCommand] : SCROLLBACK_COMMAND;
  char *full = (char *)rxvt_malloc (strlen (command) + 8);
  sprintf (full, "%s \"$1\"", command);
  spawn (full, path);
  free (full);
  free (path);
}

#endif
