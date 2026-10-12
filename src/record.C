/*----------------------------------------------------------------------*
 * File:	record.C
 *----------------------------------------------------------------------*
 *
 * Recording the terminal as an asciinema file (asciicast v2), with the
 * record:start / record:stop actions or -record FILE: everything the
 * programs print, with its timing, and the window's size changes. Play it
 * with `asciinema play FILE`, put it on a web page with asciinema-player,
 * or make a GIF of it with agg.
 *
 * What is typed is not recorded (passwords), only what programs print,
 * as asciinema itself does by default. The recording starts with the
 * screen as it is, so a recording started in the middle of something
 * doesn't begin blank.
 *
 * Also here: the screen and scrollback as text with SGR colour codes,
 * for that first frame and for the scrollback pager (scrollback:pager).
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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rxvtbuf.h"

struct term_recording
{
  FILE *file;
  char *path;
  ev_tstamp start;
  unsigned char partial[4];   // the start of a UTF-8 character cut by a read
  int npartial;
  int cols, rows;             // the size recorded last
};

/*----------------------------------------------------------------------*/
// the screen as text with SGR codes

// the SGR parameters of a colour: 38/48 ; 5 ; N, or ; 2 ; R ; G ; B
void
rxvt_term::sgr_color (rxvt_buf &b, int idx, int base)
{
  if (idx >= minCOLOR && idx <= maxTermCOLOR)
    {
      int n = idx - minCOLOR;

      if (n < 8 && base != 58)
        b.printf (";%d", base - 8 + n);        // 30-37, 40-47
      else if (n < 16 && base != 58)
        b.printf (";%d", base + 52 + n - 8);   // 90-97, 100-107
      else
        b.printf (";%d;5;%d", base, n);
    }
  else if (idx >= minTermCOLOR24 && idx <= maxTermCOLOR24)
    {
      uint32_t c = rgb24_color[idx - minTermCOLOR24];
      b.printf (";%d;2;%d;%d;%d", base, c >> 16 & 0xff, c >> 8 & 0xff, c & 0xff);
    }
  // else the default colour: nothing
}

void
rxvt_term::sgr_rend (rxvt_buf &b, rend_t r)
{
  b.add ("\033[0");

  if (r & RS_Bold)      b.add (";1");
  if (r & RS_Faint)     b.add (";2");
  if (r & RS_Italic)    b.add (";3");
  if (r & RS_Uline)
    {
      int style = GET_ULSTYLE (r);
      if (style == UL_single)
        b.add (";4");
      else
        b.printf (";4:%d", style == UL_double ? 2 : style == UL_curly ? 3
                           : style == UL_dotted ? 4 : 5);
    }
  if (r & RS_Blink)     b.add (";5");
  if (r & RS_RVid)      b.add (";7");
  if (r & RS_Conceal)   b.add (";8");
  if (r & RS_Strike)    b.add (";9");
  if (r & RS_Overline)  b.add (";53");

  sgr_color (b, GET_BASEFG (r), 38);
  sgr_color (b, GET_BASEBG (r), 48);

  if (r & RS_ulColor)
    {
      rxvt_buf u;
      sgr_color (u, GET_ULCOLOR (r), 58);
      if (u.len)
        b.add (u.data, u.len);
    }

  b.add ('m');
}

/*
 * Rows row_begin to row_end - 1 (screen rows: 0 is the top of the screen,
 * negative ones are in the scrollback) as UTF-8 text, with SGR codes when
 * sgr is set. eol ends each line; lines that continue on the next row
 * (wrapped) are joined when join is set. The first row starts at
 * first_col.
 */
void
rxvt_term::rows_text (rxvt_buf &b, int row_begin, int row_end, bool sgr, const char *eol, bool join,
                      int first_col)
{
  rend_t current = DEFAULT_RSTYLE;
  bool styled = false;

  for (int row = row_begin; row < row_end; row++)
    {
      line_t &l = ROW (row);
      int end = 0;

      if (l.valid () && l.t)
        {
          // without the blanks at the end (unless they are coloured)
          end = ncol;
          while (end > 0 && l.t[end - 1] == ' '
                 && (!sgr || GET_BASEBG (l.r[end - 1]) == Color_bg) && !(l.r[end - 1] & RS_RVid))
            end--;

          for (int col = row == row_begin ? max (first_col, 0) : 0; col < end; col++)
            {
              text_t t = l.t[col];
              rend_t r = l.r[col] & ~(RS_Careful | RS_Sel | RS_customMask | RS_linkMask | RS_fontMask);

              if (t == NOCHAR)
                continue;

              if (sgr && r != current)
                {
                  sgr_rend (b, r);
                  current = r;
                  styled = r != DEFAULT_RSTYLE;
                }

#if ENABLE_SIXEL
              if (IS_IMAGE_TILE (t))
                b.add (' ');
              else
#endif
#if ENABLE_COMBINING
              if (IS_COMPOSE (t))
                {
                  wchar_t chars[64];
                  int n = rxvt_composite.expand (t);

                  if (n <= 64)
                    {
                      rxvt_composite.expand (t, chars);
                      for (int i = 0; i < n; i++)
                        b.add_utf8 (chars[i]);
                    }
                }
              else
#endif
                b.add_utf8 (t);
            }
        }

      if (styled && sgr)
        {
          b.add ("\033[m");
          current = DEFAULT_RSTYLE;
          styled = false;
        }

      bool wrapped = join && l.valid () && l.is_longer () && end == ncol;
      if (!wrapped && row + 1 < row_end)
        b.add (eol);
    }
}

/*----------------------------------------------------------------------*/

static void
add_json_bytes (rxvt_buf &b, term_recording *rec, const char *data, size_t len)
{
  // decode UTF-8 here, keeping a character cut at the end for next time
  unsigned char buf[4];
  size_t i = 0;

  while (i < len || rec->npartial)
    {
      unsigned char c;
      int n = 0;

      if (rec->npartial)
        {
          memcpy (buf, rec->partial, rec->npartial);
          n = rec->npartial;
          rec->npartial = 0;
        }
      else
        buf[n++] = data[i++];

      c = buf[0];
      int need = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;

      if (!need)
        {
          b.add_json_char (0xfffd);
          continue;
        }

      while (n < need && i < len && (data[i] & 0xc0) == 0x80)
        buf[n++] = data[i++];

      if (n < need)
        {
          if (i == len)
            {
              // cut by the end of this read: wait for the rest
              memcpy (rec->partial, buf, n);
              rec->npartial = n;
              return;
            }

          b.add_json_char (0xfffd);
          continue;
        }

      unicode_t u = need == 1 ? c
                  : need == 2 ? (c & 0x1f) << 6 | (buf[1] & 0x3f)
                  : need == 3 ? (c & 0x0f) << 12 | (buf[1] & 0x3f) << 6 | (buf[2] & 0x3f)
                  : (c & 0x07) << 18 | (buf[1] & 0x3f) << 12 | (buf[2] & 0x3f) << 6 | (buf[3] & 0x3f);

      b.add_json_char (u);
    }
}

static void
add_event (term_recording *rec, char type, const char *data, size_t len)
{
  rxvt_buf b;

  b.printf ("[%.6f, \"%c\", \"", max (0., ev::now () - rec->start), type);
  add_json_bytes (b, rec, data, len);
  b.add ("\"]\n");

  fwrite (b.data, 1, b.len, rec->file);
}

void
rxvt_term::record_output (const char *data, size_t len)
{
  // a new size goes in when programs draw for it
  record_resize ();
  add_event (recording, 'o', data, len);
  fflush (recording->file);
}

void
rxvt_term::record_resize ()
{
  if (!recording || (recording->cols == ncol && recording->rows == nrow))
    return;

  recording->cols = ncol;
  recording->rows = nrow;
  char size[32];
  snprintf (size, sizeof (size), "%dx%d", ncol, nrow);
  add_event (recording, 'r', size, strlen (size));
  fflush (recording->file);
}

// path: the file, or 0 for a new one in recordDirectory
void
rxvt_term::record_start (const char *path)
{
  if (recording)
    return;

  char *name;

  if (path)
    name = strdup (path);
  else
    {
      const char *dir = rs[Rs_recordDirectory];
      if (!dir || !*dir)
        dir = getenv ("HOME");
      if (!dir)
        dir = "/tmp";

      char stamp[32];
      time_t now = time (0);
      strftime (stamp, sizeof (stamp), "%Y%m%d-%H%M%S", localtime (&now));

      name = (char *)rxvt_malloc (strlen (dir) + 64);
      sprintf (name, "%s/wrxvt-%s.cast", dir, stamp);
    }

  // only for you: a recording has everything that was on the screen
  int fd = open (name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  FILE *file = fd >= 0 ? fdopen (fd, "w") : 0;

  if (!file)
    {
      char *text = (char *)rxvt_malloc (strlen (name) + 128);
      sprintf (text, "Can't record to %s: %s", name, strerror (errno));
      rxvt_warn ("%s\n", text);
      notify_as ("wrxvt: recording", text);
      free (text);
      free (name);
      if (fd >= 0)
        close (fd);
      return;
    }

  recording = new term_recording;
  recording->file = file;
  recording->path = name;
  recording->start = ev::now ();
  recording->npartial = 0;
  recording->cols = ncol;
  recording->rows = nrow;

  rxvt_buf h;
  h.printf ("{\"version\": 2, \"width\": %d, \"height\": %d, \"timestamp\": %ld, \"env\": {",
            ncol, nrow, (long)time (0));
  const char *shell = getenv ("SHELL");
  h.add ("\"SHELL\": \"");
  for (const char *p = shell ? shell : ""; *p; p++)
    h.add_json_char ((unsigned char)*p);
  h.add ("\", \"TERM\": \"");
  for (const char *p = rs[Rs_term_name] ? rs[Rs_term_name] : TERMENV; *p; p++)
    h.add_json_char ((unsigned char)*p);
  h.add ("\"}");
  if (app_title && *app_title)
    {
      h.add (", \"title\": \"");
      wchar_t *ws = rxvt_mbstowcs (app_title);
      for (wchar_t *p = ws; *p; p++)
        h.add_json_char (*p);
      free (ws);
      h.add ('"');
    }
  h.add ("}\n");
  fwrite (h.data, 1, h.len, file);

  // the screen as it is now, as the first frame
  rxvt_buf screen_now;
  screen_now.add ("\033[H\033[2J");
  rows_text (screen_now, 0, nrow, true, "\r\n", false);
  screen_now.printf ("\033[%d;%dH", screen.cur.row + 1, screen.cur.col + 1);
  add_event (recording, 'o', screen_now.data, screen_now.len);
  fflush (file);

  update_title ();
}

// quiet: the terminal is closing, the recording simply ends
void
rxvt_term::record_stop (bool quiet)
{
  if (!recording)
    return;

  fclose (recording->file);

  char *text = (char *)rxvt_malloc (strlen (recording->path) + 128);
  sprintf (text, "Saved %s (asciinema play to watch it)", recording->path);
  free (recording->path);
  delete recording;
  recording = 0;

  if (!quiet)
    {
      notify_as ("wrxvt: recording", text);
      update_title ();
    }

  free (text);
}

#endif
