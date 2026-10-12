/*----------------------------------------------------------------------*
 * File:	command.C
 *----------------------------------------------------------------------*
 *
 * All portions of code are copyright by their respective author/s.
 * Copyright (c) 1992      John Bovey, University of Kent at Canterbury <jdb@ukc.ac.uk>
 *				- original version
 * Copyright (c) 1994      Robert Nation <nation@rocket.sanders.lockheed.com>
 *				- extensive modifications
 * Copyright (c) 1995      Garrett D'Amore <garrett@netcom.com>
 *				- vt100 printing
 * Copyright (c) 1995      Steven Hirsch <hirsch@emba.uvm.edu>
 *				- X11 mouse report mode and support for
 *				  DEC "private mode" save/restore functions.
 * Copyright (c) 1995      Jakub Jelinek <jj@gnu.ai.mit.edu>
 *				- key-related changes to handle Shift+function
 *				  keys properly.
 * Copyright (c) 1997      MJ Olesen <olesen@me.queensu.ca>
 *				- extensive modifications
 * Copyright (c) 1997      Raul Garcia Garcia <rgg@tid.es>
 *				- modification and cleanups for Solaris 2.x
 *				  and Linux 1.2.x
 * Copyright (c) 1997,1998 Oezguer Kesim <kesim@math.fu-berlin.de>
 * Copyright (c) 1998-2001 Geoff Wing <gcw@pobox.com>
 *				- extensive modifications
 * Copyright (c) 1998      Alfredo K. Kojima <kojima@windowmaker.org>
 * Copyright (c) 2001      Marius Gedminas
 *				- Ctrl/Mod4+Tab works like Meta+Tab (options)
 * Copyright (c) 2003      Rob McMullen <robm@flipturn.org>
 * Copyright (c) 2003-2021 Marc Lehmann <schmorp@schmorp.de>
 * Copyright (c) 2007,2015 Emanuele Giaquinta <e.giaquinta@glauco.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *----------------------------------------------------------------------*/

/*{{{ includes: */
#include "../config.h"
#include "rxvt.h"
#include "rxvtperl.h"
#include "version.h"
#include "command.h"

#ifdef KEYSYM_RESOURCE
# include "keyboard.h"
#endif

#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#if LINUX_YIELD_HACK
# include <time.h>
#endif

/*----------------------------------------------------------------------*/

#define IS_CONTROL(ch) !((ch) & 0xffffff60UL)

#if ENABLE_FRILLS || ISO_14755

#define ISO_14755_STARTED	0x80000000UL
#define ISO_14755_51		0x40000000UL // basic (section 5.1)
#define ISO_14755_52		0x20000000UL // keycap (section 5.2)
#define ISO_14755_54		0x10000000UL // code feedback (section 5.4)
#define ISO_14755_MASK		0x0fffffffUL

#if ISO_14755
static unsigned short iso14755_symtab[] = {
  // keysym,		unicode
  XK_Left,		0x2190,
  XK_KP_Left,		0x2190,
  XK_Up,		0x2191,
  XK_KP_Up,		0x2191,
  XK_Right,		0x2192,
  XK_KP_Right,		0x2192,
  XK_Down,		0x2193,
  XK_KP_Down,		0x2193,
  XK_Linefeed,		0x21b4,

  XK_Prior,		0x21de,
  XK_Next,		0x21df,
  XK_Tab,		0x21e5,
  XK_ISO_Left_Tab,	0x21e6,
  XK_Shift_L,		0x21e7,
  XK_Shift_R,		0x21e7,

  XK_Shift_Lock,	0x21eb,
  XK_ISO_Lock,		0x21eb,
  XK_Caps_Lock,		0x21ec,
  XK_Num_Lock,		0x21ed,
  XK_ISO_Level3_Shift,	0x21ee,
  XK_ISO_Level3_Lock,	0x21ef,
  XK_ISO_Group_Lock,	0x21f0,
  XK_Home,		0x21f1,
  XK_End,		0x21f2,

  XK_Execute,		0x2318,
  XK_Begin,		0x2320,
  XK_Delete,		0x2326,
  XK_Clear,		0x2327,
  XK_BackSpace,		0x232b,
  XK_Insert,		0x2380,
  XK_KP_Enter,		0x2386,
  XK_Alt_L,		0x2387,
  XK_Alt_R,		0x2387,
  XK_Control_L,		0x2388,
  XK_Control_R,		0x2388,
  XK_Pause,		0x2389,
  XK_Break,		0x238a,
  XK_Escape,		0x238b,
  XK_Undo,		0x238c,
  XK_Print,		0x2399,
  XK_Return,		0x23ce,

  XK_space,		0x2423,

#ifdef XK_KP_Begin
  XK_KP_Prior,		0x21de,
  XK_KP_Next,		0x21df,
  XK_KP_Begin,		0x2320,
  XK_KP_Insert,		0x2380,
  XK_KP_Delete,		0x2326,
  XK_KP_Space,		0x2422,
#endif
  0,
};

void ecb_cold
rxvt_term::iso14755_54 (int x, int y)
{
  x = Pixel2Col (x);
  y = Pixel2Row (y);

  if (!IN_RANGE_EXC (x, 0, ncol)
      || !IN_RANGE_EXC (y, 0, nrow))
    return;

  for (;;)
    {
      const line_t &l = ROW(y + view_start);

      text_t t = l.t[x];

      if (t != NOCHAR || !x)
        {
          iso14755_51 (l.t[x], l.r[x], x, y, view_start);
          iso14755buf = ISO_14755_54;
          break;
        }

      x--;
    }
}

void ecb_cold
rxvt_term::iso14755_51 (unicode_t ch, rend_t r, int x, int y, int y2)
{
  rxvt_fontset *fs = FONTSET (r);
  wchar_t *chr, *alloc, ch2, **fname;
  int len;

# if ENABLE_COMBINING
  if (IS_COMPOSE (ch))
    {
      len = rxvt_composite.expand (ch);
      alloc = chr = new wchar_t[len];
      rxvt_composite.expand (ch, chr);
    }
  else
# endif
    {
      ch2 = ch;

      alloc = 0;
      chr = &ch2;
      len = 1;
    }

  char rowcol[40];
  snprintf (rowcol, sizeof rowcol, "col %d row %d @%d", x, y, y2);

  char attr[80]; // plenty

  sprintf (attr, "%08x = fg %d bg %d%s%s%s%s%s%s",
           (int)r,
           fgcolor_of (r), bgcolor_of (r),
           r & RS_Bold    ? " bold"    : "",
           r & RS_Italic  ? " italic"  : "",
           r & RS_Blink   ? " blink"   : "",
           r & RS_RVid    ? " rvid"    : "",
           r & RS_Uline   ? " uline"   : "",
           r & RS_Careful ? " careful" : "");

  int width = 0;
  fname = rxvt_temp_buf<wchar_t *> (len);
  for (int i = 0; i < len; i++)
    {
      rxvt_font *f = (*fs)[fs->find_font_idx (chr[i])];
      fname[i] = rxvt_utf8towcs (f->name);
      max_it (width, wcswidth (fname[i], wcslen (fname[i])));
    }

  max_it (width, strlen (attr));

  if (y >= 0)
    {
      y = (y >= nrow - len - 5 && x < width + 2) ? 0 : -1;
      x = 0;
    }

  scr_overlay_new (x, y, width, len * 2 + 2);

  scr_overlay_set (0, 0, rowcol);

  r = SET_STYLE (OVERLAY_RSTYLE, GET_STYLE (r));

  for (int y = 0; y < len; y++)
    {
      char buf[9];

      ch = *chr++;

      sprintf (buf, "%8x", ch);
      scr_overlay_set (0, y + 1, buf);
      scr_overlay_set (9, y + 1, '=');
# if !UNICODE_3
      if (ch >= 0x10000)
        ch = 0xfffd;
# endif
      scr_overlay_set (11, y + 1, ch, r);

      if (WCWIDTH (ch) >= 2)
        scr_overlay_set (12, y + 1, NOCHAR, r);
    }

//  {
//    char buf[4+4+3+1];
//    snprintf (buf, sizeof (buf), "(%.4d|%.4d)", x, y);
//    scr_overlay_set (0, 0, buf);
//  }
  scr_overlay_set (0, len + 1, attr);
  for (int i = 0; i < len; i++)
    {
      scr_overlay_set (0, len + 2 + i, fname[i]);
      free (fname[i]);
    }

# if ENABLE_COMBINING
  if (alloc)
    delete [] alloc;
# endif
}
#endif

void ecb_cold
rxvt_term::commit_iso14755 ()
{
  wchar_t ch = iso14755buf & ISO_14755_MASK;

  if (iso14755buf & ISO_14755_51)
    {
      char mb[MB_LEN_MAX];
      int len;

      // allow verbatim 0-bytes and control-bytes to be entered
      if (ch >= 0x20)
        len = wctomb (mb, ch);
      else
        {
          mb[0] = ch;
          len = 1;
        }

      if (len > 0)
        tt_write (mb, len);
      else
        scr_bell ();
    }

  iso14755buf = 0;
}

static int ecb_cold
hex_keyval (XKeyEvent &ev)
{
  // check whether this event corresponds to a hex digit
  // if the modifiers had not been pressed.
  for (int index = 0; index < 8; index++)
    {
      KeySym k = XLookupKeysym (&ev, index);

      if (k >= XK_KP_0 && k <= XK_KP_9) return k - XK_KP_0;
      else if (k >= XK_0 && k <= XK_9)  return k - XK_0;
      else if (k >= XK_a && k <= XK_f)  return k - XK_a + 10;
      else if (k >= XK_A && k <= XK_F)  return k - XK_A + 10;
    }

  return -1;
}
#endif

static inline KeySym ecb_cold
translate_keypad (KeySym keysym, bool kp)
{
#ifdef XK_KP_Home
  static const KeySym keypadtrans[] = {
    XK_KP_7, // XK_KP_Home
    XK_KP_4, // XK_KP_Left
    XK_KP_8, // XK_KP_Up
    XK_KP_6, // XK_KP_Right
    XK_KP_2, // XK_KP_Down
    XK_KP_9, // XK_KP_Prior
    XK_KP_3, // XK_KP_Next
    XK_KP_1, // XK_KP_End
    XK_KP_5, // XK_KP_Begin
  };

  if (IN_RANGE_INC (keysym, XK_KP_Home, XK_KP_Begin))
    {
      unsigned int index = keysym - XK_KP_Home;
      keysym = kp ? keypadtrans[index] : XK_Home + index;
    }
  else if (keysym == XK_KP_Insert)
    keysym = kp ? XK_KP_0 : XK_Insert;
# ifndef NO_DELETE_KEY
  else if (keysym == XK_KP_Delete)
    keysym = kp ? XK_KP_Decimal : XK_Delete;
# endif
#endif
  return keysym;
}

static inline int ecb_cold
map_function_key (KeySym keysym)
{
  int param = 0;

  if (IN_RANGE_INC (keysym, XK_F1, XK_F35))
    {
      param = 11 + keysym - XK_F1;
      if (keysym >= XK_F17)
        param += 4;
      else if (keysym >= XK_F15)
        param += 3;
      else if (keysym >= XK_F11)
        param += 2;
      else if (keysym >= XK_F6)
        param += 1;
    }
  else
    switch (keysym)
      {
        case XK_Find:
          param = 1;
          break;
        case XK_Insert:
          param = 2;
          break;
#ifdef DXK_Remove
        case DXK_Remove:
#endif
        case XK_Execute:
          param = 3;
          break;
        case XK_Select:
          param = 4;
          break;
        case XK_Prior:
          param = 5;
          break;
        case XK_Next:
          param = 6;
          break;
        case XK_Home:
          param = 7;
          break;
        case XK_End:
          param = 8;
          break;
        case XK_Help:
          param = 28;
          break;
        case XK_Menu:
          param = 29;
          break;
      }
  return param;
}

static inline wchar_t *
rxvt_wcsdup (const wchar_t *str, int len)
{
  wchar_t *r = (wchar_t *)rxvt_malloc ((len + 1) * sizeof (wchar_t));
  memcpy (r, str, len * sizeof (wchar_t));
  r[len] = 0;
  return r;
}

void ecb_cold
rxvt_term::key_press (XKeyEvent &ev)
{
  int ctrl, meta, shft, len;
  KeySym keysym = NoSymbol;
  char rkbuf[KBUFSZ + 1];
  char *kbuf = rkbuf + 1;

#if ISO_14755
  if (iso14755buf & ISO_14755_52)
    return;
#endif

  /*
   * use Num_Lock to toggle Keypad on/off.  If Num_Lock is off, allow an
   * escape sequence to toggle the Keypad.
   *
   * Always permit `shift' to override the current setting
   */
  shft = ev.state & ShiftMask;
  ctrl = ev.state & ControlMask;
  meta = ev.state & ModMetaMask;

  kbuf[0] = 0;

#if USE_XIM
  if (Input_Context)
    {
      Status status_return;

#if 0
#ifdef X_HAVE_UTF8_STRING
      if (enc_utf8 && 0) // currently disabled, doesn't seem to work, nor is useful
        len = Xutf8LookupString (Input_Context, &ev, kbuf,
                                 KBUFSZ, &keysym, &status_return);
      else
#endif
#endif
        {
          wchar_t wkbuf[KBUFSZ + 1];

          // the XOpenIM manpage lies about hardcoding the locale
          // at the point of XOpenIM, so temporarily switch locales
          if (rs[Rs_imLocale])
            SET_LOCALE (rs[Rs_imLocale]);

          // assume wchar_t == unicode or better
          len = XwcLookupString (Input_Context, &ev, wkbuf,
                                 KBUFSZ, &keysym, &status_return);

          if (rs[Rs_imLocale])
            SET_LOCALE (locale);

          if (status_return == XLookupChars
              || status_return == XLookupBoth)
            {
              /* make sure the user can type ctrl-@, i.e. NUL */
              if (len == 1 && *wkbuf == 0)
                {
                  kbuf[0] = 0;
                  len = 1;
                }
              else
                {
                  wkbuf[len] = 0;
                  len = wcstombs ((char *)kbuf, wkbuf, KBUFSZ);
                  if (len < 0)
                    len = 0;
                }
            }
          else
            len = 0;
        }
    }
  else
#endif
    {
      len = XLookupString (&ev, kbuf, KBUFSZ, &keysym, &compose);
    }

  if (keysym != NoSymbol)
    {
      KeySym orig_keysym = keysym;

      /* Shift + F1 - F10 generates F11 - F20 */
      if (shft && keysym >= XK_F1 && keysym <= XK_F10)
        {
          keysym += (XK_F11 - XK_F1);
          shft = 0;	/* turn off Shift */
        }

      if (keysym >= 0xFF00 && keysym <= 0xFFFF)
        {
          bool kp = priv_modes & PrivMode_aplKP ? !shft : shft;
          unsigned int newlen = 1;

          if (ev.state & ModNumLockMask)
            kp = false;

          keysym = translate_keypad (keysym, kp);

          switch (keysym)
            {
#ifndef NO_BACKSPACE_KEY
              case XK_BackSpace:
                if (priv_modes & PrivMode_HaveBackSpace)
                  {
                    kbuf[0] = (!! (priv_modes & PrivMode_BackSpace)
                               ^ !!ctrl) ? '\b' : '\177';
                    kbuf[1] = '\0';
                  }
                else
                  strcpy (kbuf, rs[Rs_backspace_key]);
                break;
#endif
#ifndef NO_DELETE_KEY
              case XK_Delete:
                strcpy (kbuf, rs[Rs_delete_key]);
                break;
#endif
              case XK_Tab:
                if (shft)
                  strcpy (kbuf, "\033[Z");
                else
                  {
#ifdef CTRL_TAB_MAKES_META
                    if (ctrl)
                      meta = 1;
#endif
#ifdef MOD4_TAB_MAKES_META
                    if (ev.state & Mod4Mask)
                      meta = 1;
#endif
                    newlen = 0;
                  }
                break;

              case XK_Up:	/* "\033[A" */
              case XK_Down:	/* "\033[B" */
              case XK_Right:	/* "\033[C" */
              case XK_Left:	/* "\033[D" */
                strcpy (kbuf, "\033[Z");
                kbuf[2] = "DACB"[keysym - XK_Left];
                /* do Shift first */
                if (shft)
                  kbuf[2] = "dacb"[keysym - XK_Left];
                else if (ctrl)
                  {
                    kbuf[1] = 'O';
                    kbuf[2] = "dacb"[keysym - XK_Left];
                  }
                else if (priv_modes & PrivMode_aplCUR)
                  kbuf[1] = 'O';
                break;

              case XK_KP_Enter:
                /* allow shift to override */
                if (kp)
                  {
                    strcpy (kbuf, "\033OM");
                    break;
                  }

                /* FALLTHROUGH */

              case XK_Return:
                if (priv_modes & PrivMode_LFNL)
                  {
                    kbuf[0] = '\015';
                    kbuf[1] = '\012';
                    kbuf[2] = '\0';
                  }
                else
                  {
                    kbuf[0] = '\015';
                    kbuf[1] = '\0';
                  }
                break;

              case XK_KP_F1:	/* "\033OP" */
              case XK_KP_F2:	/* "\033OQ" */
              case XK_KP_F3:	/* "\033OR" */
              case XK_KP_F4:	/* "\033OS" */
                strcpy (kbuf, "\033OP");
                kbuf[2] += (keysym - XK_KP_F1);
                break;

              case XK_KP_Multiply:	/* "\033Oj" : "*" */
              case XK_KP_Add:		/* "\033Ok" : "+" */
              case XK_KP_Separator:	/* "\033Ol" : "," */
              case XK_KP_Subtract:	/* "\033Om" : "-" */
              case XK_KP_Decimal:	/* "\033On" : "." */
              case XK_KP_Divide:	/* "\033Oo" : "/" */
              case XK_KP_0:		/* "\033Op" : "0" */
              case XK_KP_1:		/* "\033Oq" : "1" */
              case XK_KP_2:		/* "\033Or" : "2" */
              case XK_KP_3:		/* "\033Os" : "3" */
              case XK_KP_4:		/* "\033Ot" : "4" */
              case XK_KP_5:		/* "\033Ou" : "5" */
              case XK_KP_6:		/* "\033Ov" : "6" */
              case XK_KP_7:		/* "\033Ow" : "7" */
              case XK_KP_8:		/* "\033Ox" : "8" */
              case XK_KP_9:		/* "\033Oy" : "9" */
                /* allow shift to override */
                if (kp)
                  {
                    strcpy (kbuf, "\033Oj");
                    kbuf[2] += (keysym - XK_KP_Multiply);
                  }
                else
                  {
                    kbuf[0] = ('*' + (keysym - XK_KP_Multiply));
                    kbuf[1] = '\0';
                  }
                break;

              default:
                {
                  int param = map_function_key (keysym);
                  if (param > 0)
                    sprintf (kbuf,"\033[%d~", param);
                  else
                    newlen = 0;
                }
                break;
            }

          if (newlen)
            len = strlen (kbuf);

          if (len > 0)
            {
              /*
               * pass Shift/Control indicators for function keys ending with `~'
               *
               * eg,
               *   Prior = "ESC[5~"
               *   Shift+Prior = "ESC[5$"
               *   Ctrl+Prior = "ESC[5^"
               *   Ctrl+Shift+Prior = "ESC[5@"
               */
              if (kbuf[0] == C0_ESC && kbuf[1] == '[' && kbuf[len - 1] == '~')
                kbuf[len - 1] = (shft ? (ctrl ? '@' : '$') : (ctrl ? '^' : '~'));

              /*
               * Pass meta for all function keys, if 'meta' option set
               */
#ifdef META8_OPTION
              if (meta && (meta_char == 0x80))
                kbuf[len - 1] |= 0x80;
#endif
            }

        }
      else if (ctrl && keysym == XK_minus)
        {
          len = 1;
          kbuf[0] = '\037';	/* Ctrl-Minus generates ^_ (31) */
        }
      else if (keysym == XK_ISO_Left_Tab)
        {
          strcpy (kbuf, "\033[Z");
          len = 3;
        }
      else
        {
#ifdef META8_OPTION
          /* set 8-bit on */
          if (meta && (meta_char == 0x80))
            {
              char *ch;

              for (ch = kbuf; ch < kbuf + len; ch++)
                *ch |= 0x80;
            }
#endif
          /* nil */ ;
        }

      keysym = orig_keysym;
    }

  /* escape prefix */
  if (len && meta
#ifdef META8_OPTION
      && meta_char == C0_ESC
#endif
     )
    {
      *--kbuf = C0_ESC;
      len++;
    }

  if (HOOK_INVOKE ((this, HOOK_KEY_PRESS, DT_XEVENT, &ev, DT_INT, keysym, DT_STR_LEN, kbuf, len, DT_END)))
    return;

  if (keysym != NoSymbol)
    {
#ifdef KEYSYM_RESOURCE
      if (keyboard->dispatch (this, keysym, ev.state, kbuf, len))
        return;
#endif

      if (saveLines)
        {
#ifdef UNSHIFTED_SCROLLKEYS
          if (!ctrl && !meta)
#else
          if (IS_SCROLL_MOD)
#endif
            {
              int lnsppg;

#ifdef PAGING_CONTEXT_LINES
              lnsppg = nrow - PAGING_CONTEXT_LINES;
#else
              lnsppg = nrow * 4 / 5;
#endif
              max_it (lnsppg, 1);

              if (keysym == XK_Prior)
                {
                  scr_page (lnsppg);
                  return;
                }
              else if (keysym == XK_Next)
                {
                  scr_page (-lnsppg);
                  return;
                }
            }
#ifdef SCROLL_ON_UPDOWN_KEYS
          if (IS_SCROLL_MOD)
            {
              if (keysym == XK_Up)
                {
                  scr_page (1);
                  return;
                }
              else if (keysym == XK_Down)
                {
                  scr_page (-1);
                  return;
                }
            }
#endif
#ifdef SCROLL_ON_HOMEEND_KEYS
          if (IS_SCROLL_MOD)
            {
              if (keysym == XK_Home)
                {
                  scr_changeview (top_row);
                  return;
                }
              else if (keysym == XK_End)
                {
                  scr_changeview (0);
                  return;
                }
            }
#endif
        }

      if (shft)
        {
          if (!ctrl && !meta && (priv_modes & PrivMode_ShiftKeys))
            {
              switch (keysym)
                {
                    /* normal XTerm key bindings */
                  case XK_Insert:	/* Shift+Insert = paste mouse selection */
                    selection_request (ev.time);
                    return;
#if TODO
                    /* rxvt extras */
                  case XK_KP_Add:	/* Shift+KP_Add = bigger font */
                    return;
                  case XK_KP_Subtract:	/* Shift+KP_Subtract = smaller font */
                    return;
#endif
                }
            }
        }

      if (ctrl && meta && (keysym == XK_c || keysym == XK_v))
        {
          if (keysym == XK_v)
            selection_request (ev.time, Sel_Clipboard);
          else if (selection.len > 0)
            {
              free (selection.clip_text);
              selection.clip_text = rxvt_wcsdup (selection.text, selection.len);
              selection.clip_len = selection.len;
              selection_grab (CurrentTime, true);
            }

          return;
        }

#if ENABLE_FRILLS || ISO_14755
      // ISO 14755 support
      if (iso14755buf & (ISO_14755_STARTED | ISO_14755_51))
        {
          int hv;

          if (iso14755buf & ISO_14755_51
              && (keysym == XK_space || keysym == XK_KP_Space
                  || keysym == XK_Return || keysym == XK_KP_Enter))
            {
              commit_iso14755 ();
              iso14755buf = ISO_14755_51;
# if ISO_14755
              iso14755_51 (0);
# endif
              return;
            }
          else if (keysym == XK_BackSpace)
            {
              iso14755buf = ((iso14755buf & ISO_14755_MASK) >> 4) | ISO_14755_51;
# if ISO_14755
              iso14755_51 (iso14755buf & ISO_14755_MASK);
# endif
              return;
            }
          else if ((hv = hex_keyval (ev)) >= 0)
            {
              iso14755buf = ((iso14755buf << 4) & ISO_14755_MASK)
                          | hv | ISO_14755_51;
# if ISO_14755
              iso14755_51 (iso14755buf & ISO_14755_MASK);
# endif
              return;
            }
          else
            {
# if ISO_14755
              scr_overlay_off ();
# endif
              iso14755buf = 0;
            }
        }
      else if (option (Opt_iso14755) &&
               ((ctrl && (keysym == XK_Shift_L || keysym == XK_Shift_R))
                || (shft && (keysym == XK_Control_L || keysym == XK_Control_R))))
        if (!(iso14755buf & ISO_14755_STARTED))
          {
            iso14755buf |= ISO_14755_STARTED;
# if ISO_14755
            scr_overlay_new (0, -1, sizeof ("ISO 14755 mode") - 1, 1);
            scr_overlay_set (0, 0, "ISO 14755 mode");
# endif
          }
#endif

#ifdef PRINTPIPE
      if (keysym == XK_Print)
        {
          scr_printscreen (ctrl | shft);
          return;
        }
#endif
    }

  if (len <= 0)
    return;			/* not mapped */

  tt_write_user_input (kbuf, (unsigned int)len);
}

void ecb_cold
rxvt_term::key_release (XKeyEvent &ev)
{
#if (MOUSE_WHEEL && MOUSE_SLIP_WHEELING) || ISO_14755 || ENABLE_PERL
  KeySym keysym;

  keysym = XLookupKeysym (&ev, ev.state & ShiftMask ? 1 : 0); // sorry, only shift supported :/
#endif

#if ENABLE_FRILLS || ISO_14755
  // ISO 14755 support
  if (iso14755buf)
    if (iso14755buf & ISO_14755_52)
      {
# if ISO_14755
        scr_overlay_off ();
# endif
# if ISO_14755
        // iso14755 part 5.2 handling: release time
        // first: controls
        if ((ev.state & ControlMask)
             && ((keysym >= 0x40 && keysym <= 0x5f)
                 || (keysym >= 0x61 && keysym <= 0x7f)))
          {
            iso14755buf = ISO_14755_51 | 0x2400 | (keysym & 0x1f);
            commit_iso14755 ();

            return;
          }

        for (unsigned short *i = iso14755_symtab; i[0]; i += 2)
          if (i[0] == keysym)
            {
              iso14755buf = ISO_14755_51 | i[1];
              commit_iso14755 ();

              return;
            }

        scr_bell ();
# endif
        iso14755buf = 0;

        return;
      }
    else if ((ev.state & (ShiftMask | ControlMask)) != (ShiftMask | ControlMask))
      {
# if ISO_14755
        scr_overlay_off ();
# endif
        if (iso14755buf & ISO_14755_51)
          commit_iso14755 ();
#if ISO_14755
        else if (option (Opt_iso14755_52) && iso14755buf & ISO_14755_STARTED)
          {
            iso14755buf = ISO_14755_52; // iso14755 part 5.2: remember empty begin/end pair

            scr_overlay_new (0, -1, sizeof ("KEYCAP PICTURE INSERT MODE") - 1, 1);
            scr_overlay_set (0, 0, "KEYCAP PICTURE INSERT MODE");
          }
# endif
        else
          iso14755buf = 0;
      }
#endif

  if (HOOK_INVOKE ((this, HOOK_KEY_RELEASE, DT_XEVENT, &ev, DT_INT, keysym, DT_END)))
    return;

#if defined(MOUSE_WHEEL) && defined(MOUSE_SLIP_WHEELING)
  if (!(ev.state & ControlMask))
    slip_wheel_ev.stop ();
  else if (keysym == XK_Control_L || keysym == XK_Control_R)
    mouse_slip_wheel_speed = 0;
#endif
}

void
rxvt_term::flush ()
{
  flush_ev.stop ();

  if (priv_modes & PrivMode_SyncOutput)
    {
      // a synchronized update is in progress: draw it when it's complete
      display->flush ();
      return;
    }

#ifdef HAVE_IMG
  if (bg_flags & BG_NEEDS_REFRESH)
    {
      bg_flags &= ~BG_NEEDS_REFRESH;
      scr_touch (false);
    }
#endif

  if (want_refresh)
    {
      if (SHOULD_INVOKE (HOOK_LINE_UPDATE))
        {
          int row = view_start;
          int end_row = row + nrow;

          while (row > top_row && ROW (row - 1).is_longer ())
            --row;

          do
            {
              int start_row = row;
              line_t *l;

              do
                {
                  l = &ROW (row++);

                  if (!(l->f & LINE_FILTERED))
                    {
                      // line not filtered, mark it as filtered
                      l->f |= LINE_FILTERED;
                      while (l->is_longer ())
                        {
                          l = &ROW (row++);
                          l->f |= LINE_FILTERED;
                        }

                      // and filter it
                      HOOK_INVOKE ((this, HOOK_LINE_UPDATE, DT_INT, start_row, DT_END));

                      break;
                    }
                }
              while (l->is_longer () && row < end_row);
            }
          while (row < end_row);
        }

      scr_refresh ();
      scrollBar.show (1);
#if USE_XIM
      im_send_spot ();
#endif
    }

  display->flush ();
}

/* checks whether a refresh is requested and starts the refresh timer */
void
rxvt_term::refresh_check ()
{
  if (want_refresh && !flush_ev.is_active () && !(priv_modes & PrivMode_SyncOutput))
    flush_ev.start ();

  display->flush ();
}

/*
 * The colour scheme, as programs ask for it with CSI ? 996 n: 1 for
 * dark, 2 for light, from the background colour.
 */
int
rxvt_term::color_scheme ()
{
  rgba bg = pix_colors_focused[Color_bg];

  // relative luminance (ITU-R BT.709) of the gamma-encoded values: the
  // perceived lightness, which is what tells dark from light
  unsigned long luminance = 2126UL * bg.r + 7152UL * bg.g + 722UL * bg.b;

  return luminance > 10000UL * rgba::MAX_CC / 2 ? 2 : 1;
}

// with mode 2031 set, tell the program when the scheme changes
void
rxvt_term::color_scheme_check ()
{
  if (!(priv_modes & PrivMode_ColorScheme))
    return;

  int scheme = color_scheme ();

  if (scheme != reported_color_scheme)
    {
      reported_color_scheme = scheme;
      tt_printf ("\033[?997;%dn", scheme);
    }
}

/*
 * Synchronized output (mode 2026): while it's set, changes to the screen
 * are not drawn, so a program can redraw everything and have it appear
 * at once, without flicker or half-drawn frames. A program that never
 * resets it doesn't freeze the terminal: it ends by itself after
 * SYNC_OUTPUT_TIMEOUT seconds.
 */
void
rxvt_term::sync_output (bool on)
{
  if (on)
    {
      // a second "begin" doesn't extend the time limit
      if (!sync_ev.is_active ())
        sync_ev.start (SYNC_OUTPUT_TIMEOUT);
      priv_modes |= PrivMode_SyncOutput;
    }
  else
    {
      priv_modes &= ~PrivMode_SyncOutput;
      sync_ev.stop ();
      refresh_check ();
    }
}

void
rxvt_term::sync_cb (ev::timer &w, int revents)
{
  make_current ();

  sync_output (false);
}

void
rxvt_term::flush_cb (ev::timer &w, int revents)
{
  make_current ();

  refresh_count = 0;
  flush ();
}

#ifdef CURSOR_BLINK
void
rxvt_term::cursor_blink_reset ()
{
  if (!focus)
    return;

  if (hidden_cursor)
    {
      hidden_cursor = 0;
      want_refresh = 1;
    }

  if (option (Opt_cursorBlink) || (priv_modes & PrivMode_BlinkingCursor))
    cursor_blink_ev.again ();
  else
    cursor_blink_ev.stop ();
}

void
rxvt_term::cursor_blink_cb (ev::timer &w, int revents)
{
  hidden_cursor = !hidden_cursor;
  want_refresh = 1;
  refresh_check ();
}
#endif

#ifdef TEXT_BLINK
void
rxvt_term::text_blink_cb (ev::timer &w, int revents)
{
  if (scr_refresh_rend (RS_Blink, RS_Blink))
    {
      hidden_text = !hidden_text;
      want_refresh = 1;
      refresh_check ();
    }
  else
    w.stop ();
}
#endif

#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
void
rxvt_term::cont_scroll_cb (ev::timer &w, int revents)
{
  if ((scrollBar.state == SB_STATE_UP || scrollBar.state == SB_STATE_DOWN)
      && scr_page (scrollBar.state == SB_STATE_UP ? UP : DN, 1))
    {
      want_refresh = 1;
      refresh_check ();
    }
  else
    w.stop ();
}
#endif

#ifdef SELECTION_SCROLLING
void
rxvt_term::sel_scroll_cb (ev::timer &w, int revents)
{
  if (scr_page (scroll_selection_lines))
    {
      selection_extend (selection_save_x, selection_save_y, selection_save_state);
      want_refresh = 1;
      refresh_check ();
    }
  else
    w.stop ();
}
#endif

#if defined(MOUSE_WHEEL) && defined(MOUSE_SLIP_WHEELING)
void
rxvt_term::slip_wheel_cb (ev::timer &w, int revents)
{
  if (scr_page (mouse_slip_wheel_speed))
    {
      want_refresh = 1;
      refresh_check ();
    }

  if (view_start == top_row || view_start == 0 || mouse_slip_wheel_speed == 0)
    {
      mouse_slip_wheel_speed = 0;
      w.stop ();
    }
}
#endif

#if LINUX_YIELD_HACK
static struct event_handler
{
  ev::prepare yield_ev;

  void yield_cb (ev::prepare &w, int revents)
  {
    // this should really be sched_yield(), but the linux guys thought
    // that giving a process calling sched_yield () less cpu time than
    // ones with high nice levels is a useful thing to do. It surely is is
    // allowed by the sus... as is returning ENOSYS.
    // since the linux guys additionally thought that breaking the only
    // known workaround against their unusable sched_yield hack is cool,
    // we just nanosleep a bit and hope for the best.

    struct timespec ts = { 0, 1000 };
    nanosleep (&ts, 0);

    w.stop ();
  }

  event_handler ()
  : yield_ev (this, &event_handler::yield_cb)
  {
  }
} event_handler;
#endif

/* make sure all the cmd data is at beginning of cmdbuf */
void
rxvt_term::cmdbuf_reify ()
{
  if (cmdbuf_ptr == cmdbuf_base)
    return;

  ssize_t used = cmdbuf_endp - cmdbuf_ptr;

  memmove (cmdbuf_base, cmdbuf_ptr, used);
  cmdbuf_ptr  = cmdbuf_base;
  cmdbuf_endp = cmdbuf_ptr + used;

}

#if defined (KEYSYM_RESOURCE)
void
rxvt_term::cmdbuf_append (const char *str, size_t count)
{
  cmdbuf_reify ();

  size_t avail = cmdbuf_base + CBUFSIZ - cmdbuf_endp;

  if (count > avail)
    return;

  memcpy (cmdbuf_endp, str, count);
  cmdbuf_endp += count;

  cmd_parse ();
}
#endif

bool
rxvt_term::pty_fill ()
{
  cmdbuf_reify ();

  size_t avail = cmdbuf_base + CBUFSIZ - cmdbuf_endp;

  if (!avail)
    {
      // normally this indicates a "too long" command sequence - just drop the data we have
      cmdbuf_ptr  = cmdbuf_base;
      cmdbuf_endp = cmdbuf_ptr;
      avail       = CBUFSIZ;
    }

  ssize_t r = read (pty->pty, cmdbuf_endp, avail);

  if (r > 0)
    {
      cmdbuf_endp += r;
      return true;
    }
  else if (r < 0 && (errno == EAGAIN || errno == EINTR))
    {
#if LINUX_YIELD_HACK
      if (display->is_local)
        event_handler.yield_ev.start ();
#endif
    }
  else
    {
      pty_ev.stop ();

      if (!option (Opt_hold))
        destroy ();
    }

  return false;
}

void
rxvt_term::pty_cb (ev::io &w, int revents)
{
  make_current ();

  if (revents & ev::READ)
    // loop, but don't allow a single term to monopolize us
    for (int i = CBUFCNT; i-- && pty_fill (); )
      cmd_parse ();

  if (revents & ev::WRITE)
    pty_write ();

  refresh_check ();
}

void ecb_cold
rxvt_term::pointer_unblank ()
{
  XDefineCursor (dpy, vt, TermWin_cursor);
  recolor_cursor ();

#ifdef POINTER_BLANK
  hidden_pointer = 0;

  if (option (Opt_pointerBlank))
    pointer_ev.start (pointerBlankDelay);
#endif
}

#ifdef POINTER_BLANK
void ecb_cold
rxvt_term::pointer_blank ()
{
  if (!option (Opt_pointerBlank))
    return;

  XDefineCursor (dpy, vt, display->blank_cursor);
  XFlush (dpy);

  hidden_pointer = 1;
}

void ecb_cold
rxvt_term::pointer_cb (ev::timer &w, int revents)
{
  make_current ();

  pointer_blank ();
}
#endif

void
rxvt_term::mouse_report (XButtonEvent &ev)
{
  int button_number, state = 0;
  int x, y;
  bool release = ev.type == ButtonRelease;

#if ENABLE_FRILLS
  if (priv_modes & PrivMode_ExtMousePixels)
    {
      // SGR-Pixels: the position in pixels from the top left of the text
      // area, starting at 1 like cells
      x = 1 + clamp (ev.x, 0, Width2Pixel (ncol) - 1);
      y = 1 + clamp (ev.y, 0, Height2Pixel (nrow) - 1);
    }
  else
#endif
    {
      x = 1 + clamp (Pixel2Col (ev.x), 0, ncol - 1);
      y = 1 + clamp (Pixel2Row (ev.y), 0, nrow - 1);
    }

  if (ev.type == MotionNotify)
    {
      if (x == mouse_row && y == mouse_col)
        return;

      mouse_row = x;
      mouse_col = y;
      state += 32;
    }

  if (MEvent.button == AnyButton)
    button_number = 3;
  else
    {
      button_number = MEvent.button - Button1;
      if (button_number > 10)
        return;
      else if (button_number >= 7)
        button_number += 128 - 7;
      else if (button_number >= 3)
        button_number += 64 - 3;
    }

  if (priv_modes & PrivMode_MouseX10)
    {
      /*
       * do not report ButtonRelease
       * no state info allowed
       */
      if (release)
        return;
    }
  else
    {
      /* XTerm mouse reporting needs these values:
       *   4 = Shift
       *   8 = Meta
       *  16 = Control
       * plus will add in our own Double-Click reporting
       *  32 = Double Click
       */
      state += ((MEvent.state & ShiftMask) ? 4 : 0)
               + ((MEvent.state & ModMetaMask) ? 8 : 0)
               + ((MEvent.state & ControlMask) ? 16 : 0);
#ifdef MOUSE_REPORT_DOUBLECLICK
      state += ((MEvent.clicks > 1) ? 32 : 0);
#endif
    }

  int code = 32 + (release ? 3 : button_number) + state;

#if DEBUG_MOUSEREPORT
  fprintf (stderr, "Mouse [");
  if (state & 16)
    fputc ('C', stderr);
  if (state & 4)
    fputc ('S', stderr);
  if (state & 8)
    fputc ('A', stderr);
  if (state & 32)
    fputc ('2', stderr);
  fprintf (stderr, "]: <%d>, %d/%d\n",
          button_number,
          x,
          y);
#endif

#if ENABLE_FRILLS
  if (priv_modes & (PrivMode_ExtMouseSGR | PrivMode_ExtMousePixels))
    tt_printf ("\033[<%d;%d;%d%c",
              button_number + state,
              x,
              y,
              release ? 'm' : 'M');
  else if (priv_modes & PrivMode_ExtMouseUrxvt)
    tt_printf ("\033[%d;%d;%dM",
              code,
              x,
              y);
  else if (priv_modes & PrivMode_ExtMouseUTF8)
    if (code < 128)
      tt_printf ("\033[M%c%lc%lc",
                code,
                wint_t (32 + x),
                wint_t (32 + y));
    else
      tt_printf ("\033[M%c%c%lc%lc",
                0xc0 + (code >> 6),
                0x80 + (code & 0x3f),
                wint_t (32 + x),
                wint_t (32 + y));
  else
#endif
    tt_printf ("\033[M%c%c%c",
              code,
              32 + x,
              32 + y);
}

/*{{{ process an X event */
void ecb_hot
rxvt_term::x_cb (XEvent &ev)
{
  make_current ();

  dLocal (Display *, dpy);

  if (ev.xany.window == vt
      && SHOULD_INVOKE (HOOK_X_EVENT)
      && HOOK_INVOKE ((this, HOOK_X_EVENT, DT_XEVENT, &ev, DT_END)))
    return;

  // for XQueryPointer
  Window unused_root, unused_child;
  int unused_root_x, unused_root_y;
  unsigned int unused_mask;

  switch (ev.type)
    {
      case KeyPress:
        key_press (ev.xkey);
        break;

      case KeyRelease:
        key_release (ev.xkey);
        break;

      case ButtonPress:
        button_press (ev.xbutton);
        break;

      case ButtonRelease:
        button_release (ev.xbutton);
        break;

      case ClientMessage:
        if (ev.xclient.format == 32
            && !HOOK_INVOKE ((this, HOOK_CLIENT_MESSAGE, DT_XEVENT, &ev, DT_END)))
          {
            if (ev.xclient.message_type == xa[XA_WM_PROTOCOLS])
              {
                if (!HOOK_INVOKE ((this, HOOK_WM_PROTOCOLS, DT_XEVENT, &ev, DT_END)))
                  {
                    if (ev.xclient.data.l[0] == xa[XA_WM_DELETE_WINDOW])
                      {
                        if (!HOOK_INVOKE ((this, HOOK_WM_DELETE_WINDOW, DT_XEVENT, &ev, DT_END)))
                          destroy ();
                      }
#if ENABLE_EWMH
                    else if (ev.xclient.data.l[0] == xa[XA_NET_WM_PING])
                      XSendEvent (dpy, ev.xclient.window = display->root,
                                  False, SubstructureRedirectMask | SubstructureNotifyMask,
                                  &ev);
#endif
                  }
              }
#if ENABLE_XEMBED
            else if (ev.xclient.format == 32 && ev.xclient.message_type == xa[XA_XEMBED])
              {
                if (ev.xclient.data.l[1] == XEMBED_FOCUS_IN)
                  focus_in ();
                else if (ev.xclient.data.l[1] == XEMBED_FOCUS_OUT)
                  focus_out ();
              }
#endif
          }
        break;

        /*
         * XXX: this is not the _current_ arrangement
         * Here's my conclusion:
         * If the window is completely unobscured, use bitblt's
         * to scroll. Even then, they're only used when doing partial
         * screen scrolling. When partially obscured, we have to fill
         * in the GraphicsExpose parts, which means that after each refresh,
         * we need to wait for the graphics expose or Noexpose events,
         * which ought to make things real slow!
         */
      case VisibilityNotify:
        switch (ev.xvisibility.state)
          {
            case VisibilityUnobscured:
              refresh_type = FAST_REFRESH;
              break;
            case VisibilityPartiallyObscured:
              refresh_type = SLOW_REFRESH;
              break;
            default:
              refresh_type = NO_REFRESH;
              break;
          }
        break;

      case FocusIn:
        if (ev.xfocus.detail != NotifyInferior
            && ev.xfocus.detail != NotifyPointer
            && ev.xfocus.mode != NotifyGrab)
          focus_in ();
        break;

      case FocusOut:
        if (ev.xfocus.detail != NotifyInferior
            && ev.xfocus.detail != NotifyPointer
            && ev.xfocus.mode != NotifyGrab)
          focus_out ();
        break;

      case ConfigureNotify:
        if (ev.xconfigure.window == parent)
          {
            while (XCheckTypedWindowEvent (dpy, ev.xconfigure.window, ConfigureNotify, &ev))
              ;

            bool want_position_change = SHOULD_INVOKE (HOOK_POSITION_CHANGE);

            if (want_position_change)
              {
                int x, y;

                if (ev.xconfigure.send_event)
                  {
                    x = ev.xconfigure.x;
                    y = ev.xconfigure.y;
                  }
                else
                  get_window_origin (x, y);

                if (x != parent_x || y != parent_y)
                  {
                    parent_x = x;
                    parent_y = y;
                    HOOK_INVOKE ((this, HOOK_POSITION_CHANGE, DT_INT, x, DT_INT, y, DT_END));
                  }
              }

            if (szHint.width != ev.xconfigure.width || szHint.height != ev.xconfigure.height)
              {
                seen_resize = 1;
                resize_all_windows (ev.xconfigure.width, ev.xconfigure.height, 1);
              }

            HOOK_INVOKE ((this, HOOK_CONFIGURE_NOTIFY, DT_XEVENT, &ev, DT_END));
          }
        break;

      case PropertyNotify:
        HOOK_INVOKE ((this, HOOK_PROPERTY_NOTIFY, DT_XEVENT, &ev, DT_END));
        break;

      case SelectionClear:
        selection_clear (ev.xselectionclear.selection == xa[XA_CLIPBOARD]);
        break;

      case SelectionRequest:
        selection_send (ev.xselectionrequest);
        break;

      case MapNotify:
        mapped = 1;
#ifdef TEXT_BLINK
        text_blink_ev.start ();
#endif
        HOOK_INVOKE ((this, HOOK_MAP_NOTIFY, DT_XEVENT, &ev, DT_END));
        break;

      case UnmapNotify:
        mapped = 0;
#ifdef TEXT_BLINK
        text_blink_ev.stop ();
#endif
        HOOK_INVOKE ((this, HOOK_UNMAP_NOTIFY, DT_XEVENT, &ev, DT_END));
        break;

      case GraphicsExpose:
      case Expose:
        if (ev.xany.window == vt)
          {
            do
              {
                scr_expose (ev.xexpose.x, ev.xexpose.y,
                            ev.xexpose.width, ev.xexpose.height, false);
              }
            while (XCheckTypedWindowEvent (dpy, vt, ev.xany.type, &ev));

            ev.xany.type = ev.xany.type == Expose ? GraphicsExpose : Expose;

            while (XCheckTypedWindowEvent (dpy, vt, ev.xany.type, &ev))
              {
                scr_expose (ev.xexpose.x, ev.xexpose.y,
                            ev.xexpose.width, ev.xexpose.height, false);
              }

            want_refresh = 1;
          }
        else
          {
            XEvent unused_event;

            while (XCheckTypedWindowEvent (dpy, ev.xany.window, Expose, &unused_event))
              ;
            while (XCheckTypedWindowEvent (dpy, ev.xany.window, GraphicsExpose, &unused_event))
              ;

            if (scrollBar.state && ev.xany.window == scrollBar.win)
              {
                scrollBar.state = SB_STATE_IDLE;
                scrollBar.show (0);
              }
          }
        break;

      case MotionNotify:
#ifdef POINTER_BLANK
        if (hidden_pointer)
          pointer_unblank ();
#endif
        if (!bypass_keystate
            && ((priv_modes & PrivMode_MouseBtnEvent && ev.xbutton.state & (Button1Mask|Button2Mask|Button3Mask))
                || priv_modes & PrivMode_MouseAnyEvent))
          mouse_report (ev.xbutton);
        if ((priv_modes & PrivMode_mouse_report) && !bypass_keystate)
          break;

        if (ev.xany.window == vt)
          {
            if (SHOULD_INVOKE (HOOK_MOTION_NOTIFY)
                && HOOK_INVOKE ((this, HOOK_MOTION_NOTIFY, DT_XEVENT, &ev, DT_END)))
              ; // nop
            else if (ev.xbutton.state & (Button1Mask | Button3Mask))
              {
                while (XCheckTypedWindowEvent (dpy, vt, MotionNotify, &ev))
                  ;

                XQueryPointer (dpy, vt,
                               &unused_root, &unused_child,
                               &unused_root_x, &unused_root_y,
                               &ev.xbutton.x, &ev.xbutton.y,
                               &ev.xbutton.state);
#ifdef MOUSE_THRESHOLD
                /* deal with a `jumpy' mouse */
                if (ev.xmotion.time - MEvent.time > MOUSE_THRESHOLD)
#endif
                  {
#if ISO_14755
                    // 5.4
                    if (iso14755buf & (ISO_14755_STARTED | ISO_14755_54))
                      {
                        iso14755_54 (ev.xbutton.x, ev.xbutton.y);
                        break;
                      }
#endif
                    selection_extend (ev.xbutton.x, ev.xbutton.y,
                                      ev.xbutton.state & Button3Mask ? 2 : 0);

#ifdef SELECTION_SCROLLING
                    if (ev.xbutton.y < int_bwidth
                        || Pixel2Row (ev.xbutton.y) > (nrow-1))
                      {
                        page_dirn scroll_selection_dir;
                        int dist;

                        /* don't clobber the current delay if we are
                         * already in the middle of scrolling.
                         */
                        if (!sel_scroll_ev.is_active ())
                          sel_scroll_ev.start (SCROLLBAR_INITIAL_DELAY, SCROLLBAR_CONTINUOUS_DELAY);

                        /* save the event params so we can highlight
                         * the selection in the pending-scroll loop
                         */
                        selection_save_x = ev.xbutton.x;
                        selection_save_y = ev.xbutton.y;
                        selection_save_state = (ev.xbutton.state & Button3Mask) ? 2 : 0;

                        /* calc number of lines to scroll */
                        if (ev.xbutton.y < int_bwidth)
                          {
                            scroll_selection_dir = UP;
                            dist = int_bwidth - ev.xbutton.y;
                          }
                        else
                          {
                            scroll_selection_dir = DN;
                            dist = ev.xbutton.y - (int_bwidth + vt_height);
                          }

                        scroll_selection_lines = Pixel2Height (dist)
                                                 / SELECTION_SCROLL_LINE_SPEEDUP
                                                 + 1;
                        min_it (scroll_selection_lines,
                                SELECTION_SCROLL_MAX_LINES);
                        scroll_selection_lines *= scroll_selection_dir;
                      }
                    else
                      {
                        /* we are within the text window, so we
                         * shouldn't be scrolling
                         */
                        sel_scroll_ev.stop();
                      }
#endif
                  }
              }
          }
        else if (scrollBar.state == SB_STATE_MOTION && ev.xany.window == scrollBar.win)
          {
            while (XCheckTypedWindowEvent (dpy, scrollBar.win,
                                           MotionNotify, &ev))
              ;

            XQueryPointer (dpy, scrollBar.win,
                          &unused_root, &unused_child,
                          &unused_root_x, &unused_root_y,
                          &ev.xbutton.x, &ev.xbutton.y,
                          &unused_mask);
            scr_move_to (scrollBar.position (ev.xbutton.y) - csrO,
                         scrollBar.size ());
            want_refresh = 1;
            scrollBar.show (1);
          }
        break;
    }

#if defined(CURSOR_BLINK)
  if (ev.type == KeyPress)
    cursor_blink_reset ();
#endif

#if defined(POINTER_BLANK)
  if (option (Opt_pointerBlank) && pointerBlankDelay > 0)
    {
      if (ev.type == MotionNotify
          || ev.type == ButtonPress
          || ev.type == ButtonRelease)
        if (hidden_pointer)
          pointer_unblank ();

      if (ev.type == KeyPress && hidden_pointer == 0)
        pointer_blank ();
    }
#endif

  refresh_check ();
}

#if ENABLE_FRILLS
void ecb_cold
rxvt_term::set_urgency (bool enable)
{
  if (enable == urgency_hint)
    return;

  if (XWMHints *h = XGetWMHints (dpy, parent))
    {
      h->flags = h->flags & ~XUrgencyHint | (enable ? XUrgencyHint : 0);
      XSetWMHints (dpy, parent, h);
      urgency_hint = enable;
      XFree (h);
    }
}
#endif

void ecb_cold
rxvt_term::focus_in ()
{
  if (!focus)
    {
      focus = 1;
      want_refresh = 1;

#if USE_XIM
      if (Input_Context != NULL)
        {
          im_set_position ();
          XSetICFocus (Input_Context);
        }
#endif
#if CURSOR_BLINK
      if (option (Opt_cursorBlink))
        cursor_blink_ev.again ();
#endif
#if OFF_FOCUS_FADING
      if (rs[Rs_fade])
        {
          pix_colors = pix_colors_focused;
          scr_recolor ();
        }
#endif
#if ENABLE_FRILLS
      if (option (Opt_urgentOnBell))
        set_urgency (0);

      if (priv_modes & PrivMode_FocusEvent)
        tt_printf ("\x1b[I");
#endif

      HOOK_INVOKE ((this, HOOK_FOCUS_IN, DT_END));
    }
}

void ecb_cold
rxvt_term::focus_out ()
{
  if (focus)
    {
      focus = 0;
      want_refresh = 1;

#if ENABLE_FRILLS
      if (option (Opt_urgentOnBell))
        set_urgency (0);

      if (priv_modes & PrivMode_FocusEvent)
        tt_printf ("\x1b[O");
#endif
#if ENABLE_FRILLS || ISO_14755
      if (iso14755buf)
        {
          iso14755buf = 0;
# if ISO_14755
          scr_overlay_off ();
# endif
        }
#endif
#if USE_XIM
      if (Input_Context != NULL)
        XUnsetICFocus (Input_Context);
#endif
#if CURSOR_BLINK
      if (option (Opt_cursorBlink))
        cursor_blink_ev.stop ();

      hidden_cursor = 0;
#endif
#if OFF_FOCUS_FADING
      if (rs[Rs_fade])
        {
          pix_colors = pix_colors_unfocused;
          scr_recolor ();
        }
#endif

      HOOK_INVOKE ((this, HOOK_FOCUS_OUT, DT_END));
    }
}

void ecb_cold
rxvt_term::update_fade_color (unsigned int idx, bool first_time)
{
#if OFF_FOCUS_FADING
  if (rs[Rs_fade])
    {
      if (!first_time)
        pix_colors_focused [idx].free (this);

      rgba c;
      pix_colors [Color_fade].get (c);
      pix_colors_focused [idx].fade (this, atoi (rs[Rs_fade]), pix_colors_unfocused [idx], c);
    }
#endif
}

#if ENABLE_PERL
void ecb_hot
rxvt_term::rootwin_cb (XEvent &ev)
{
  make_current ();

  if (SHOULD_INVOKE (HOOK_ROOT_EVENT)
      && HOOK_INVOKE ((this, HOOK_ROOT_EVENT, DT_XEVENT, &ev, DT_END)))
    return;

  switch (ev.type)
    {
      case PropertyNotify:
        /*
         * if user used some Esetroot compatible prog to set the root bg,
         * use the property to determine the pixmap.  We use it later on.
         */
        if (ev.xproperty.atom == xa[XA_XROOTPMAP_ID]
            || ev.xproperty.atom == xa[XA_ESETROOT_PMAP_ID])
          {
            HOOK_INVOKE ((this, HOOK_ROOTPMAP_CHANGE, DT_END));
          }

        break;
    }

  refresh_check ();
}
#endif

void
rxvt_term::button_press (XButtonEvent &ev)
{
  int reportmode = 0, clickintime;

  bypass_keystate = ev.state & (ModMetaMask | ShiftMask);

  if (!bypass_keystate)
    reportmode = !! (priv_modes & PrivMode_mouse_report);

  /*
   * VT window processing of button press
   */
  if (ev.window == vt)
    {
      if (HOOK_INVOKE ((this, HOOK_BUTTON_PRESS, DT_XEVENT, &ev, DT_END)))
        return;

#if ISO_14755
      // 5.4
      if (iso14755buf & (ISO_14755_STARTED | ISO_14755_54))
        {
          iso14755_54 (ev.x, ev.y);
          return;
        }
#endif

      clickintime = ev.time - MEvent.time < multiClickTime;

      if (reportmode)
        {
          /* mouse report from vt window */
          /* save the xbutton state (for ButtonRelease) */
          MEvent.state = ev.state;
#ifdef MOUSE_REPORT_DOUBLECLICK
          if (ev.button == MEvent.button && clickintime)
            {
              /* same button, within allowed time */
              MEvent.clicks++;

              if (MEvent.clicks > 1)
                {
                  /* only report double clicks */
                  MEvent.clicks = 2;
                  mouse_report (ev);

                  /* don't report the release */
                  MEvent.clicks = 0;
                  MEvent.button = AnyButton;
                }
            }
          else
            {
              /* different button, or time expired */
              MEvent.clicks = 1;
              MEvent.button = ev.button;
              mouse_report (ev);
            }
#else
          MEvent.button = ev.button;
          mouse_report (ev);
#endif /* MOUSE_REPORT_DOUBLECLICK */

        }
      else
        {
          if (ev.button != MEvent.button)
            MEvent.clicks = 0;

          switch (ev.button)
            {
              case Button1:
                /* allow meta + click to select rectangular areas */
                /* should be done in screen.C */
#if ENABLE_FRILLS
                selection.rect = !!(ev.state & ModMetaMask);
#else
                selection.rect = false;
#endif

                /* allow shift+left click to extend selection */
                if (ev.state & ShiftMask && !(priv_modes & PrivMode_mouse_report))
                  {
                    if (MEvent.button == Button1 && clickintime)
                      selection_rotate (ev.x, ev.y);
                    else
                      selection_extend (ev.x, ev.y, 1);
                  }
                else
                  {
                    if (MEvent.button == Button1 && clickintime)
                      MEvent.clicks++;
                    else
                      MEvent.clicks = 1;

                    selection_click (MEvent.clicks, ev.x, ev.y);
                  }

                MEvent.button = Button1;
                break;

              case Button3:
                if (MEvent.button == Button3 && clickintime)
                  selection_rotate (ev.x, ev.y);
                else
                  selection_extend (ev.x, ev.y, 1);

                MEvent.button = Button3;
                break;
            }
        }

      MEvent.time = ev.time;
      return;
    }

  /*
   * Scrollbar window processing of button press
   */
  if (scrollBar.state && ev.window == scrollBar.win)
    {
      page_dirn direction = NO_DIR;

      if (scrollBar.upButton (ev.y))
        direction = UP; /* up */
      else if (scrollBar.dnButton (ev.y))
        direction = DN;  /* down */

      scrollBar.state = SB_STATE_IDLE;
      /*
       * Rxvt-style scrollbar:
       * move up if mouse is above slider
       * move dn if mouse is below slider
       *
       * XTerm-style scrollbar:
       * Move display proportional to pointer location
       * pointer near top -> scroll one line
       * pointer near bot -> scroll full page
       */
#ifndef NO_SCROLLBAR_REPORT
      if (reportmode)
        {
          /*
           * Mouse report disabled scrollbar:
           * arrow buttons - send up/down
           * click on scrollbar - send pageup/down
           */
          if (direction == UP)
            tt_printf ("\033[A");
          else if (direction == DN)
            tt_printf ("\033[B");
          else
            switch (ev.button)
              {
                case Button2:
                  tt_printf ("\014");
                  break;
                case Button1:
                  tt_printf ("\033[6~");
                  break;
                case Button3:
                  tt_printf ("\033[5~");
                  break;
              }
        }
      else
#endif /* NO_SCROLLBAR_REPORT */
        {
          if (direction != NO_DIR)
            {
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
              if (!cont_scroll_ev.is_active ())
                cont_scroll_ev.start (SCROLLBAR_INITIAL_DELAY, SCROLLBAR_CONTINUOUS_DELAY);
#endif
              if (scr_page (direction, 1))
                {
                  if (direction == UP)
                    scrollBar.state = SB_STATE_UP;
                  else
                    scrollBar.state = SB_STATE_DOWN;
                }
            }
          else
            switch (ev.button)
              {
                case Button2:
                  switch (scrollBar.align)
                    {
                      case SB_ALIGN_TOP:
                        csrO = 0;
                        break;
                      case SB_ALIGN_CENTRE:
                        csrO = (scrollBar.bot - scrollBar.top) / 2;
                        break;
                      case SB_ALIGN_BOTTOM:
                        csrO = scrollBar.bot - scrollBar.top;
                        break;
                    }

                  if (scrollBar.style == SB_STYLE_XTERM
                      || scrollBar.above_slider (ev.y)
                      || scrollBar.below_slider (ev.y))
                    scr_move_to (scrollBar.position (ev.y) - csrO, scrollBar.size ());

                  scrollBar.state = SB_STATE_MOTION;
                  break;

                case Button1:
                  if (scrollBar.align == SB_ALIGN_CENTRE)
                    csrO = ev.y - scrollBar.top;
                  /* FALLTHROUGH */

                case Button3:
                  if (scrollBar.style != SB_STYLE_XTERM)
                    {
                      if (scrollBar.above_slider (ev.y))
# ifdef RXVT_SCROLL_FULL
                        scr_page (UP, nrow - 1);
# else
                        scr_page (UP, nrow / 4);
# endif
                      else if (scrollBar.below_slider (ev.y))
# ifdef RXVT_SCROLL_FULL
                        scr_page (DN, nrow - 1);
# else
                        scr_page (DN, nrow / 4);
# endif
                      else
                        scrollBar.state = SB_STATE_MOTION;
                    }
                  else
                    {
                      scr_page ((ev.button == Button1 ? DN : UP),
                                (nrow
                                 * scrollBar.position (ev.y)
                                 / scrollBar.size ()));
                    }

                  break;
              }
        }

      return;
    }
}

void
rxvt_term::button_release (XButtonEvent &ev)
{
  int reportmode = 0;

  csrO = 0;		/* reset csr Offset */
  if (!bypass_keystate)
    reportmode = !! (priv_modes & PrivMode_mouse_report);

  if (scrollBar.state == SB_STATE_UP || scrollBar.state == SB_STATE_DOWN)
    {
      scrollBar.state = SB_STATE_IDLE;
      scrollBar.show (0);
    }

#ifdef SELECTION_SCROLLING
  sel_scroll_ev.stop();
#endif

  if (ev.window == vt)
    {
      if (HOOK_INVOKE ((this, HOOK_BUTTON_RELEASE, DT_XEVENT, &ev, DT_END)))
        return;

#if ISO_14755
      // 5.4
      if (iso14755buf & (ISO_14755_STARTED | ISO_14755_54))
        return;
#endif

      if (reportmode)
        {
          /* mouse report from vt window */
          /* don't report release of wheel "buttons" */
          if (ev.button >= 4 && ev.button <= 7)
            return;
#ifdef MOUSE_REPORT_DOUBLECLICK
          /* only report the release of 'slow' single clicks */
          if (MEvent.button != AnyButton
              && (ev.button != MEvent.button
                  || (ev.time - MEvent.time
                      > multiClickTime / 2)))
            {
              MEvent.clicks = 0;
              MEvent.button = ev.button;
              mouse_report (ev);
            }
#else				/* MOUSE_REPORT_DOUBLECLICK */
          MEvent.button = ev.button;
          mouse_report (ev);
          MEvent.button = AnyButton;
#endif /* MOUSE_REPORT_DOUBLECLICK */
          return;
        }

      /*
       * dumb hack to compensate for the failure of click-and-drag
       * when overriding mouse reporting
       */
      if (priv_modes & PrivMode_mouse_report
          && bypass_keystate
          && ev.button == Button1 && MEvent.clicks <= 1)
        selection_extend (ev.x, ev.y, 0);

      // control-click on a hyperlink opens it
      if (ev.button == Button1 && ev.state & ControlMask && MEvent.clicks <= 1
          && open_link (ev.x, ev.y))
        return;

      switch (ev.button)
        {
          case Button1:
          case Button3:
            selection_make (ev.time);
            break;

          case Button2:
            if (IN_RANGE_EXC (ev.x, 0, vt_width) && IN_RANGE_EXC (ev.y, 0, vt_height)) // inside window?
              selection_request (ev.time, ev.state & ModMetaMask ? Sel_Clipboard : Sel_Primary);
            break;

#ifdef MOUSE_WHEEL
          case Button4:
          case Button5:
            {
              int lines;
              page_dirn dirn;

              dirn = ev.button == Button4 ? UP : DN;

              if (ev.state & ShiftMask)
                lines = 1;
              else if (option (Opt_mouseWheelScrollPage))
                lines = nrow - 1;
              else
                lines = 5;

# ifdef MOUSE_SLIP_WHEELING
              if (ev.state & ControlMask)
                {
                  mouse_slip_wheel_speed += dirn;
                  clamp_it (mouse_slip_wheel_speed, -nrow, nrow);

                  if (!slip_wheel_ev.is_active ())
                    slip_wheel_ev.start (SCROLLBAR_CONTINUOUS_DELAY, SCROLLBAR_CONTINUOUS_DELAY);
                }
              else
# endif
                {
                  scr_page (dirn, lines);
                  scrollBar.show (1);
                }
            }
            break;
#endif
        }
    }
}

/*}}} */

void ecb_hot
rxvt_term::cmd_parse ()
{
  wchar_t ch = NOCHAR;
  char *seq_begin; // remember start of esc-sequence here

  if (ecb_unlikely (pstr.kind))
    try
      {
        read_string (); // continue the string the last input ended in
      }
    catch (const class out_of_input &o)
      {
        return;
      }

  for (;;)
    {
      if (ecb_unlikely (ch == NOCHAR))
        {
          seq_begin = cmdbuf_ptr;
          ch = next_char ();

          if (ch == NOCHAR)
            break;
        }

      if (ecb_likely (!IS_CONTROL (ch) || ch == C0_LF || ch == C0_CR || ch == C0_HT))
        {
          if (ecb_unlikely (!seen_input))
            {
              seen_input = 1;
              // many badly-written programs (e.g. jed) contain a race condition:
              // they first read the screensize and then install a SIGWINCH handler.
              // some window managers resize the window early, and these programs
              // then sometimes get the size wrong.
              // unfortunately other programs are even more buggy and dislike
              // being sent SIGWINCH, so only do it when we were in fact being
              // resized.
              if (seen_resize && cmd_pid)
                kill (-cmd_pid, SIGWINCH);
            }

          /* Read a text string from the input buffer */
          wchar_t buf[UBUFSIZ];
          bool refreshnow = false;
          int nlines = 0;
          wchar_t *str = buf;
          wchar_t *eol = str + min (ncol, UBUFSIZ);

          for (;;)
            {
              if (ecb_unlikely (ch == NOCHAR || (IS_CONTROL (ch) && ch != C0_LF && ch != C0_CR && ch != C0_HT)))
                break;

              *str++ = ch;

              if (ecb_unlikely (ch == C0_LF || str >= eol))
                {
                  if (ch == C0_LF)
                    nlines++;

                  refresh_count++;

                  if (!option (Opt_jumpScroll) || refresh_count >= nrow - 1)
                    {
                      refresh_count = 0;

                      if (!option (Opt_skipScroll) || ev_time () > ev::now () + 1. / 60.)
                        {
                          refreshnow = true;
                          ch = NOCHAR;
                          break;
                        }
                    }

                  // scr_add_lines only works for nlines <= nrow - 1.
                  if (nlines >= nrow - 1)
                    {
                      if (!(SHOULD_INVOKE (HOOK_ADD_LINES)
                            && HOOK_INVOKE ((this, HOOK_ADD_LINES, DT_WCS_LEN, buf, str - buf, DT_END))))
                        scr_add_lines (buf, str - buf, nlines);

                      nlines = 0;
                      str = buf;
                      eol = str + min (ncol, UBUFSIZ);
                    }

                  if (str >= eol)
                    {
                      if (eol >= buf + UBUFSIZ)
                        {
                          ch = NOCHAR;
                          break;
                        }
                      else
                        eol = min (eol + ncol, buf + UBUFSIZ);
                    }

                }

              seq_begin = cmdbuf_ptr;
              ch = next_char ();
            }

          if (!(SHOULD_INVOKE (HOOK_ADD_LINES)
                && HOOK_INVOKE ((this, HOOK_ADD_LINES, DT_WCS_LEN, buf, str - buf, DT_END))))
            scr_add_lines (buf, str - buf, nlines);

          /*
           * If there have been a lot of new lines, then update the screen
           * What the heck we'll cheat and only refresh less than every page-full.
           * if skipScroll is enabled.
           */
          if (refreshnow)
            {
              scr_refresh ();
              want_refresh = 1;
            }
        }
      else
        {
          try
            {
              process_nonprinting (ch);
            }
          catch (const class out_of_input &o)
            {
              // we ran out of input, retry later - except in a string,
              // which keeps what it read and continues (see read_string)
              if (!pstr.kind)
                cmdbuf_ptr = seq_begin;
              break;
            }

          ch = NOCHAR;
        }
    }
}

// read the next character
wchar_t ecb_hot
rxvt_term::next_char () noexcept
{
  while (cmdbuf_ptr < cmdbuf_endp)
    {
      // assume 7-bit to be ascii ALWAYS (always true in POSIX)
      if (ecb_likely ((unsigned char)*cmdbuf_ptr <= 0x7f))
        return *cmdbuf_ptr++;

      wchar_t wc;
      size_t len = mbrtowc (&wc, cmdbuf_ptr, cmdbuf_endp - cmdbuf_ptr, mbstate);

      if (len == (size_t)-2)
        {
          // the mbstate stores incomplete sequences. didn't know this :/
          cmdbuf_ptr = cmdbuf_endp;
          break;
        }

      if (len == (size_t)-1)
        {
          mbstate.reset (); // reset now undefined conversion state
          // a -1 might indicate that a previous incomplete char is invalid (previous return -2)
          // in which case we "erroneously" return the next byte which might be valid.
          return (unsigned char)*cmdbuf_ptr++; // the _occasional_ latin1 character is allowed to slip through
        }

      // assume wchar == unicode
      cmdbuf_ptr += len;
      return wc & UNICODE_MASK;
    }

  return NOCHAR;
}

// read the next octet
uint32_t ecb_hot
rxvt_term::next_octet () noexcept
{
  return cmdbuf_ptr < cmdbuf_endp
         ? (unsigned char)*cmdbuf_ptr++
         : NOCHAR;
}

static class out_of_input out_of_input;

wchar_t ecb_hot
rxvt_term::cmd_getc ()
{
  wchar_t c = next_char ();

  if (c == NOCHAR)
    throw out_of_input;

  return c;
}

uint32_t ecb_hot
rxvt_term::cmd_get8 ()
{
  uint32_t c = next_octet ();

  if (c == NOCHAR)
    throw out_of_input;

  return c;
}

/*{{{ print pipe */
/*----------------------------------------------------------------------*/
#ifdef PRINTPIPE
FILE * ecb_cold
rxvt_term::popen_printer ()
{
  FILE *stream = popen (rs[Rs_print_pipe] ? rs[Rs_print_pipe] : PRINTPIPE, "w");

  if (stream == NULL)
    rxvt_warn ("can't open printer pipe, not printing.\n");

  return stream;
}

int ecb_cold
rxvt_term::pclose_printer (FILE *stream)
{
  fflush (stream);
  return pclose (stream);
}

/*
 * simulate attached vt100 printer
 */
void ecb_cold
rxvt_term::process_print_pipe ()
{
  FILE *fd = popen_printer ();

  if (!fd)
    return;

  /*
   * Send all input to the printer until either ESC[4i or ESC[?4i
   * is received.
   */
  for (int done = 0; !done; )
    {
      unsigned char buf[8];
      unicode_t ch;
      unsigned int i, len;

      if ((ch = cmd_getc ()) != C0_ESC)
        {
          if (putc (ch, fd) == EOF)
            break;		/* done = 1 */
        }
      else
        {
          len = 0;
          buf[len++] = ch;

          if ((buf[len++] = cmd_getc ()) == '[')
            {
              if ((ch = cmd_getc ()) == '?')
                {
                  buf[len++] = '?';
                  ch = cmd_getc ();
                }
              if ((buf[len++] = ch) == '4')
                {
                  if ((buf[len++] = cmd_getc ()) == 'i')
                    break;	/* done = 1 */
                }
            }

          for (i = 0; i < len; i++)
            if (putc (buf[i], fd) == EOF)
              {
                done = 1;
                break;
              }
        }
    }

  pclose_printer (fd);
}
#endif /* PRINTPIPE */
/*}}} */

enum {
  C1_40 = 0x40,
          C1_41 , C1_BPH, C1_NBH, C1_44 , C1_NEL, C1_SSA, C1_ESA,
  C1_HTS, C1_HTJ, C1_VTS, C1_PLD, C1_PLU, C1_RI , C1_SS2, C1_SS3,
  C1_DCS, C1_PU1, C1_PU2, C1_STS, C1_CCH, C1_MW , C1_SPA, C1_EPA,
  C1_SOS, C1_59 , C1_SCI, C1_CSI, CS_ST , C1_OSC, C1_PM , C1_APC,
};

/*{{{ process non-printing single characters */
void ecb_hot
rxvt_term::process_nonprinting (unicode_t ch)
{
  switch (ch)
    {
      case C0_ESC:
        process_escape_seq ();
        break;
      case C0_ENQ:	/* terminal Status */
        if (rs[Rs_answerbackstring])
          tt_write (rs [Rs_answerbackstring], strlen (rs [Rs_answerbackstring]));
        else
          tt_write (VT100_ANS, strlen (VT100_ANS));
        break;
      case C0_BEL:	/* bell */
        scr_bell ();
        break;
      case C0_BS:		/* backspace */
        scr_backspace ();
        break;
      case C0_HT:		/* tab */
        scr_tab (1);
        break;
      case C0_CR:		/* carriage return */
        scr_gotorc (0, 0, R_RELATIVE);
        break;
      case C0_VT:		/* vertical tab, form feed */
      case C0_FF:
      case C0_LF:		/* line feed */
        scr_index (UP);
        break;
      case C0_SO:		/* shift out - acs */
        scr_charset_choose (1);
        break;
      case C0_SI:		/* shift in - acs */
        scr_charset_choose (0);
        break;

#ifdef EIGHT_BIT_CONTROLS
      // 8-bit controls
      case 0x90:		/* DCS */
        process_dcs_seq ();
        break;
      case 0x9b:		/* CSI */
        process_csi_seq ();
        break;
      case 0x9d:		/* OSC */
        process_osc_seq ();
        break;
      case 0x98:		/* SOS */
      case 0x9e:		/* PM */
      case 0x9f:		/* APC */
        begin_string (C1_APC);
        break;
#endif
    }
}
/*}}} */


/*{{{ process VT52 escape sequences */
void ecb_cold
rxvt_term::process_escape_vt52 (unicode_t ch)
{
  int row, col;

  switch (ch)
    {
      case 'A':		/* cursor up */
        scr_gotorc (-1, 0, R_RELATIVE | C_RELATIVE);
        break;
      case 'B':		/* cursor down */
        scr_gotorc (1, 0, R_RELATIVE | C_RELATIVE);
        break;
      case 'C':		/* cursor right */
        scr_gotorc (0, 1, R_RELATIVE | C_RELATIVE);
        break;
      case 'D':		/* cursor left */
        scr_gotorc (0, -1, R_RELATIVE | C_RELATIVE);
        break;
      case 'H':		/* cursor home */
        scr_gotorc (0, 0, 0);
        break;
      case 'I':		/* cursor up and scroll down if needed */
        scr_index (DN);
        break;
      case 'J':		/* erase to end of screen */
        scr_erase_screen (0);
        break;
      case 'K':		/* erase to end of line */
        scr_erase_line (0);
        break;
      case 'Y':         	/* move to specified row and col */
        /* full command is 'ESC Y row col' where row and col
         * are encoded by adding 32 and sending the ascii
         * character.  eg. SPACE = 0, '+' = 13, '0' = 18,
         * etc. */
        row = cmd_getc () - ' ';
        col = cmd_getc () - ' ';
        scr_gotorc (row, col, 0);
        break;
      case 'Z':		/* identify the terminal type */
        tt_printf ("\033/Z");	/* I am a VT100 emulating a VT52 */
        break;
      case '<':		/* turn off VT52 mode */
        priv_modes &= ~PrivMode_vt52;
        break;
      case 'F':     	/* use special graphics character set */
      case 'G':           /* use regular character set */
        /* unimplemented */
        break;
      case '=':     	/* use alternate keypad mode */
      case '>':           /* use regular keypad mode */
        /* unimplemented */
        break;
    }
}
/*}}} */


/*{{{ process escape sequences */
void ecb_hot
rxvt_term::process_escape_seq ()
{
  unicode_t ch = cmd_getc ();

  if (priv_modes & PrivMode_vt52)
    {
      process_escape_vt52 (ch);
      return;
    }

  switch (ch)
    {
      case '#':
        if (cmd_getc () == '8')
          scr_E ();
        break;
      case '(':
        scr_charset_set (0, (unsigned int)cmd_getc ());
        break;
      case ')':
        scr_charset_set (1, (unsigned int)cmd_getc ());
        break;
      case '*':
        scr_charset_set (2, (unsigned int)cmd_getc ());
        break;
      case '+':
        scr_charset_set (3, (unsigned int)cmd_getc ());
        break;
#if !ENABLE_MINIMAL
      case '6':
        scr_backindex ();
        break;
#endif
      case '7':
        scr_cursor (SAVE);
        break;
      case '8':
        scr_cursor (RESTORE);
        break;
#if !ENABLE_MINIMAL
      case '9':
        scr_forwardindex ();
        break;
#endif
      // DECPAM/DECPNM
      case '=':
        priv_modes |= PrivMode_aplKP;
        break;
      case '>':
        priv_modes &= ~PrivMode_aplKP;
        break;

      case C1_40:
        cmd_getc ();
        break;
      case C1_44:
        scr_index (UP);
        break;

        /* 8.3.87: NEXT LINE */
      case C1_NEL:		/* ESC E */
        {
          wchar_t nlcr[] = { C0_LF, C0_CR };
          scr_add_lines (nlcr, ecb_array_length (nlcr), 1);
        }
        break;

#if 0 // disabled because embedded newlines can make exploits easier
        /* kidnapped escape sequence: Should be 8.3.48 */
      case C1_ESA:		/* ESC G */
        // used by original rxvt for rob nations own graphics mode
        if (cmd_getc () == 'Q' && option (Opt_insecure))
          tt_printf ("\033G0\012");	/* query graphics - no graphics */
        break;
#endif

        /* 8.3.63: CHARACTER TABULATION SET */
      case C1_HTS:		/* ESC H */
        scr_set_tab (1);
        break;

        /* 8.3.105: REVERSE LINE FEED */
      case C1_RI:			/* ESC M */
        scr_index (DN);
        break;

        /* 8.3.142: SINGLE-SHIFT TWO */
      /* case C1_SS2: break; */

        /* 8.3.143: SINGLE-SHIFT THREE */
      /* case C1_SS3: break; */

        /* 8.3.27: DEVICE CONTROL STRING */
      case C1_DCS:		/* ESC P */
        process_dcs_seq ();
        break;

        /* 8.3.110: SINGLE CHARACTER INTRODUCER */
      case C1_SCI:		/* ESC Z */
        tt_write (ESCZ_ANSWER, sizeof (ESCZ_ANSWER) - 1);
        break;			/* steal obsolete ESC [ c */

        /* 8.3.16: CONTROL SEQUENCE INTRODUCER (CSI) */
      case C1_CSI:		/* ESC [ */
        process_csi_seq ();
        break;

        /* 8.3.90: OPERATING SYSTEM COMMAND (OSC) */
      case C1_OSC:		/* ESC ] */
        process_osc_seq ();
        break;

        /* 8.3.2: APPLICATION PROGRAM COMMAND, 8.3.94: PRIVACY MESSAGE,
         * 8.3.128: START OF STRING - read and ignored */
      case C1_APC:		/* ESC _ */
      case C1_PM:		/* ESC ^ */
      case C1_SOS:		/* ESC X */
        begin_string (C1_APC);
        break;

        /* 8.3.106: RESET TO INITIAL STATE (RIS) */
      case 'c':
        mbstate.reset ();
        scr_poweron ();
        scrollBar.show (1);
        break;

        /* 8.3.79: LOCKING-SHIFT TWO (see ISO2022) */
      case 'n':
        scr_charset_choose (2);
        break;

        /* 8.3.81: LOCKING-SHIFT THREE (see ISO2022) */
      case 'o':
        scr_charset_choose (3);
        break;
    }
}
/*}}} */

/*{{{ process CONTROL SEQUENCE INTRODUCER (CSI) sequences `ESC[' */
enum {
  CSI_ICH = 0x40,
           CSI_CUU, CSI_CUD, CSI_CUF, CSI_CUB, CSI_CNL, CSI_CPL, CSI_CHA,
  CSI_CUP, CSI_CHT, CSI_ED , CSI_EL , CSI_IL , CSI_DL , CSI_EF , CSI_EA ,
  CSI_DCH, CSI_SEE, CSI_CPR, CSI_SU , CSI_SD , CSI_NP , CSI_PP , CSI_CTC,
  CSI_ECH, CSI_CVT, CSI_CBT, CSI_SRS, CSI_PTX, CSI_SDS, CSI_SIMD, CSI_5F,
  CSI_HPA, CSI_HPR, CSI_REP, CSI_DA , CSI_VPA, CSI_VPR, CSI_HVP, CSI_TBC,
  CSI_SM , CSI_MC , CSI_HPB, CSI_VPB, CSI_RM , CSI_SGR, CSI_DSR, CSI_DAQ,
  CSI_70 , CSI_71 , CSI_72 , CSI_73 , CSI_74 , CSI_75 , CSI_76 , CSI_77 ,
  CSI_78 , CSI_79 , CSI_7A , CSI_7B , CSI_7C , CSI_7D , CSI_7E , CSI_7F
};

#define make_byte(b0,b1,b2,b3,b4,b5,b6,b7)			\
    (((b7) << 7) | ((b6) << 6) | ((b5) << 5) | ((b4) << 4)	\
     | ((b3) << 3) | ((b2) << 2) | ((b1) << 1) | (b0))
#define get_byte_array_bit(array, bit)				\
    (!! ((array)[(bit) >> 3] & (1 << ((bit) & 7))))

static const unsigned char csi_defaults[] =
  {
    make_byte (1,1,1,1,1,1,1,1),	/* @, A, B, C, D, E, F, G, */
    make_byte (1,1,0,0,1,1,0,0),	/* H, I, J, K, L, M, N, O, */
    make_byte (1,0,1,1,1,1,1,0),	/* P, Q, R, S, T, U, V, W, */
    make_byte (1,1,1,0,0,0,1,0),	/* X, Y, Z, [, \, ], ^, _, */
    make_byte (1,1,1,0,1,1,1,0),	/* `, a, b, c, d, e, f, g, */
    make_byte (0,0,1,1,0,0,0,0),	/* h, i, j, k, l, m, n, o, */
    make_byte (0,0,0,0,0,0,0,0),	/* p, q, r, s, t, u, v, w, */
    make_byte (0,0,0,0,0,0,0,0),	/* x, y, z, {, |, }, ~,    */
  };

void ecb_hot
rxvt_term::process_csi_seq ()
{
  unicode_t ch, priv, prev_ch, i;
  unsigned int nargs, p;
  int n, ndef;
  int arg[ESC_ARGS] = { };
  // bit p set: arg[p] is a subparameter of the argument before it, i.e. it
  // followed a ':' (ISO 8613-6), as in "4:3" or "38:2::255:0:0"
  uint32_t sub = 0;
  bool colon = false;

  nargs = 0;

  priv = 0;
  ch = cmd_getc ();
  if ((ch >= '<' && ch <= '?') || ch == '!')
    {
      /* '<' '=' '>' '?' '!' */
      priv = ch;
      ch = cmd_getc ();
    }

  prev_ch = 0;
  /* read any numerical arguments */
  for (n = -1; ch < CSI_ICH; )
    {
      if (isdigit (ch))
        {
          if (n < 0)
            n = ch - '0';
          else
            n = n * 10 + ch - '0';
        }
      else if (ch == ';' || ch == ':')
        {
          if (nargs < ESC_ARGS)
            {
              if (colon)
                sub |= 1U << nargs;
              arg[nargs++] = n;
            }
          n = -1;
          colon = ch == ':';
        }
      else if (IS_CONTROL (ch))
        process_nonprinting (ch);

      prev_ch = ch;
      ch = cmd_getc ();
    }

  if (ch > CSI_7F)
    return;

  if (nargs < ESC_ARGS)
    {
      if (colon)
        sub |= 1U << nargs;
      arg[nargs++] = n;
    }

  i = ch - CSI_ICH;
  ndef = get_byte_array_bit (csi_defaults, i);
  for (p = 0; p < nargs; p++)
    if (arg[p] == -1)
      arg[p] = ndef;

  /*
   * private mode handling
   */
  if (priv)
    {
      switch (priv)
        {
          case '>':
            if (ch == CSI_DA)	/* secondary device attributes */
              {
                // first parameter is normally 0 for vt100, 1 for vt220, 'R' for rxvt,
                // 'U' for rxvt-unicode != 7.[34] (where it was broken).
                //
                // second parameter is xterm patch level for xterm, MMmmpp (e.g. 20703) for rxvt
                // and Mm (e.g. 72 for 7.2) for urxvt <= 7.2, 94 for urxvt <= 8.3, and 95 for later
                // versions.
                //
                tt_printf ("\033[>%d;95;0c", 'U');
              }
            else if (ch == 'q' && arg[0] == 0) /* XTVERSION: name and version */
              tt_printf ("\033P>|wrxvt(" VERSION ")\033\\");
            break;

          case '?':
            if (ch == 'h' || ch == 'l' || ch == 'r' || ch == 's' || ch == 't')
              process_terminal_mode (ch, priv, nargs, arg);
            if (prev_ch == '$' && ch == 'p')
              process_terminal_mode (ch, priv, nargs, arg);
            if (ch == CSI_DSR && arg[0] == 996) /* the colour scheme: dark or light */
              tt_printf ("\033[?997;%dn", color_scheme ());
            break;

          case '!':
            if (ch == CSI_70)
              {
                /* DECSTR: soft terminal reset, used by our terminfo since 9.06 */
                scr_soft_reset ();

                static const int pm_h[] = { 7, 25 };
                static const int pm_l[] = { 1, 3, 4, 5, 6, 9, 66, 1000, 1001, 1005, 1006, 1015, 1016, 1049 };

                process_terminal_mode ('h', 0, ecb_array_length (pm_h), pm_h);
                process_terminal_mode ('l', 0, ecb_array_length (pm_l), pm_l);
              }
          break;
        }

      return;
    }

  switch (ch)
    {
        /*
         * ISO/IEC 6429:1992 (E) CSI sequences (defaults in parentheses)
         */
#ifdef PRINTPIPE
      case CSI_MC:		/* 8.3.83: (0) MEDIA COPY */
        switch (arg[0])
          {
            case 0:			/* initiate transfer to primary aux device */
              scr_printscreen (0);
              break;
            case 5:			/* start relay to primary aux device */
              process_print_pipe ();
              break;
          }
        break;
#endif

      case CSI_CUU:		/* 8.3.22: (1) CURSOR UP */
      case CSI_VPB:		/* 8.3.160: (1) LINE POSITION BACKWARD */
        arg[0] = -arg[0];
        /* FALLTHROUGH */
      case CSI_CUD:		/* 8.3.19: (1) CURSOR DOWN */
      case CSI_VPR:		/* 8.3.161: (1) LINE POSITION FORWARD */
        scr_gotorc (arg[0], 0, RELATIVE);
        break;

      case CSI_CUB:		/* 8.3.18: (1) CURSOR LEFT */
      case CSI_HPB:		/* 8.3.59: (1) CHARACTER POSITION BACKWARD */
#ifdef ISO6429
        arg[0] = -arg[0];
#else				/* emulate common DEC VTs */
        arg[0] = arg[0] ? -arg[0] : -1;
#endif
        /* FALLTHROUGH */
      case CSI_CUF:		/* 8.3.20: (1) CURSOR RIGHT */
      case CSI_HPR:		/* 8.3.60: (1) CHARACTER POSITION FORWARD */
#ifdef ISO6429
        scr_gotorc (0, arg[0], RELATIVE);
#else				/* emulate common DEC VTs */
        scr_gotorc (0, arg[0] ? arg[0] : 1, RELATIVE);
#endif
        break;

      case CSI_CPL:		/* 8.3.13: (1) CURSOR PRECEDING LINE */
        arg[0] = -arg[0];
        /* FALLTHROUGH */
      case CSI_CNL:		/* 8.3.12: (1) CURSOR NEXT LINE */
        scr_gotorc (arg[0], 0, R_RELATIVE);
        break;

      case CSI_CHA:		/* 8.3.9: (1) CURSOR CHARACTER ABSOLUTE */
      case CSI_HPA:		/* 8.3.58: (1) CURSOR POSITION ABSOLUTE */
        scr_gotorc (0, arg[0] - 1, R_RELATIVE);
        break;

      case CSI_VPA:		/* 8.3.159: (1) LINE POSITION ABSOLUTE */
        scr_gotorc (arg[0] - 1, 0, C_RELATIVE);
        break;

      case CSI_CUP:		/* 8.3.21: (1,1) CURSOR POSITION */
      case CSI_HVP:		/* 8.3.64: (1,1) CHARACTER AND LINE POSITION */
        scr_gotorc (arg[0] - 1, nargs < 2 ? 0 : (arg[1] - 1), 0);
        break;

      case CSI_CBT:		/* 8.3.7: (1) CURSOR BACKWARD TABULATION */
        arg[0] = -arg[0];
        /* FALLTHROUGH */
      case CSI_CHT:		/* 8.3.10: (1) CURSOR FORWARD TABULATION */
        scr_tab (arg[0]);
        break;

      case CSI_ED:		/* 8.3.40: (0) ERASE IN PAGE */
        scr_erase_screen (arg[0]);
        break;

      case CSI_EL:		/* 8.3.42: (0) ERASE IN LINE */
        scr_erase_line (arg[0]);
        break;

      case CSI_ICH:		/* 8.3.65: (1) INSERT CHARACTER */
        scr_insdel_chars (arg[0], INSERT);
        break;

      case CSI_IL:		/* 8.3.68: (1) INSERT LINE */
        scr_insdel_lines (arg[0], INSERT);
        break;

      case CSI_DL:		/* 8.3.33: (1) DELETE LINE */
        scr_insdel_lines (arg[0], DELETE);
        break;

      case CSI_ECH:		/* 8.3.39: (1) ERASE CHARACTER */
        scr_insdel_chars (arg[0], ERASE);
        break;

      case CSI_DCH:		/* 8.3.26: (1) DELETE CHARACTER */
        scr_insdel_chars (arg[0], DELETE);
        break;

      case CSI_SD:		/* 8.3.114: (1) SCROLL DOWN */
        arg[0] = -arg[0];
        /* FALLTHROUGH */
      case CSI_SU:		/* 8.3.148: (1) SCROLL UP */
        scr_scroll_text (screen.tscroll, screen.bscroll, arg[0]);
        break;

      case CSI_DA:		/* 8.3.24: (0) DEVICE ATTRIBUTES */
        tt_write (VT100_ANS, sizeof (VT100_ANS) - 1);
        break;

      case CSI_SGR:		/* 8.3.118: (0) SELECT GRAPHIC RENDITION */
        process_sgr_mode (nargs, arg, sub);
        break;

      case CSI_DSR:		/* 8.3.36: (0) DEVICE STATUS REPORT */
        switch (arg[0])
          {
            case 5:			/* DSR requested */
              tt_printf ("\033[0n");
              break;
            case 6:			/* CPR requested */
              scr_report_position ();
              break;
            case 7:			/* unofficial extension */
              if (option (Opt_insecure))
                tt_printf ("%-.250s\012", rs[Rs_display_name]);
              break;
            case 8:			/* unofficial extension */
              {
                string_term st;
                st.v[0] = CHAR_ST;
                st.v[1] = '\0';
                process_xterm_seq (XTerm_title, (char *)RESNAME "-" VERSION, st); // char * cast verified
                break;
              }
          }
        break;

      case CSI_TBC:		/* 8.3.155: (0) TABULATION CLEAR */
        switch (arg[0])
          {
            case 0:			/* char tab stop cleared at active position */
              scr_set_tab (0);
              break;
              /* case 1: */		/* line tab stop cleared in active line */
              /* case 2: */		/* char tab stops cleared in active line */
            case 3:			/* all char tab stops are cleared */
              /* case 4: */		/* all line tab stops are cleared */
            case 5:			/* all tab stops are cleared */
              scr_set_tab (-1);
              break;
          }
        break;

      case CSI_CTC:		/* 8.3.17: (0) CURSOR TABULATION CONTROL */
        switch (arg[0])
          {
            case 0:			/* char tab stop set at active position */
              scr_set_tab (1);
              break;		/* = ESC H */
              /* case 1: */		/* line tab stop set at active line */
            case 2:			/* char tab stop cleared at active position */
              scr_set_tab (0);
              break;		/* = ESC [ 0 g */
              /* case 3: */		/* line tab stop cleared at active line */
              /* case 4: */		/* char tab stops cleared at active line */
            case 5:			/* all char tab stops are cleared */
              scr_set_tab (-1);
              break;		/* = ESC [ 3 g */
              /* case 6: */		/* all line tab stops are cleared */
          }
        break;

      case CSI_RM:		/* 8.3.107: RESET MODE */
        if (arg[0] == 4)
          scr_insert_mode (0);
        else if (arg[0] == 20)
          priv_modes &= ~PrivMode_LFNL;
        break;

      case CSI_SM:		/* 8.3.126: SET MODE */
        if (arg[0] == 4)
          scr_insert_mode (1);
        else if (arg[0] == 20)
          priv_modes |= PrivMode_LFNL;
        break;

      case CSI_71:		// DECSCUSR: set cursor style
        if (prev_ch == ' ')
          set_cursor_style (arg[0]);
        break;

        /*
         * PRIVATE USE beyond this point.  All CSI_7? sequences here
         */
      case CSI_72:		/* DECSTBM: set top and bottom margins */
        if (nargs == 1)
          scr_scroll_region (arg[0] - 1, MAX_ROWS - 1);
        else if (nargs == 0 || arg[0] >= arg[1])
          scr_scroll_region (0, MAX_ROWS - 1);
        else
          scr_scroll_region (arg[0] - 1, arg[1] - 1);
        break;

      case CSI_73:
        scr_cursor (SAVE);
        break;
      case CSI_75:
        scr_cursor (RESTORE);
        break;

#if !ENABLE_MINIMAL
      case CSI_74:
        process_window_ops (arg, nargs);
        break;
#endif

      case CSI_78:		/* DECREQTPARM */
        if (arg[0] == 0 || arg[0] == 1)
          tt_printf ("\033[%d;1;1;128;128;1;0x", arg[0] + 2);
        break;

      default:
        break;
    }
}
/*}}} */

// the window title (or icon name) in the locale's encoding; free () it
char *
rxvt_term::get_title (bool icon)
{
  XTextProperty prop;
  char *title = 0;

  if (XGetTextProperty (dpy, parent, &prop, icon ? XA_WM_ICON_NAME : XA_WM_NAME))
    {
      char **list;
      int count;

      if (XmbTextPropertyToTextList (dpy, &prop, &list, &count) >= Success && count > 0)
        {
          title = strdup (list[0]);
          XFreeStringList (list);
        }

      XFree (prop.value);
    }

  return title ? title : strdup ("");
}

#if !ENABLE_MINIMAL
void
rxvt_term::process_window_ops (const int *args, unsigned int nargs)
{
  int x, y;
  XWindowAttributes wattr;
  Window wdummy;

  dLocal (Display *, dpy);

  if (nargs == 0)
    return;

  switch (args[0])
    {
      /*
       * commands
       */
      case 1:			/* deiconify window */
        XMapWindow (dpy, parent);
        break;
      case 2:			/* iconify window */
        XIconifyWindow (dpy, parent, display->screen);
        break;
      case 3:			/* set position (pixels) */
        XMoveWindow (dpy, parent, args[1], args[2]);
        break;
      case 4:			/* set size (pixels) */
        set_widthheight ((unsigned int)args[2], (unsigned int)args[1]);
        break;
      case 5:			/* raise window */
        XRaiseWindow (dpy, parent);
        break;
      case 6:			/* lower window */
        XLowerWindow (dpy, parent);
        break;
      case 7:			/* refresh window */
        scr_touch (true);
        break;
      case 8:			/* set size (chars) */
        set_widthheight ((unsigned int) (args[2] * fwidth),
                         (unsigned int) (args[1] * fheight));
        break;

      //case 9: NYI, TODO, restore maximized window or maximize window
      default:
        if (args[0] >= 24)	/* set height (chars) */
          set_widthheight ((unsigned int)vt_width,
                           (unsigned int) (args[1] * fheight));
        break;

      /*
       * reports - some output format copied from XTerm
       */
      case 11:			/* report window state */
        XGetWindowAttributes (dpy, parent, &wattr);
        tt_printf ("\033[%dt", wattr.map_state == IsViewable ? 1 : 2);
        break;
      case 13:			/* report window position */
        XGetWindowAttributes (dpy, parent, &wattr);
        XTranslateCoordinates (dpy, parent, wattr.root,
                               -wattr.border_width, -wattr.border_width,
                               &x, &y, &wdummy);
        tt_printf ("\033[3;%d;%dt", x, y);
        break;
      case 14:			/* report text area size (pixels), with 2: the window's */
        if (nargs > 1 && args[1] == 2)
          {
            XGetWindowAttributes (dpy, parent, &wattr);
            tt_printf ("\033[4;%d;%dt", wattr.height, wattr.width);
          }
        else
          tt_printf ("\033[4;%d;%dt", Height2Pixel (nrow), Width2Pixel (ncol));
        break;
      case 15:			/* report screen size (pixels) */
        XGetWindowAttributes (dpy, parent, &wattr);
        tt_printf ("\033[5;%d;%dt", HeightOfScreen (wattr.screen), WidthOfScreen (wattr.screen));
        break;
      case 16:			/* report character cell size (pixels) */
        tt_printf ("\033[6;%d;%dt", fheight, fwidth);
        break;
      case 18:			/* report text area size (chars) */
        tt_printf ("\033[8;%d;%dt", nrow, ncol);
        break;
      case 19:			/* report window size (chars) */
        tt_printf ("\033[9;%d;%dt", nrow, ncol);
        break;
      case 20:			/* report icon label */
      case 21:			/* report window title */
        {
          char *s = get_title (args[0] == 20);
          tt_printf ("\033]%c%-.250s\033\\", args[0] == 20 ? 'L' : 'l', option (Opt_insecure) ? s : "");
          free (s);
        }
        break;

      /*
       * the title stack: programs (vim, for one) save the title, set their
       * own, and restore it when they exit. 0 is both, 1 the icon name,
       * 2 the title.
       */
      case 22:			/* push the title and/or icon name */
        {
          int which = nargs > 1 ? args[1] : 0;
          title_stack_entry e = { 0, 0 };

          if (which == 0 || which == 2)
            e.title = get_title (false);

          if (which == 0 || which == 1)
            e.icon = get_title (true);

          if (title_depth == TITLE_STACK_MAX)
            {
              // full: forget the oldest
              free (title_stack[0].title);
              free (title_stack[0].icon);
              memmove (title_stack, title_stack + 1, (TITLE_STACK_MAX - 1) * sizeof (title_stack[0]));
              title_depth--;
            }

          title_stack[title_depth++] = e;
        }
        break;
      case 23:			/* pop the title and/or icon name */
        if (title_depth)
          {
            int which = nargs > 1 ? args[1] : 0;
            title_stack_entry &e = title_stack[--title_depth];

            if (e.title && (which == 0 || which == 2))
              set_title (e.title);
            if (e.icon && (which == 0 || which == 1))
              set_icon_name (e.icon);

            free (e.title);
            free (e.icon);
          }
        break;
    }
}
#endif

/*----------------------------------------------------------------------*/
/*
 * Control strings: OSC, DCS, APC, PM and SOS, up to STRING TERMINATOR
 * (ESC \ or 0x9c, or BEL).
 *
 * A string can be much longer than the input buffer (an image, a copied
 * file), so it isn't parsed again from the start each time more input
 * arrives, like other sequences: it is collected in pstr across reads,
 * and cmd_parse continues it before anything else.
 */
void
rxvt_term::begin_string (unicode_t kind)
{
  pstr.kind = kind;
  pstr.len = 0;
  pstr.seen_esc = false;
  pstr.seen_syn = false;
  pstr.overflow = false;

  read_string ();
}

// read the rest of the string, or as much as there is. Throws out_of_input
// when the input runs out: what was read so far stays in pstr.
void
rxvt_term::read_string ()
{
  for (;;)
    {
      unicode_t ch;

      if (pstr.seen_syn)
        {
          ch = cmd_get8 ();
          pstr.seen_syn = false;
        }
      else
        {
          ch = cmd_getc ();

          if (pstr.seen_esc)
            {
              if (ch == 0x5c)	/* 7bit ST */
                end_string (ch);
              else
                pstr.kind = 0; /* ESC and something else: abort */
              return;
            }
          else if (ch == C0_ESC)
            {
              pstr.seen_esc = true;
              continue;
            }
          else if (ch == C0_BEL || ch == CHAR_ST)
            {
              end_string (ch);
              return;
            }
          else if (ch == C0_SYN)
            {
              pstr.seen_syn = true;
              continue;
            }
          else if (ch < 0x20)
            {
              // format effectors (BS, HT, LF, VT, FF, CR) are allowed in
              // device control strings, and ignored
              if (pstr.kind != C1_OSC && IN_RANGE_INC (ch, C0_BS, C0_CR))
                continue;

              pstr.kind = 0; /* other control character - abort */
              return;
            }
        }

      if (pstr.len + 1 >= pstr.size)
        {
          if (pstr.size >= STRING_MAX)
            pstr.overflow = true; // keep reading up to the end, then drop it
          else
            {
              pstr.size = pstr.size ? pstr.size * 2 : 256;
              pstr.buf = (wchar_t *)rxvt_realloc (pstr.buf, pstr.size * sizeof (wchar_t));
            }
        }

      if (!pstr.overflow)
        pstr.buf[pstr.len++] = ch;
    }
}

void
rxvt_term::end_string (unicode_t terminator)
{
  unicode_t kind = pstr.kind;
  pstr.kind = 0;

  string_term st;
  unsigned int n = 0;
  if (terminator == 0x5c)
    st.v[n++] = C0_ESC;
  st.v[n++] = terminator;
  st.v[n] = '\0';

  char *s = 0;
  if (!pstr.overflow && (kind == C1_OSC || kind == C1_DCS))
    {
      pstr.buf[pstr.len] = 0;
      s = rxvt_wcstombs (pstr.buf, pstr.len);
    }

  // don't keep a big buffer around after a big string
  if (pstr.size > 65536)
    {
      free (pstr.buf);
      pstr.buf = 0;
      pstr.size = 0;
    }
  pstr.len = 0;

  // APC, PM and SOS strings, and strings too long, are ignored
  if (!s)
    return;

  if (kind == C1_OSC)
    process_xterm_seq (pstr.osc, s, st);
  else
    process_dcs_string (s);

  free (s);
}

/*----------------------------------------------------------------------*/
/*
 * process DEVICE CONTROL STRING `ESC P ... (ST|BEL)' or `0x90 ... (ST|BEL)'
 *
 * DCS Pp... Pi... F data ST: parameters (digits ; : < = > ?), then
 * intermediates (0x20-0x2f), then the final byte (0x40-0x7e) and the data.
 */
void
rxvt_term::process_dcs_seq ()
{
  unsigned int np = 0, ni = 0;
  unicode_t ch;

  for (;;)
    {
      ch = cmd_getc ();

      if (IN_RANGE_INC (ch, '0', '?'))
        {
          if (np < sizeof (pstr.params) - 1)
            pstr.params[np++] = ch;
        }
      else if (IN_RANGE_INC (ch, 0x20, 0x2f))
        {
          if (ni < sizeof (pstr.inter) - 1)
            pstr.inter[ni++] = ch;
        }
      else
        break;
    }

  pstr.params[np] = 0;
  pstr.inter[ni] = 0;
  pstr.final = ch;

  if (!IN_RANGE_INC (ch, 0x40, 0x7e))
    {
      // an empty string (ESC P ESC \) or garbage: skip to the terminator
      if (ch == C0_ESC)
        cmd_getc ();
      else if (ch != C0_BEL && ch != CHAR_ST)
        begin_string (C1_APC); // read and ignored
      return;
    }

  begin_string (C1_DCS);
}

// a complete device control string: pstr has its parameters, s the data
void
rxvt_term::process_dcs_string (const char *s)
{
  bool noparams = !pstr.params[0];

  if (pstr.final == 'q' && !strcmp (pstr.inter, "$") && noparams)
    decrqss (s);
  else if (pstr.final == 'q' && !strcmp (pstr.inter, "+") && noparams)
    xtgettcap (s);
}

/*
 * The SGR sequence that recreates rendition r: "0;1;38:5:208".
 */
void
rxvt_term::sgr_string (rend_t r, char *buf, size_t len) const
{
  int n = snprintf (buf, len, "0");

#define SGR_ADD(...) n += snprintf (buf + n, n < (int)len ? len - n : 0, __VA_ARGS__)

  if (r & RS_Bold)      SGR_ADD (";1");
  if (r & RS_Faint)     SGR_ADD (";2");
  if (r & RS_Italic)    SGR_ADD (";3");
  if (r & RS_Uline)
    {
      if (GET_ULSTYLE (r) == UL_single)
        SGR_ADD (";4");
      else
        SGR_ADD (";4:%d", GET_ULSTYLE (r) + 1);
    }
  if (r & RS_Blink)     SGR_ADD (";5");
  if (r & RS_RVid)      SGR_ADD (";7");
  if (r & RS_Conceal)   SGR_ADD (";8");
  if (r & RS_Strike)    SGR_ADD (";9");
  if (r & RS_Overline)  SGR_ADD (";53");

  struct { int color, base; } colors[] = {
    { (int)GET_BASEFG (r), 30 },
    { (int)GET_BASEBG (r), 40 },
    { r & RS_ulColor ? GET_ULCOLOR (r) : -1, 50 },
  };

  for (auto &c : colors)
    {
      int idx = c.color;

      if (idx < minCOLOR || idx > maxTermCOLOR24)
        continue; // the default colour (or none)

      if (c.base != 50 && idx <= maxCOLOR)
        SGR_ADD (";%d", c.base + idx - minCOLOR);
#if !ENABLE_MINIMAL
      else if (c.base != 50 && IN_RANGE_INC (idx, minBrightCOLOR, maxBrightCOLOR))
        SGR_ADD (";%d", c.base + 60 + idx - minBrightCOLOR);
#endif
      else if (idx <= maxTermCOLOR)
        SGR_ADD (";%d:5:%d", c.base + 8, idx - minCOLOR);
      else
        {
          uint32_t rgb = rgb24_color[idx - minTermCOLOR24];
          SGR_ADD (";%d:2::%d:%d:%d", c.base + 8, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
        }
    }

#undef SGR_ADD
}

/*
 * DECRQSS, request selection or setting: DCS $ q Pt ST
 * answered with DCS 1 $ r <the sequence that sets it> ST,
 * or DCS 0 $ r ST for settings we don't report.
 */
void
rxvt_term::decrqss (const char *setting)
{
  char reply[256];

  if (!strcmp (setting, "m"))           // SGR
    {
      sgr_string (rstyle, reply, sizeof (reply) - 1);
      strcat (reply, "m");
    }
  else if (!strcmp (setting, "r"))      // DECSTBM, the scroll region
    snprintf (reply, sizeof (reply), "%d;%dr", screen.tscroll + 1, screen.bscroll + 1);
  else if (!strcmp (setting, " q"))     // DECSCUSR, the cursor style
    {
      int blink = 0;
#ifdef CURSOR_BLINK
      blink = option (Opt_cursorBlink);
#endif
      snprintf (reply, sizeof (reply), "%d q", cursor_type * 2 + (blink ? 1 : 2));
    }
  else if (!strcmp (setting, "t"))      // DECSLPP, lines per page
    snprintf (reply, sizeof (reply), "%dt", nrow);
  else if (!strcmp (setting, "\"q"))    // DECSCA: no protected characters
    strcpy (reply, "0\"q");
  else if (!strcmp (setting, "\"p"))    // DECSCL: a VT220-class terminal, 7-bit replies
    strcpy (reply, "62;1\"p");
  else
    {
      tt_printf ("\033P0$r\033\\");
      return;
    }

  tt_printf ("\033P1$r%s\033\\", reply);
}

/*
 * XTGETTCAP: DCS + q <hex name>;<hex name>... ST
 * answered for each name with DCS 1 + r <hex name>=<hex value> ST
 * (just the name for a boolean), or DCS 0 + r <hex name> ST.
 */
static int
hex_digit (char c)
{
  return IN_RANGE_INC (c, '0', '9') ? c - '0'
       : IN_RANGE_INC (c, 'a', 'f') ? c - 'a' + 10
       : IN_RANGE_INC (c, 'A', 'F') ? c - 'A' + 10
       : -1;
}

const char *
rxvt_term::tcap_value (const char *name, bool &boolean)
{
  static char number[16];

  boolean = false;

  if (!strcmp (name, "TN") || !strcmp (name, "name"))
    return rs[Rs_term_name] ? rs[Rs_term_name] : TERMENV;

  if (!strcmp (name, "Co") || !strcmp (name, "colors"))
    {
      snprintf (number, sizeof (number), "%d", maxTermCOLOR - minCOLOR + 1);
      return number;
    }

  static const struct { const char *name, *value; } caps[] = {
    // 24-bit colours (SGR 38:2, 48:2)
    { "RGB",    "8/8/8" },
    { "Tc",     0 },
    // styled and coloured underlines (SGR 4:N, 58)
    { "Smulx",  "\033[4:%p1%dm" },
    { "Setulc", "\033[58:2::%p1%{65536}%/%d:%p1%{256}%/%{255}%&%d:%p1%{255}%&%dm" },
    { "ol",     "\033[59m" },
    // crossed-out text (SGR 9)
    { "smxx",   "\033[9m" },
    { "rmxx",   "\033[29m" },
    // dim, overline
    { "dim",    "\033[2m" },
    { "Smol",   "\033[53m" },
    { "Rmol",   "\033[55m" },
    // cursor styles (DECSCUSR)
    { "Ss",     "\033[%p1%d q" },
    { "Se",     "\033[0 q" },
    // bracketed paste
    { "BE",     "\033[?2004h" },
    { "BD",     "\033[?2004l" },
    { "PS",     "\033[200~" },
    { "PE",     "\033[201~" },
    // focus events
    { "fe",     "\033[?1004h" },
    { "fd",     "\033[?1004l" },
    // synchronized output (mode 2026), as tmux writes it
    { "Sync",   "\033[?2026%?%p1%{1}%-%tl%eh%;" },
    // setting the selection (OSC 52), as tmux writes it
    { "Ms",     "\033]52;%p1%s;%p2%s\007" },
    // XTVERSION, the request for the terminal's name and version
    { "XR",     "\033[>0q" },
  };

  for (auto &cap : caps)
    if (!strcmp (name, cap.name))
      {
        boolean = !cap.value;
        return cap.value ? cap.value : "";
      }

  return 0;
}

void
rxvt_term::xtgettcap (const char *request)
{
  const char *p = request;

  while (*p)
    {
      const char *end = strchr (p, ';');
      if (!end)
        end = p + strlen (p);

      char name[64];
      unsigned int n = 0;
      bool valid = (end - p) % 2 == 0 && end > p && (end - p) / 2 < (int)sizeof (name);

      for (const char *q = p; valid && q < end; q += 2)
        {
          int hi = hex_digit (q[0]), lo = hex_digit (q[1]);
          if (hi < 0 || lo < 0)
            valid = false;
          else
            name[n++] = hi * 16 + lo;
        }
      name[n] = 0;

      bool boolean;
      const char *value = valid ? tcap_value (name, boolean) : 0;

      int len = end - p;
      if (!value)
        tt_printf ("\033P0+r%.*s\033\\", len, p);
      else if (boolean)
        tt_printf ("\033P1+r%.*s\033\\", len, p);
      else
        {
          // DCS 1 + r name = value in hex (every byte becomes two
          // characters, so this can be longer than tt_printf allows)
          size_t vlen = strlen (value);
          size_t size = len + 2 * vlen + 16;
          char *reply = (char *)rxvt_malloc (size);
          int r = snprintf (reply, size, "\033P1+r%.*s=", len, p);
          for (size_t i = 0; i < vlen; i++)
            r += sprintf (reply + r, "%02X", (unsigned char)value[i]);
          r += sprintf (reply + r, "\033\\");
          tt_write (reply, r);
          free (reply);
        }

      p = *end ? end + 1 : end;
    }
}

/*----------------------------------------------------------------------*/
/*
 * OSC 52: ESC ] 52 ; Pc ; Pd ST, set the selections named in Pc to the
 * base64 text Pd, or report them if Pd is "?".
 */
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *
base64_encode (const char *data, unsigned int len)
{
  char *out = (char *)rxvt_malloc ((len + 2) / 3 * 4 + 1);
  char *p = out;

  for (unsigned int i = 0; i < len; i += 3)
    {
      uint32_t v = (unsigned char)data[i] << 16;
      if (i + 1 < len) v |= (unsigned char)data[i + 1] << 8;
      if (i + 2 < len) v |= (unsigned char)data[i + 2];

      *p++ = base64_chars[v >> 18 & 63];
      *p++ = base64_chars[v >> 12 & 63];
      *p++ = i + 1 < len ? base64_chars[v >> 6 & 63] : '=';
      *p++ = i + 2 < len ? base64_chars[v & 63] : '=';
    }

  *p = 0;
  return out;
}

// decodes in place, returns the length, or -1 if it isn't base64
static int
base64_decode (char *str)
{
  uint32_t v = 0;
  int bits = 0, len = 0;
  bool padding = false;

  for (char *p = str; *p; p++)
    {
      if (*p == '=')
        {
          padding = true;
          continue;
        }

      const char *c = strchr (base64_chars, *p);
      if (!c || padding)
        return -1;

      v = v << 6 | (c - base64_chars);
      bits += 6;

      if (bits >= 8)
        {
          bits -= 8;
          str[len++] = v >> bits & 0xff;
        }
    }

  return len;
}

void
rxvt_term::selection_report (char target, const char *data, unsigned int len)
{
  char *encoded = base64_encode (data ? data : "", data ? len : 0);

  tt_printf ("\033]52;%c;", target);
  tt_write (encoded, strlen (encoded));
  tt_write ("\033\\", 2);

  free (encoded);
}

void
rxvt_term::process_selection_seq (char *str, string_term &st)
{
  char *data = strchr (str, ';');
  if (!data)
    return;

  *data++ = 0;

  // the selections: c the clipboard; p, s and cut buffers 0-7 primary
  // (they are what the user selects); none means "s 0", the selection
  bool clipboard = false, primary = false;
  char target = 0;

  for (char *p = *str ? str : (char *)"s"; *p; p++)
    if (*p == 'c')
      {
        clipboard = true;
        target = target ? target : 'c';
      }
    else if (*p == 'p' || *p == 's' || IN_RANGE_INC (*p, '0', '7'))
      {
        primary = true;
        target = target ? target : *p;
      }

  if (!target)
    return;

  if (!strcmp (data, "?"))
    {
      // the selection could hold anything (a password just copied), so
      // programs can read it only with the insecure option
      if (!option (Opt_insecure))
        return;

      // a terminal of this process (wrxvtd runs many) has it?
      rxvt_term *owner = clipboard ? display->clipboard_owner : display->selection_owner;

      if (owner)
        {
          wchar_t *own = clipboard ? owner->selection.clip_text : owner->selection.text;
          unsigned int own_len = clipboard ? owner->selection.clip_len : owner->selection.len;
          char *text = own ? rxvt_wcstombs (own, own_len) : strdup ("");
          selection_report (target, text, strlen (text));
          free (text);
        }
      else if (!selection_req)
        {
          // another program has it: ask it, and answer when it replies
          selection_req = new rxvt_selection (display, clipboard ? Sel_Clipboard : Sel_Primary,
                                              CurrentTime, vt, xa[XA_VT_SELECTION], this);
          selection_req->report = target;
          selection_req->run ();
        }

      return;
    }

  int len = base64_decode (data);
  if (len < 0)
    return;

  data[len] = 0;
  wchar_t *text = rxvt_mbstowcs (data, len);

  if (primary)
    {
      selection_clear (false);
      selection.text = wcsdup (text);
      selection.len = wcslen (text);
      selection_grab (CurrentTime, false);
    }

  if (clipboard)
    {
      selection_clear (true);
      selection.clip_text = wcsdup (text);
      selection.clip_len = wcslen (text);
      selection_grab (CurrentTime, true);
    }

  free (text);
}

/*----------------------------------------------------------------------*/
/*
 * OSC 7: the shell tells its current directory as file://host/path. It is
 * published as the _WRXVT_CWD property of the window, so a window manager
 * binding can open a new terminal in the same place:
 *
 *   wrxvt -cd "$(xprop -id "$(xdotool getactivewindow)" _WRXVT_CWD | cut -d'"' -f2)"
 */
void
rxvt_term::set_cwd (const char *uri)
{
  if (strncmp (uri, "file://", 7))
    return;

  const char *path = strchr (uri + 7, '/'); // skip the host name
  if (!path)
    return;

  // undo the percent-encoding
  char *dir = (char *)rxvt_malloc (strlen (path) + 1);
  char *d = dir;

  for (const char *p = path; *p; p++)
    {
      int hi, lo;

      if (*p == '%'
          && (hi = hex_digit (p[1])) >= 0
          && (lo = hex_digit (p[2])) >= 0)
        {
          *d++ = hi * 16 + lo;
          p += 2;
        }
      else
        *d++ = *p;
    }

  *d = 0;

  set_utf8_property (display->atom ("_WRXVT_CWD"), dir);
  free (dir);
}

/*
 * OSC 8: ESC ] 8 ; params ; URI ST makes the text that follows a link to
 * URI, up to ESC ] 8 ; ; ST. params can have id=ID: separate pieces of
 * text with the same ID and URI are one link.
 */
void
rxvt_term::process_hyperlink (char *str)
{
  char *uri = strchr (str, ';');
  if (!uri)
    return;

  *uri++ = 0;

  rstyle &= ~RS_linkMask;

  if (!*uri)
    return; // the end of the link

  // URIs are printable ASCII (anything else is percent-encoded)
  size_t len = 0;
  for (const char *p = uri; *p; p++, len++)
    if (!IN_RANGE_INC (*p, 0x21, 0x7e) || len > 8192)
      return;

  const char *id = "";
  for (char *param = strtok (str, ":"); param; param = strtok (0, ":"))
    if (!strncmp (param, "id=", 3))
      id = param + 3;

  if (!links)
    links = (hyperlink *)rxvt_calloc (LINK_MAX + 1, sizeof (hyperlink));

  // the same link as a recent one: use its index
  unsigned int idx = 0;

  for (unsigned int i = 0, n = link_next; i < 256 && links[n].uri; i++, n = n > 1 ? n - 1 : LINK_MAX)
    if (!strcmp (links[n].uri, uri) && !strcmp (links[n].id, id))
      {
        idx = n;
        break;
      }

  if (!idx)
    {
      // a new one, replacing the oldest when the table is full
      link_next = link_next % LINK_MAX + 1;
      idx = link_next;

      free (links[idx].id);
      free (links[idx].uri);
      links[idx].id = strdup (id);
      links[idx].uri = strdup (uri);
    }

  rstyle |= (rend_t)idx << RS_linkShift;
}

// the URI of the hyperlink at pixel x, y of the window, or 0
const char *
rxvt_term::link_at (int x, int y)
{
  if (!links)
    return 0;

  int row = Pixel2Row (y);
  int col = Pixel2Col (x);

  if (!IN_RANGE_EXC (row, 0, nrow) || !IN_RANGE_EXC (col, 0, ncol))
    return 0;

  line_t &l = ROW (view_start + row);

  if (!l.valid ())
    return 0;

  int idx = GET_LINK (l.r[col]);

  return idx ? links[idx].uri : 0;
}

/*
 * Run command with sh -c, the arguments as $1 and $2 (never interpreted
 * by the shell), detached from the terminal.
 */
void
rxvt_term::spawn (const char *command, const char *arg1, const char *arg2)
{
  // fork twice, so the command is not our child and needs no reaping
  pid_t pid = fork ();

  if (pid == 0)
    {
      if (fork () == 0)
        {
          setsid ();

          // nothing of the terminal: not the pty, nor the X connection
          int null = open ("/dev/null", O_RDWR);
          dup2 (null, STDIN_FILENO);
          for (int fd = 3; fd < 1024; fd++)
            close (fd);

          if (env)
            environ = env;

          execl ("/bin/sh", "sh", "-c", command, "sh", arg1, arg2, (char *)0);
        }

      _exit (0);
    }

  if (pid > 0)
    waitpid (pid, 0, 0);
}

/*
 * Open the hyperlink at x, y with the url-launcher resource (the same
 * as the matcher extension's), xdg-open by default.
 */
bool
rxvt_term::open_link (int x, int y)
{
  const char *uri = link_at (x, y);
  if (!uri)
    return false;

  const char *launcher = x_resource ("url-launcher");
  if (!launcher || !*launcher)
    launcher = "xdg-open";

  char *command = (char *)rxvt_malloc (strlen (launcher) + 8);
  sprintf (command, "%s \"$1\"", launcher);
  spawn (command, uri);
  free (command);

  return true;
}

/*
 * OSC 9 ; text: a desktop notification (as in iTerm2), shown with the
 * notifyCommand resource, notify-send by default, given the window
 * title and the text.
 */
void
rxvt_term::notify (const char *text)
{
  // ConEmu's progress reports (9;4;...) and other numbered subcommands
  if (isdigit (text[0]) && text[1] == ';')
    return;

  const char *notifier = rs[Rs_notifyCommand];
  if (!notifier)
    notifier = "notify-send --";

  if (!*notifier)
    return; // disabled

  char *title = get_title (false);

  char *command = (char *)rxvt_malloc (strlen (notifier) + 16);
  sprintf (command, "%s \"$1\" \"$2\"", notifier);
  spawn (command, *title ? title : RESNAME, text);
  free (command);
  free (title);

#if ENABLE_FRILLS
  if (!focus && option (Opt_urgentOnBell))
    set_urgency (1);
#endif
}

/*
 * Scroll so that the previous (dir < 0) or next (dir > 0) prompt marked
 * with OSC 133 ; A is the top line; the next one after the last prompt
 * is the bottom of the screen.
 */
bool
rxvt_term::jump_to_prompt (int dir)
{
  if (current_screen != PRIMARY)
    return false;

  if (dir < 0)
    {
      for (int row = view_start - 1; row >= top_row; row--)
        if (ROW (row).f & LINE_PROMPT)
          return scr_changeview (row);

      return false;
    }

  for (int row = view_start + 1; row < 0; row++)
    if (ROW (row).f & LINE_PROMPT)
      return scr_changeview (row);

  return scr_changeview (0);
}

/*
 * A font list with every size changed by delta: pixelsize=N and size=N
 * of xft: fonts, and the pixel size field of X core font names.
 */
static char *
scale_font_list (const char *list, int delta)
{
  // every number grows by at most a few digits
  char *out = (char *)rxvt_malloc (strlen (list) * 2 + 64);
  char *o = out;
  const char *p = list;

  while (*p)
    {
      const char *end = strchr (p, ',');
      if (!end)
        end = p + strlen (p);

      const char *num = 0;

      if (!strncmp (p, "xft:", 4))
        {
          // the last pixelsize= or size= of this font
          for (const char *q = p; q < end; q++)
            if (*q == ':' && !strncmp (q + 1, "pixelsize=", 10))
              num = q + 11;
            else if (*q == ':' && !strncmp (q + 1, "size=", 5))
              num = q + 6;
        }
      else if (*p == '-')
        {
          // -foundry-family-weight-slant-setwidth-adstyle-PIXELSIZE-...
          int dashes = 0;
          for (const char *q = p; q < end; q++)
            if (*q == '-' && ++dashes == 7)
              {
                num = q + 1;
                break;
              }
        }

      if (num && isdigit (*num))
        {
          char *e;
          long size = strtol (num, &e, 10);

          memcpy (o, p, num - p);
          o += num - p;
          o += sprintf (o, "%ld", max (1L, size + delta));
          p = e;
        }

      memcpy (o, p, end - p);
      o += end - p;
      p = end;

      if (*p == ',')
        *o++ = *p++;
    }

  *o = 0;
  return out;
}

/*
 * The font-size actions: make all fonts one step (one pixel or point)
 * bigger or smaller, or as configured again (delta 0).
 */
void
rxvt_term::change_font_size (int delta)
{
#if ENABLE_STYLES
  const int fonts = 4; // normal, bold, italic, bold italic
#else
  const int fonts = 1;
#endif

  if (!font_size_set)
    {
      // remember the fonts as configured
      for (int i = 0; i < fonts; i++)
        font_orig[i] = rs[Rs_font + i] ? strdup (rs[Rs_font + i]) : 0;

      font_size_set = true;
    }

  font_delta = delta ? font_delta + delta : 0;

  for (int i = 0; i < fonts; i++)
    if (font_orig[i])
      {
        char *f = scale_font_list (font_orig[i], font_delta);
        allocated.push_back ((void *)f);
        rs[Rs_font + i] = f;
      }

  set_fonts ();
}

/*
 * Actions for keysym bindings (URxvt.keysym.C-S-Up: prompt:previous)
 * that wrxvt does without perl.
 */
bool
rxvt_term::builtin_action (const char *name, const char *arg)
{
  if (!strcmp (name, "font-size"))
    {
      if (!strcmp (arg, "increase"))
        change_font_size (1);
      else if (!strcmp (arg, "decrease"))
        change_font_size (-1);
      else if (!strcmp (arg, "reset"))
        change_font_size (0);
      else
        return false;

      return true;
    }

  if (!strcmp (name, "prompt"))
    {
      if (!strcmp (arg, "previous"))
        jump_to_prompt (-1);
      else if (!strcmp (arg, "next"))
        jump_to_prompt (1);
      else
        return false;

      refresh_check ();
      return true;
    }

  return false;
}

/*----------------------------------------------------------------------*/
/*
 * process OPERATING SYSTEM COMMAND sequence `ESC ] Ps ; Pt (ST|BEL)'
 */
void
rxvt_term::process_osc_seq ()
{
  int arg;

  unicode_t ch = cmd_getc ();
  for (arg = 0; isdigit (ch); ch = cmd_getc ())
    arg = arg * 10 + (ch - '0');

  if (ch == ';')
    {
      pstr.osc = arg;
      begin_string (C1_OSC);
    }
  else if (ch == C0_BEL || ch == CHAR_ST || (ch == C0_ESC && cmd_getc () == '\\'))
    {
      // no text at all: ESC ] 104 ST resets all colours, for example
      string_term st;
      st.v[0] = ch == C0_ESC ? C0_ESC : ch;
      st.v[1] = ch == C0_ESC ? '\\' : 0;
      st.v[2] = 0;

      char empty[1] = "";
      process_xterm_seq (arg, empty, st);
    }
}

static unsigned int
colorcube_index (unsigned int idx_r,
                 unsigned int idx_g,
                 unsigned int idx_b)
{
  assert (idx_r < Red_levels);
  assert (idx_g < Green_levels);
  assert (idx_b < Blue_levels);

  return idx_r * Blue_levels * Green_levels +
         idx_g * Blue_levels +
         idx_b;
}

/*
 * Find the nearest color slot in the hidden color cube,
 * adapt its value to the 32bit RGBA color.
 */
unsigned int
rxvt_term::map_rgb24_color (unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
  r &= 0xff;
  g &= 0xff;
  b &= 0xff;
  a &= 0xff;

  uint32_t color = (a << 24) | (r << 16) | (g << 8) | b;

  /* we allow one of the 6 closest neighbouring colours */
  /* to replace the current color, if they not used recently */
  #if 0
  static const unsigned char dxyz[][3] = {
    0, 0, 0,
    0, 0, 4,
    0, 4, 4,
    4, 4, 0,
    4, 0, 4,
    0, 4, 0,
    4, 0, 0,
  };
  #else
  // silly compressed verlapping triplets version of above
  static const unsigned char dxyz[] = {
    0, 0, 0, 4, 4, 0, 4, 0, 0,
  };
  #endif

  static const unsigned char color_level[8][32] = {
    // neighbour index
    {0, 0, 1, 0, 0, 1, 1, 2, 2, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    {0, 1, 0, 1, 1, 2, 1, 1, 2, 2, 3, 3, 2, 2, 2, 3, 3, 3, 4, 4, 4, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4},
    {0, 0, 1, 2, 1, 1, 2, 3, 3, 2, 2, 3, 3, 4, 4, 3, 3, 3, 4, 4, 5, 5, 5, 4, 4, 4, 5, 5, 5, 5, 5, 5},
    {0, 0, 2, 1, 2, 3, 2, 2, 3, 4, 4, 3, 3, 4, 4, 5, 5, 4, 4, 5, 5, 5, 6, 6, 5, 5, 5, 6, 6, 6, 6, 6},
    // Red_levels/Green_levels/Blue_levels index
    {0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6},
  };

  unsigned int idx;

  for (int n = 0; n < ecb_array_length (dxyz) - 2; ++n)
    {
      unsigned int idx_r = color_level[  Red_levels - dxyz[n + 0]][r / 8];
      unsigned int idx_g = color_level[Green_levels - dxyz[n + 1]][g / 8];
      unsigned int idx_b = color_level[ Blue_levels - dxyz[n + 2]][b / 8];
      unsigned int index = colorcube_index (idx_r, idx_g, idx_b);

      if (n == 0)
        idx = index;

      if (rgb24_color[index] == color)
        {
          rgb24_seqno[index] = ++rgb24_sequence;
          return index + minTermCOLOR24;
        }

      // minor issue: could update index 0 few more times
      if ((rgb24_seqno[index] | rgb24_color[index]) == 0)
        {
          idx = index;
          goto update;
        }

      // like (rgb24_seqno[idx] > rgb24_seqno[index])
      // but also handles wrap around values well enough
      if ((uint16_t)(rgb24_seqno[idx] - rgb24_seqno[index]) < 0x8000)
        idx = index;
    }

update:
  rgb24_color[idx] = color;
  rgb24_seqno[idx] = ++rgb24_sequence;

  idx += minTermCOLOR24;
  pix_colors_focused [idx].free (this);
  pix_colors_focused [idx].set (this, rgba (r * 0x0101, g * 0x0101, b * 0x0101, a * 0x0101));
  update_fade_color (idx, false);

  return idx;
}

void
rxvt_term::process_color_seq (int report, int color, const char *str, string_term &st)
{
  if (str[0] == '?' && !str[1])
    {
      rgba c;
      pix_colors_focused[color].get (c);
      char rgba_str[32];

#if XFT
      if (c.a != rgba::MAX_CC)
        snprintf (rgba_str, sizeof (rgba_str), "rgba:%04x/%04x/%04x/%04x", c.r, c.g, c.b, c.a);
      else
#endif
        snprintf (rgba_str, sizeof (rgba_str), "rgb:%04x/%04x/%04x", c.r, c.g, c.b);

      if (IN_RANGE_INC (color, minCOLOR, maxTermCOLOR))
        tt_printf ("\033]%d;%d;%s%s", report, color - minCOLOR, rgba_str, st.v);
      else
        tt_printf ("\033]%d;%s%s", report, rgba_str, st.v);
    }
  else
    set_window_color (color, str);
}

#ifdef XTERM_COLOR_CHANGE
// a colour back to its configured value
void
rxvt_term::reset_color (int idx)
{
  if (initial_colors[idx] && (!rs[Rs_color + idx] || strcmp (rs[Rs_color + idx], initial_colors[idx])))
    set_window_color (idx, initial_colors[idx]);
}
#endif

/*
 * XTerm escape sequences: ESC ] Ps;Pt (ST|BEL)
 */
void
rxvt_term::process_xterm_seq (int op, char *str, string_term &st)
{
  int color;
  char *buf, *name;
  bool query = str[0] == '?' && !str[1];
  int saveop = op;

  dLocal (Display *, dpy);

  assert (str != NULL);

  if (HOOK_INVOKE ((this, HOOK_OSC_SEQ, DT_INT, op, DT_STR, str, DT_END)))
    return;

  switch (op)
    {
      case XTerm_name:
        set_title (str);
        /* FALLTHROUGH */
      case XTerm_iconName:
        set_icon_name (str);
        break;
      case XTerm_title:
        set_title (str);
        break;
      case XTerm_property:
        if (str[0] == '?')
          {
            Atom prop = display->atom (str + 1);
            Atom actual_type;
            int actual_format;
            unsigned long nitems;
            unsigned long bytes_after;
            unsigned char *value = 0;
            const char *str = "";

            if (prop
                && XGetWindowProperty (dpy, parent,
                                       prop, 0, 1<<16, 0, AnyPropertyType,
                                       &actual_type, &actual_format,
                                       &nitems, &bytes_after, &value) == Success
                && actual_type != None
                && actual_format == 8)
              str = (const char *)(value);

            tt_printf ("\033]%d;%s%s", op, option (Opt_insecure) ? str : "", st.v);

            XFree (value);
          }
        else
          {
            char *eq = strchr (str, '=');

            if (eq)
              {
                *eq = 0;
                set_utf8_property (display->atom (str), eq + 1);
              }
            else
              XDeleteProperty (dpy, parent,
                               display->atom (str));
          }
        break;

      case XTerm_Color:
        for (buf = (char *)str; buf && *buf;)
          {
            if ((name = strchr (buf, ';')) == NULL)
              break;

            *name++ = '\0';
            color = atoi (buf) + minCOLOR;

            if (!IN_RANGE_INC (color, minCOLOR, maxTermCOLOR))
              break;

            if ((buf = strchr (name, ';')) != NULL)
              *buf++ = '\0';

            process_color_seq (op, color, name, st);
          }
        break;
      case Rxvt_restoreFG:
      case XTerm_Color00:
        process_color_seq (op, Color_fg, str, st);
        break;
      case Rxvt_restoreBG:
      case XTerm_Color01:
        process_color_seq (op, Color_bg, str, st);
        break;
#ifndef NO_CURSORCOLOR
      case XTerm_Color_cursor:
        process_color_seq (op, Color_cursor, str, st);
        break;
#endif
      case XTerm_Color_pointer_fg:
        process_color_seq (op, Color_pointer_fg, str, st);
        break;
      case XTerm_Color_pointer_bg:
        process_color_seq (op, Color_pointer_bg, str, st);
        break;
#ifdef OPTION_HC
      case XTerm_Color_HC:
        process_color_seq (op, Color_HC, str, st);
        break;
      case XTerm_Color_HTC:
        process_color_seq (op, Color_HTC, str, st);
        break;
#endif
#ifndef NO_BOLD_UNDERLINE_REVERSE
      case URxvt_Color_BD:
        process_color_seq (op, Color_BD, str, st);
        break;
      case URxvt_Color_UL:
        process_color_seq (op, Color_UL, str, st);
        break;
      case URxvt_Color_IT:
        process_color_seq (op, Color_IT, str, st);
        break;
#endif
      case URxvt_Color_border:
        process_color_seq (op, Color_border, str, st);
        break;

      case XTerm_logfile:
        // TODO, when secure mode?
        break;

      case XTerm_selection:
        process_selection_seq (str, st);
        break;

      case XTerm_cwd:
        set_cwd (str);
        break;

      case XTerm_hyperlink:
        process_hyperlink (str);
        break;

      case XTerm_notify:
        notify (str);
        break;

#ifdef XTERM_COLOR_CHANGE
      case XTerm_resetColor:
        if (!*str)
          for (color = minCOLOR; color <= maxTermCOLOR; color++)
            reset_color (color);
        else
          for (buf = str; *buf; )
            {
              color = atoi (buf) + minCOLOR;

              if (IN_RANGE_INC (color, minCOLOR, maxTermCOLOR))
                reset_color (color);

              buf = strchr (buf, ';');
              if (!buf)
                break;
              buf++;
            }
        break;

      case XTerm_resetColor00 ... XTerm_resetColor_HTC:
        {
          // the colours of OSC 10 to 19, one less of them each
          static const int colors[] = {
            Color_fg, Color_bg,
# ifndef NO_CURSORCOLOR
            Color_cursor,
# else
            -1,
# endif
            Color_pointer_fg, Color_pointer_bg, -1, -1,
# ifdef OPTION_HC
            Color_HC, -1, Color_HTC,
# else
            -1, -1, -1,
# endif
          };

          if (colors[op - XTerm_resetColor00] >= 0)
            reset_color (colors[op - XTerm_resetColor00]);
        }
        break;
#endif

      case Shell_mark:
        // A: a prompt starts here. B (command typed), C (its output
        // starts) and D (it ended) need nothing yet.
        if (str[0] == 'A' && (!str[1] || str[1] == ';') && current_screen == PRIMARY)
          ROW (screen.cur.row).f |= LINE_PROMPT;
        break;

#if 0
      case Rxvt_dumpscreen:	/* no error notices */
        {
          int fd;
          if ((fd = open (str, O_RDWR | O_CREAT | O_EXCL, 0600)) >= 0)
            {
              scr_dump (fd);
              close (fd);
            }
        }
        break;
#endif
      case XTerm_font:
        op = URxvt_font;
      case URxvt_font:
#if ENABLE_STYLES
      case URxvt_boldFont:
      case URxvt_italicFont:
      case URxvt_boldItalicFont:
#endif
        if (query)
          tt_printf ("\33]%d;%-.250s%s", saveop,
                     option (Opt_insecure) && fontset[op - URxvt_font]->fontdesc
                       ? fontset[op - URxvt_font]->fontdesc : "",
                     st.v);
        else
          {
            const char *&res = rs[Rs_font + (op - URxvt_font)];

            res = strdup (str);
            allocated.push_back ((void *)res);
            set_fonts ();
          }
        break;

      case URxvt_version:
        if (query)
          tt_printf ("\33]%d;rxvt-unicode;%-.20s;%c;%c%s",
                     op,
                     rs[Rs_name], VERSION[0], VERSION[2],
                     st.v);
        break;

#if !ENABLE_MINIMAL
      case URxvt_cellinfo:
        if (query)
          tt_printf ("\33]%d;%d;%d;%d%s", saveop,
                     fwidth, fheight, fbase,
                     st.v);
        break;

      case URxvt_locale:
        if (query)
          tt_printf ("\33]%d;%-.250s%s", op, option (Opt_insecure) ? locale : "", st.v);
        else
          {
            set_locale (str);
            pty->set_utf8_mode (enc_utf8);
            init_xlocale ();
          }
        break;

      case URxvt_view_up:
      case URxvt_view_down:
        {
          int lines = atoi (str);

          if (lines)
            scr_page (op == URxvt_view_up ? UP : DN, lines);
          else
            scr_erase_savelines ();
        }

        break;
#endif

#if ENABLE_PERL
      case URxvt_perl:
        HOOK_INVOKE ((this, HOOK_OSC_SEQ_PERL, DT_STR, str, DT_STR, st.v, DT_END));
        break;
#endif
    }
}
/*----------------------------------------------------------------------*/

/*{{{ process DEC private mode sequences `ESC [ ? Ps mode' */
/*
 * mode can only have the following values:
 *      'l' = low
 *      'h' = high
 *      's' = save
 *      'r' = restore
 *      't' = toggle
 * so no need for fancy checking
 */
int ecb_cold
rxvt_term::privcases (int mode, unsigned long bit)
{
  int state;

  if (mode == 's')
    {
      if (priv_modes & bit)
        SavedModes |= bit;
      else
        SavedModes &= ~bit;
      return -1;
    }
  else
    {
      if (mode == 'r')
        state = (SavedModes & bit) ? 1 : 0;	/* no overlapping */
      else
        state = (mode == 't') ? ! (priv_modes & bit) : mode;

      if (state)
        priv_modes |= bit;
      else
        priv_modes &= ~bit;
    }

  return state;
}

/* we're not using priv _yet_ */
void
rxvt_term::process_terminal_mode (int mode, int priv ecb_unused, unsigned int nargs, const int *arg)
{
  unsigned int i, j;
  int state;

  static const struct
  {
    const int       argval;
    const unsigned long bit;
  } argtopriv[] = {
                  { 1, PrivMode_aplCUR },       // DECCKM
                  { 2, PrivMode_vt52 },         // DECANM
                  { 3, PrivMode_132 },          // DECCOLM
                  { 4, PrivMode_smoothScroll }, // DECSCLM
                  { 5, PrivMode_rVideo },       // DECSCNM
                  { 6, PrivMode_relOrigin },    // DECOM
                  { 7, PrivMode_Autowrap },     // DECAWM
                 // 8, auto-repeat keys         // DECARM
                  { 9, PrivMode_MouseX10 },
                  { 12, PrivMode_BlinkingCursor },
                 // 18 end FF to printer after print screen
                 // 19 Print screen prints full screen/scroll region
                  { 25, PrivMode_VisibleCursor }, // DECTCEM cnorm/cvvis/civis
#ifdef scrollBar_esc
                  { scrollBar_esc, PrivMode_scrollBar },
#endif
                  { 35, PrivMode_ShiftKeys },   // rxvt extension
                 // 38, tektronix mode          // DECTEK
                  { 40, PrivMode_132OK },
                 // 41 xterm more fixes NYI
                 // 45 margin bell NYI
                 // 46 start logging
                  { 47, PrivMode_Screen },
                  { 66, PrivMode_aplKP },       // DECNKM
#ifndef NO_BACKSPACE_KEY
                  { 67, PrivMode_BackSpace },   // DECBKM
#endif
                  { 1000, PrivMode_MouseX11 },
                  { 1002, PrivMode_MouseBtnEvent },
                  { 1003, PrivMode_MouseAnyEvent },
#if ENABLE_FRILLS
                  { 1004, PrivMode_FocusEvent },
                  { 1005, PrivMode_ExtMouseUTF8 },
                  { 1006, PrivMode_ExtMouseSGR },
                  { 1016, PrivMode_ExtMousePixels },
#endif
                  { 1010, PrivMode_TtyOutputInh }, // rxvt extension
                  { 1011, PrivMode_Keypress }, // rxvt extension
#if ENABLE_FRILLS
                  { 1015, PrivMode_ExtMouseUrxvt }, // urxvt extension of 1005
#endif
                 // 1035 enable modifiers for alt, numlock NYI
                 // 1036 send ESC for meta keys NYI
                 // 1037 send DEL for keypad delete NYI
                  { 1047, PrivMode_Screen },
                 // 1048 save and restore cursor, implemented in code
                  { 1049, PrivMode_Screen }, /* xterm extension, clear screen on ti rather than te */
                 // 1051, 1052, 1060, 1061 keyboard emulation NYI
#if ENABLE_FRILLS
                  { 2004, PrivMode_BracketPaste },
#endif
                  { 2026, PrivMode_SyncOutput },
                  { 2031, PrivMode_ColorScheme },
                };

  if (nargs == 0)
    return;

  // DECRQM
  if (mode == 'p')
    {
      int status = 0;
      if (nargs != 1)
        return;

      for (j = 0; j < ecb_array_length (argtopriv); j++)
        if (argtopriv[j].argval == arg[0])
          {
            status = (priv_modes & argtopriv[j].bit) ? 1 : 2;
            break;
          }

      tt_printf ("\33[?%d;%d$y", arg[0], status);
      return;
    }

  /* make lo/hi boolean */
  if (mode == 'l')
    mode = 0;		/* reset */
  else if (mode == 'h')
    mode = 1;		/* set */

  for (i = 0; i < nargs; i++)
    {
      state = -1;

      /* basic handling */
      for (j = 0; j < ecb_array_length (argtopriv); j++)
        if (argtopriv[j].argval == arg[i])
          {
            state = privcases (mode, argtopriv[j].bit);
            break;
          }

      /* extra handling for values with state unkept  */
      switch (arg[i])
        {
#if ENABLE_STYLES
          case 1021:
            set_option (Opt_intensityStyles, mode);

            scr_touch (true);
            break;
#endif
          case 1048:		/* alternative cursor save */
            if (option (Opt_secondaryScreen))
              if (mode == 0)
                scr_cursor (RESTORE);
              else if (mode == 1)
                scr_cursor (SAVE);
            break;
        }

      if (state >= 0)
        /* extra handling for values with valid 0 or 1 state */
        switch (arg[i])
          {
              /* case 1:	- application cursor keys */
            case 2:			/* VT52 mode */
              /* oddball mode.  should be set regardless of set/reset
               * parameter.  Return from VT52 mode with an ESC < from
               * within VT52 mode
               */
              priv_modes |= PrivMode_vt52;
              break;
            case 3:			/* 80/132 */
              if (priv_modes & PrivMode_132OK)
                set_widthheight ((state ? 132 : 80) * fwidth, 24 * fheight);
              break;
            case 4:			/* smooth scrolling */
              set_option (Opt_jumpScroll, !state);
              break;
            case 5:			/* reverse video */
              scr_rvideo_mode (state);
              break;
            case 6:			/* relative/absolute origins  */
              scr_relative_origin (state);
              break;
            case 7:			/* autowrap */
              scr_autowrap (state);
              break;
            /* case 8:	- auto repeat, can't do on a per window basis */
            case 9:			/* X10 mouse reporting */
              if (state)		/* orthogonal */
                priv_modes &= ~(PrivMode_MouseX11|PrivMode_MouseBtnEvent|PrivMode_MouseAnyEvent);
              break;
#ifdef scrollBar_esc
            case scrollBar_esc:
              scrollBar.map (state);
              resize_all_windows (0, 0, 0);
              scr_touch (true);
              break;
#endif
#ifdef CURSOR_BLINK
            case 12:
              cursor_blink_reset ();
              break;
#endif
            case 25:		/* visible/invisible cursor */
              scr_cursor_visible (state);
              break;
            /* case 35:	- shift keys */
            /* case 40:	- 80 <--> 132 mode */
            case 47:		/* secondary screen */
              scr_change_screen (state);
              break;
            /* case 66:	- application key pad */
            /* case 67:	- backspace key */
            case 1000:		/* X11 mouse reporting */
              if (state)		/* orthogonal */
                priv_modes &= ~(PrivMode_MouseX10|PrivMode_MouseBtnEvent|PrivMode_MouseAnyEvent);
              break;
            case 1002:
            case 1003:
              if (state)
                {
                  priv_modes &= ~(PrivMode_MouseX10|PrivMode_MouseX11);
                  priv_modes &= arg[i] == 1003 ? ~PrivMode_MouseBtnEvent : ~PrivMode_MouseAnyEvent;
                  mouse_row = mouse_col = 0;
                  vt_emask_mouse = PointerMotionMask;
                }
              else
                vt_emask_mouse = NoEventMask;

              vt_select_input ();
              break;
            case 1010:		/* scroll to bottom on TTY output inhibit */
              set_option (Opt_scrollTtyOutput, !state);
              break;
            case 1011:		/* scroll to bottom on key press */
              set_option (Opt_scrollTtyKeypress, state);
              break;
            case 1047:		/* secondary screen w/ clearing last */
              if (option (Opt_secondaryScreen))
                if (!state)
                  scr_erase_screen (2);

              scr_change_screen (state);
              break;
            case 1049:		/* secondary screen w/ clearing first */
              if (option (Opt_secondaryScreen))
                if (state)
                  scr_cursor (SAVE);

              scr_change_screen (state);

              if (option (Opt_secondaryScreen))
                if (state)
                  scr_erase_screen (2);
                else
                  scr_cursor (RESTORE);
              break;
            case 2026:		/* synchronized output */
              sync_output (state);
              break;
            case 2031:		/* colour scheme updates */
              reported_color_scheme = color_scheme ();
              break;
            default:
              break;
          }
    }
}
/*}}} */

/*{{{ process sgr sequences */
void ecb_hot
rxvt_term::process_sgr_mode (unsigned int nargs, const int *arg, uint32_t sub)
{
  unsigned int i;
  short rendset;
  rend_t rendstyle;

  if (nargs == 0)
    {
      scr_rendition (0, ~RS_None);
      return;
    }

  // is arg[i] a subparameter (it followed a ':')?
  #define SGR_SUB(i) ((i) < nargs && (sub >> (i) & 1))

  for (i = 0; i < nargs; i++)
    {
      // subparameters the code below doesn't use belong to an argument we
      // already handled: never read them as arguments of their own
      if (SGR_SUB (i))
        continue;

      rendset = -1;
      switch (arg[i])
        {
          case 0:
            rendset = 0, rendstyle = ~RS_None;
            break;
          case 1:
            rendset = 1, rendstyle = RS_Bold;
            break;
          case 2: // faint
            rendset = 1, rendstyle = RS_Faint;
            break;
          case 3:
            rendset = 1, rendstyle = RS_Italic;
            break;
          case 4:
            // 4:0 is "no underline", 4:1 to 4:5 single, double, curly,
            // dotted and dashed
            if (SGR_SUB (i + 1))
              scr_underline (arg[i + 1] <= 0 ? -1 : arg[i + 1] - 1);
            else
              scr_underline (UL_single);
            break;
          case 5: // slowly blinking
          case 6: // rapidly blinking
            rendset = 1, rendstyle = RS_Blink;
            break;
          //case 6: // scoansi light background
          case 7:
            rendset = 1, rendstyle = RS_RVid;
            break;
          case 8: // invisible
            rendset = 1, rendstyle = RS_Conceal;
            break;
          case 9: // crossed out
            rendset = 1, rendstyle = RS_Strike;
            break;
          //case 10: // scoansi acs off, primary font
          //case 11: // scoansi acs on, first alt font
          //case 12: // scoansi acs on, |0x80, second alt font
          //...
          //case 19: // ninth alt font
          //case 20: // gothic
          case 21: // doubly underlined (ECMA-48, as in xterm, VTE, kitty)
            scr_underline (UL_double);
            break;
          case 22: // normal intensity: neither bold nor faint
            rendset = 0, rendstyle = RS_Bold | RS_Faint;
            break;
          case 23: // disable italic
            rendset = 0, rendstyle = RS_Italic;
            break;
          case 24: // underline off (vt220)
            rendset = 0, rendstyle = RS_Uline;
            break;
          case 25: // blink off (vt220)
            rendset = 0, rendstyle = RS_Blink;
            break;
          case 26: // variable spacing (iso 8613)
            rendset = 0, rendstyle = RS_Blink;
            break;
          case 27: // reverse off (vt220)
            rendset = 0, rendstyle = RS_RVid;
            break;
          case 28: // visible
            rendset = 0, rendstyle = RS_Conceal;
            break;
          case 29: // not crossed out
            rendset = 0, rendstyle = RS_Strike;
            break;
          case 53: // overlined
            rendset = 1, rendstyle = RS_Overline;
            break;
          case 55: // not overlined
            rendset = 0, rendstyle = RS_Overline;
            break;
        }

      if (rendset != -1)
        {
          scr_rendition (rendset, rendstyle);
          continue;		/* for (;i;) */
        }

      switch (arg[i])
        {
          case 30:
          case 31:		/* set fg color */
          case 32:
          case 33:
          case 34:
          case 35:
          case 36:
          case 37:
            scr_color ((unsigned int) (minCOLOR + (arg[i] - 30)), Color_fg);
            break;
          case 39:		/* default fg */
            scr_color (Color_fg, Color_fg);
            break;

          case 40:
          case 41:		/* set bg color */
          case 42:
          case 43:
          case 44:
          case 45:
          case 46:
          case 47:
            scr_color ((unsigned int) (minCOLOR + (arg[i] - 40)), Color_bg);
            break;
          case 49:		/* default bg */
            scr_color (Color_bg, Color_bg);
            break;

          case 38: // set fg color, ISO 8613-6
          case 48: // set bg color, ISO 8613-6
          case 58: // set underline color (kitty, VTE, xterm...)
            {
              unsigned int which = arg[i];
              int idx = -1;

              if (SGR_SUB (i + 1))
                {
                  // the ISO 8613-6 form with colons: 38:5:N, 38:2:R:G:B and
                  // 38:2:CS:R:G:B (CS, the colour space id, is usually empty)
                  unsigned int count = 0;
                  while (SGR_SUB (i + 1 + count))
                    count++;

                  if (arg[i + 1] == 5 && count >= 2)
                    {
                      if (IN_RANGE_INC (arg[i + 2], 0, 255))
                        idx = minCOLOR + arg[i + 2];
                    }
                  else if (arg[i + 1] == 2 && count >= 4)
                    {
                      unsigned int first = count >= 5 ? i + 3 : i + 2;
                      idx = map_rgb24_color (arg[first], arg[first + 1], arg[first + 2], 0xff);
                    }

                  i += count;
                }
              else if (nargs > i + 2 && arg[i + 1] == 5)
                {
                  // only the 256 palette colours: higher numbers would pick
                  // the slots used internally for 24-bit colours
                  if (IN_RANGE_INC (arg[i + 2], 0, 255))
                    idx = minCOLOR + arg[i + 2];
                  i += 2;
                }
              else if (nargs > i + 4 && arg[i + 1] == 2)
                {
                  idx = map_rgb24_color (arg[i + 2], arg[i + 3], arg[i + 4], 0xff);
                  i += 4;
                }

              if (idx < 0)
                break;

              if (which == 58)
                scr_underline_color (idx);
              else
                scr_color (idx, which == 38 ? Color_fg : Color_bg);
            }
            break;
          case 59: // default underline color
            scr_underline_color (-1);
            break;

          //case 50: // not variable spacing

#if !ENABLE_MINIMAL
          case 90:
          case 91:		/* set bright fg color */
          case 92:
          case 93:
          case 94:
          case 95:
          case 96:
          case 97:
            scr_color ((unsigned int) (minBrightCOLOR + (arg[i] - 90)), Color_fg);
            break;
          case 100:
          case 101:		/* set bright bg color */
          case 102:
          case 103:
          case 104:
          case 105:
          case 106:
          case 107:
            scr_color ((unsigned int) (minBrightCOLOR + (arg[i] - 100)), Color_bg);
            break;
#endif
        }
    }
}

void
rxvt_term::set_cursor_style (int style)
{
  if (!IN_RANGE_INC (style, 0, 6))
    return;

  // 0 is the style the user configured (cursorUnderline, cursorBlink),
  // so programs that reset the cursor when they exit leave it as it was
  if (style == 0)
    style = default_cursor_style;

  cursor_type = (style - 1) / 2;
  set_option (Opt_cursorUnderline, cursor_type == 1);

#ifdef CURSOR_BLINK
  set_option (Opt_cursorBlink, style & 1);
  cursor_blink_reset ();
#endif

  want_refresh = 1;
}
/*}}} */

/* ---------------------------------------------------------------------- */
/* Write data to the pty as typed by the user, pasted with the mouse,
 * or generated by us in response to a query ESC sequence.
 */

/*
 * Send printf () formatted output to the command.
 * Only use for small amounts of data.
 */
void
rxvt_term::tt_printf (const char *fmt,...)
{
  va_list arg_ptr;
  char buf[256];

  va_start (arg_ptr, fmt);
  vsnprintf ((char *)buf, 256, fmt, arg_ptr);
  va_end (arg_ptr);
  tt_write (buf, strlen (buf));
}

/* Write data to the pty as typed by the user. */
void
rxvt_term::tt_write_user_input (const char *data, unsigned int len)
{
  if (HOOK_INVOKE ((this, HOOK_TT_WRITE, DT_STR_LEN, data, len, DT_END)))
    return;

  if (option (Opt_scrollTtyKeypress))
    scr_changeview (0);

  tt_write_ (data, len);
}

void
rxvt_term::tt_write (const char *data, unsigned int len)
{
  if (HOOK_INVOKE ((this, HOOK_TT_WRITE, DT_STR_LEN, data, len, DT_END)))
    return;

  tt_write_ (data, len);
}

static const unsigned int MAX_PTY_WRITE = 255; // minimum MAX_INPUT

void
rxvt_term::tt_write_ (const char *data, unsigned int len)
{
  if (pty->pty < 0)
    return;

  if (v_buflen == 0)
    {
      ssize_t written = write (pty->pty, data, min (len, MAX_PTY_WRITE));

      max_it (written, 0);

      if (written == len)
        return;

      data += written;
      len  -= written;
    }

  v_buffer = (char *)rxvt_realloc (v_buffer, v_buflen + len);

  memcpy (v_buffer + v_buflen, data, len);
  v_buflen += len;

  pty_ev.set (ev::READ | ev::WRITE);
}

void rxvt_term::pty_write ()
{
  int written = write (pty->pty, v_buffer, min (v_buflen, MAX_PTY_WRITE));

  if (written > 0)
    {
      v_buflen -= written;

      if (v_buflen == 0)
        {
          free (v_buffer);
          v_buffer = 0;

          pty_ev.set (ev::READ);
          return;
        }

      memmove (v_buffer, v_buffer + written, v_buflen);
    }
  else if (written != -1 || (errno != EAGAIN && errno != EINTR))
    pty_ev.set (ev::READ);
}

/*----------------------- end-of-file (C source) -----------------------*/

