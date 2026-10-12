/*----------------------------------------------------------------------*
 * File:	dnd.C
 *----------------------------------------------------------------------*
 *
 * Drag and drop (XDND, version 5): files dropped on the window are typed
 * as their paths, quoted for the shell; dropped text is pasted. File
 * managers (Thunar, Nautilus, Dolphin, pcmanfm), browsers and dragon
 * send them.
 *
 * Both arrive as a paste (bracketed paste and the paste hooks apply), so
 * the shell sees one paste, not keys.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#if ENABLE_DND

#include <string.h>

#include "rxvtbuf.h"

#define XDND_VERSION 5

// the window can take drops
void rxvt_term::dnd_init() {
  Atom version = XDND_VERSION;

  XChangeProperty(dpy, parent, display->atom("XdndAware"), XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)&version, 1);
}

// the most useful of the types the source offers, or None
Atom rxvt_term::dnd_choose_type(const Atom *types, unsigned long count) {
  static const char *const preferred[] = {
      "text/uri-list", "UTF8_STRING", "text/plain;charset=utf-8",
      "text/plain",    "STRING",
  };

  for (unsigned int p = 0; p < ecb_array_length(preferred); p++) {
    Atom want = display->atom(preferred[p]);

    for (unsigned long i = 0; i < count; i++)
      if (types[i] == want)
        return want;
  }

  return None;
}

void rxvt_term::dnd_send(Window to, const char *message, long l1, long l2,
                         long l3, long l4) {
  XEvent ev;

  memset(&ev, 0, sizeof(ev));
  ev.xclient.type = ClientMessage;
  ev.xclient.display = dpy;
  ev.xclient.window = to;
  ev.xclient.message_type = display->atom(message);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = parent;
  ev.xclient.data.l[1] = l1;
  ev.xclient.data.l[2] = l2;
  ev.xclient.data.l[3] = l3;
  ev.xclient.data.l[4] = l4;

  XSendEvent(dpy, to, False, NoEventMask, &ev);
  XFlush(dpy);
}

void rxvt_term::dnd_finish(bool accepted) {
  if (dnd_source)
    dnd_send(dnd_source, "XdndFinished", accepted,
             accepted ? display->atom("XdndActionCopy") : None);

  dnd_source = None;
  dnd_type = None;
}

// a ClientMessage: true if it was part of a drag
bool rxvt_term::dnd_client_message(XClientMessageEvent &ev) {
  Atom type = ev.message_type;

  if (type == display->atom("XdndEnter")) {
    dnd_source = ev.data.l[0];
    dnd_type = None;

    if (ev.data.l[1] & 1) {
      // more than three types: they are in a property of the source
      Atom actual;
      int format;
      unsigned long count, after;
      unsigned char *data = 0;

      if (XGetWindowProperty(dpy, dnd_source, display->atom("XdndTypeList"), 0,
                             1024, False, XA_ATOM, &actual, &format, &count,
                             &after, &data) == Success &&
          data && actual == XA_ATOM && format == 32)
        dnd_type = dnd_choose_type((Atom *)data, count);

      if (data)
        XFree(data);
    } else {
      Atom types[3] = {(Atom)ev.data.l[2], (Atom)ev.data.l[3],
                       (Atom)ev.data.l[4]};
      dnd_type = dnd_choose_type(types, 3);
    }

    return true;
  }

  if (type == display->atom("XdndPosition")) {
    if ((Window)ev.data.l[0] == dnd_source)
      dnd_send(dnd_source, "XdndStatus", dnd_type ? 1 : 0, 0, 0,
               dnd_type ? display->atom("XdndActionCopy") : None);
    return true;
  }

  if (type == display->atom("XdndLeave")) {
    dnd_source = None;
    dnd_type = None;
    return true;
  }

  if (type == display->atom("XdndDrop")) {
    if ((Window)ev.data.l[0] != dnd_source)
      return true;

    if (!dnd_type)
      dnd_finish(false);
    else
      XConvertSelection(dpy, display->atom("XdndSelection"), dnd_type,
                        display->atom("_WRXVT_DND"), parent, ev.data.l[2]);

    return true;
  }

  return false;
}

static void add_quoted(rxvt_buf &b, const char *s, size_t n) {
  bool plain = n > 0;

  for (size_t i = 0; i < n; i++)
    if (!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "_./+:@%,=-",
                s[i]) ||
        (i == 0 && s[i] == '-'))
      plain = false;

  if (plain) {
    b.add(s, n);
    return;
  }

  b.add('\'');
  for (size_t i = 0; i < n; i++)
    if (s[i] == '\'')
      b.add("'\\''");
    else
      b.add(s[i]);
  b.add('\'');
}

static int hex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}

// text/uri-list: one URI per line; file:// ones become shell-quoted paths
static char *uri_list_to_words(const char *list, size_t len) {
  rxvt_buf out;
  const char *end = list + len;

  for (const char *line = list; line < end;) {
    const char *eol = line;
    while (eol < end && *eol != '\r' && *eol != '\n')
      eol++;

    if (eol > line && *line != '#') {
      rxvt_buf word;

      if (eol - line > 7 && !strncmp(line, "file://", 7)) {
        // skip the host name
        const char *p = (const char *)memchr(line + 7, '/', eol - line - 7);

        for (; p && p < eol; p++) {
          int h, l;

          if (*p == '%' && p + 2 < eol && (h = hex(p[1])) >= 0 &&
              (l = hex(p[2])) >= 0) {
            word.add((char)(h * 16 + l));
            p += 2;
          } else
            word.add(*p);
        }
      } else
        word.add(line, eol - line);

      if (word.len) {
        add_quoted(out, word.data, word.len);
        out.add(' ');
      }
    }

    line = eol;
    while (line < end && (*line == '\r' || *line == '\n'))
      line++;
  }

  return out.release();
}

// the dropped data has arrived (SelectionNotify for _WRXVT_DND)
void rxvt_term::dnd_selection_notify(XSelectionEvent &ev) {
  if (!dnd_source)
    return;

  if (ev.property == None) {
    dnd_finish(false);
    return;
  }

  Atom actual;
  int format;
  unsigned long count, after;
  unsigned char *data = 0;

  if (XGetWindowProperty(dpy, parent, ev.property, 0, 4 << 20, True,
                         AnyPropertyType, &actual, &format, &count, &after,
                         &data) != Success ||
      !data || format != 8 || actual == display->atom("INCR")) {
    // (INCR, for data over the server's request size, isn't supported)
    if (data)
      XFree(data);
    dnd_finish(false);
    return;
  }

  char *utf8;

  if (dnd_type == display->atom("text/uri-list"))
    utf8 = uri_list_to_words((const char *)data, count);
  else
    utf8 = strndup((const char *)data, count);

  XFree(data);

  if (*utf8) {
    // paste it in the locale's encoding
    wchar_t *w = rxvt_utf8towcs(utf8);
    char *text = rxvt_wcstombs(w);
    paste(text, strlen(text));
    free(text);
    free(w);
  }

  free(utf8);
  dnd_finish(true);
}

#endif
