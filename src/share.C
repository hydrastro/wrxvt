/*----------------------------------------------------------------------*
 * File:	share.C
 *----------------------------------------------------------------------*
 *
 * Sharing a terminal on a web page. The share:start action makes the
 * terminal a small web server: a browser opening its link sees the screen
 * live, and with the control link can type into it too.
 *
 *  - Two links, each with its own random 128-bit key: one to watch, one
 *    to watch and type. New keys every time sharing starts; share:stop
 *    disconnects everybody and the links stop working.
 *  - The key is after the # of the link, so browsers never send it in a
 *    request (nor in Referer headers); the page sends it over the
 *    WebSocket, and takes it out of the address bar.
 *  - It listens on 127.0.0.1 unless shareAddress says otherwise; anything
 *    but a local address needs -insecure, since the connection is not
 *    encrypted (put it behind ssh -L or a TLS proxy instead).
 *  - The window title says the terminal is shared and how many people are
 *    watching, and a notification tells when someone opens a link.
 *
 * The page draws the screen from what the terminal sends: rows of text
 * runs with their colours and attributes, sent again only when they
 * change, at most 30 times a second.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#if ENABLE_SHARE

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "rxvtbuf.h"
#include "share-page.h"

#define SHARE_MAX_CLIENTS   16
#define SHARE_MAX_REQUEST   8192         // an HTTP request, headers included
#define SHARE_MAX_MESSAGE   (1 << 20)    // a message from the page (a paste)
#define SHARE_MAX_BACKLOG   (4 << 20)    // unsent output before a client is dropped
#define SHARE_LOGIN_TIME    10.          // seconds to present a key

// a JSON string from text in the locale's encoding
static void
add_json_string (rxvt_buf &b, const char *s)
{
  wchar_t *ws = rxvt_mbstowcs (s ? s : "");

  b.add ('"');
  for (wchar_t *p = ws; *p; p++)
    b.add_json_char (*p);
  b.add ('"');

  free (ws);
}

/*----------------------------------------------------------------------*/
// SHA-1, for the WebSocket handshake only (RFC 6455 asks for it)

static void
sha1 (const unsigned char *msg, size_t len, unsigned char digest[20])
{
  uint32_t h[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
  size_t blocks = (len + 8) / 64 + 1;

  for (size_t b = 0; b < blocks; b++)
    {
      uint32_t w[80];

      for (int i = 0; i < 64; i++)
        {
          size_t pos = b * 64 + i;
          unsigned char c;

          if (pos < len)
            c = msg[pos];
          else if (pos == len)
            c = 0x80;
          else if (pos >= blocks * 64 - 8)
            c = (uint64_t)len * 8 >> (blocks * 64 - 1 - pos) * 8;
          else
            c = 0;

          if (i % 4 == 0)
            w[i / 4] = 0;
          w[i / 4] |= (uint32_t)c << (24 - i % 4 * 8);
        }

      for (int i = 16; i < 80; i++)
        {
          uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
          w[i] = x << 1 | x >> 31;
        }

      uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];

      for (int i = 0; i < 80; i++)
        {
          uint32_t f, k;

          if (i < 20)
            f = (bb & c) | (~bb & d), k = 0x5a827999;
          else if (i < 40)
            f = bb ^ c ^ d, k = 0x6ed9eba1;
          else if (i < 60)
            f = (bb & c) | (bb & d) | (c & d), k = 0x8f1bbcdc;
          else
            f = bb ^ c ^ d, k = 0xca62c1d6;

          uint32_t t = (a << 5 | a >> 27) + f + e + k + w[i];
          e = d;
          d = c;
          c = bb << 30 | bb >> 2;
          bb = a;
          a = t;
        }

      h[0] += a;
      h[1] += bb;
      h[2] += c;
      h[3] += d;
      h[4] += e;
    }

  for (int i = 0; i < 20; i++)
    digest[i] = h[i / 4] >> (24 - i % 4 * 8);
}

/*----------------------------------------------------------------------*/
// reading the little JSON objects the page sends: {"type":"key","key":"Enter",...}

// the value of "name" as a string (unescaped, UTF-8), or 0; free () it
static char *
json_get_string (const char *json, const char *name)
{
  rxvt_buf key;
  key.printf ("\"%s\"", name);

  for (const char *p = strstr (json, key.data); p; p = strstr (p + 1, key.data))
    {
      const char *q = p + key.len;
      while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
      if (*q++ != ':')
        continue;
      while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
      if (*q++ != '"')
        return 0;

      rxvt_buf out;
      unsigned int high = 0;     // a pending high surrogate

      while (*q && *q != '"')
        {
          unicode_t c = (unsigned char)*q++;

          if (c != '\\')
            {
              out.add ((char)c);
              continue;
            }

          switch (*q++)
            {
              case 'n': c = '\n'; break;
              case 'r': c = '\r'; break;
              case 't': c = '\t'; break;
              case 'b': c = '\b'; break;
              case 'f': c = '\f'; break;
              case 'u':
                {
                  c = 0;
                  for (int i = 0; i < 4; i++)
                    {
                      int d = *q >= '0' && *q <= '9' ? *q - '0'
                            : *q >= 'a' && *q <= 'f' ? *q - 'a' + 10
                            : *q >= 'A' && *q <= 'F' ? *q - 'A' + 10 : -1;
                      if (d < 0)
                        return 0;
                      c = c * 16 + d;
                      q++;
                    }

                  if (c >= 0xd800 && c < 0xdc00)
                    {
                      high = c;
                      continue;
                    }

                  if (c >= 0xdc00 && c < 0xe000)
                    {
                      if (!high)
                        continue;
                      c = 0x10000 + ((high - 0xd800) << 10) + (c - 0xdc00);
                    }

                  high = 0;
                  out.add_utf8 (c);
                  continue;
                }
              case 0:
                return 0;
              default:
                c = q[-1]; // \" \\ \/
            }

          out.add ((char)c);
        }

      if (*q != '"')
        return 0;

      return strdup (out.data ? out.data : "");
    }

  return 0;
}

static bool
json_get_bool (const char *json, const char *name)
{
  rxvt_buf key;
  key.printf ("\"%s\"", name);

  const char *p = strstr (json, key.data);
  if (!p)
    return false;

  p += key.len;
  while (*p == ' ' || *p == ':') p++;

  return !strncmp (p, "true", 4);
}

/*----------------------------------------------------------------------*/

struct share_server;

struct share_client
{
  share_server *server;
  int fd;
  ev::io io_ev;
  ev_tstamp opened;

  enum { HTTP, WEBSOCKET } state;
  enum { NONE, VIEW, CONTROL } role;
  bool close_after_write;
  bool need_full;              // send the whole screen next
  uint32_t *hashes;            // the rows as this client last got them
  int hash_rows;
  int cursor[3];
  char *title;

  rxvt_buf in, out, message;  // message: a fragmented WebSocket message

  share_client (share_server *server, int fd);
  ~share_client ();

  void io_cb (ev::io &w, int revents);
  void update_events ();
  bool process_http ();
  bool process_frames ();
  bool handle_message (const char *json);
  void send_frame (int opcode, const char *data, size_t len);
  void send_text (const rxvt_buf &b) { send_frame (1, b.data, b.len); }
  void send_close (int code, const char *reason);
  void respond (const char *status, const char *type, const char *body, size_t len);
};

struct share_server
{
  rxvt_term *term;
  int fd;
  ev::io accept_ev;
  ev::timer update_ev;         // screen changes go out after a moment, together
  ev::timer housekeeping_ev;   // drops connections that never showed a key
  char key[2][33];             // watch, control
  char *url[2];
  share_client *clients[SHARE_MAX_CLIENTS];
  int nclients;

  share_server (rxvt_term *term) : term (term), fd (-1), nclients (0)
  {
    url[0] = url[1] = 0;
    accept_ev.set<share_server, &share_server::accept_cb> (this);
    update_ev.set<share_server, &share_server::update_cb> (this);
    housekeeping_ev.set<share_server, &share_server::housekeeping_cb> (this);
  }

  ~share_server ();

  bool start (const char *&error);
  void accept_cb (ev::io &w, int revents);
  void update_cb (ev::timer &w, int revents);
  void housekeeping_cb (ev::timer &w, int revents);
  void changed ();
  void remove (share_client *c);
  int watching () const;

  void hello (share_client *c);
  void encode_row (rxvt_buf &b, int row);
  const char *color (int idx, char buf[8]);
  void send_key (const char *name, bool shift, bool ctrl, bool alt);
};

/*----------------------------------------------------------------------*/

share_client::share_client (share_server *server, int fd)
: server (server), fd (fd), state (HTTP), role (NONE), close_after_write (false),
  need_full (true), hashes (0), hash_rows (0), title (0)
{
  opened = ev::now ();
  cursor[0] = cursor[1] = cursor[2] = -1;
  io_ev.set<share_client, &share_client::io_cb> (this);
  io_ev.start (fd, ev::READ);
}

share_client::~share_client ()
{
  io_ev.stop ();
  close (fd);
  free (hashes);
  free (title);
}

void
share_client::update_events ()
{
  io_ev.set (fd, out.len ? ev::READ | ev::WRITE : ev::READ);
}

void
share_client::io_cb (ev::io &w, int revents)
{
  if (revents & ev::WRITE)
    {
      ssize_t n = write (fd, out.data, out.len);

      if (n < 0 && errno != EAGAIN && errno != EINTR)
        return server->remove (this);

      if (n > 0)
        out.consume (n);

      if (!out.len && close_after_write)
        return server->remove (this);

      // a client that fell behind gets the whole screen once it caught up
      if (!out.len && need_full && role != NONE)
        server->changed ();
    }

  if (revents & ev::READ)
    {
      char buf[16384];
      ssize_t n = read (fd, buf, sizeof (buf));

      if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR))
        return server->remove (this);

      if (n > 0 && !close_after_write)
        {
          in.add (buf, n);

          if (!(state == HTTP ? process_http () : process_frames ()))
            return server->remove (this);
        }
    }

  if (out.len > SHARE_MAX_BACKLOG)
    return server->remove (this);

  update_events ();
}

void
share_client::respond (const char *status, const char *type, const char *body, size_t len)
{
  out.printf ("HTTP/1.1 %s\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %lu\r\n"
              "Cache-Control: no-store\r\n"
              "X-Frame-Options: DENY\r\n"
              "X-Content-Type-Options: nosniff\r\n"
              "Referrer-Policy: no-referrer\r\n"
              "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; "
              "style-src 'unsafe-inline'; connect-src 'self'\r\n"
              "Connection: close\r\n\r\n",
              status, type, (unsigned long)len);
  out.add (body, len);
  close_after_write = true;
}

// a header's value in the request, or 0; free () it
static char *
header (const char *request, const char *name)
{
  size_t n = strlen (name);

  for (const char *line = strstr (request, "\r\n"); line; line = strstr (line, "\r\n"))
    {
      line += 2;

      if (!strncasecmp (line, name, n) && line[n] == ':')
        {
          const char *v = line + n + 1;
          while (*v == ' ' || *v == '\t') v++;

          const char *end = strstr (v, "\r\n");
          while (end > v && (end[-1] == ' ' || end[-1] == '\t')) end--;

          return strndup (v, end - v);
        }
    }

  return 0;
}

static bool
has_token (const char *list, const char *token)
{
  if (!list)
    return false;

  size_t n = strlen (token);

  for (const char *p = list; *p; )
    {
      while (*p == ' ' || *p == ',') p++;
      if (!strncasecmp (p, token, n) && (!p[n] || p[n] == ',' || p[n] == ' '))
        return true;
      while (*p && *p != ',') p++;
    }

  return false;
}

// false: drop the connection
bool
share_client::process_http ()
{
  char *end = strstr (in.data, "\r\n\r\n");

  if (!end)
    {
      if (in.len > SHARE_MAX_REQUEST)
        {
          static const char msg[] = "request too large\n";
          respond ("431 Request Header Fields Too Large", "text/plain", msg, sizeof (msg) - 1);
        }
      return true;
    }

  *end = 0;

  char method[16], path[256];
  if (sscanf (in.data, "%15s %255s HTTP/1.", method, path) != 2)
    return false;

  if (char *q = strchr (path, '?'))
    *q = 0;

  if (strcmp (method, "GET"))
    {
      static const char msg[] = "only GET\n";
      respond ("405 Method Not Allowed", "text/plain", msg, sizeof (msg) - 1);
    }
  else if (!strcmp (path, "/") || !strcmp (path, "/index.html"))
    respond ("200 OK", "text/html; charset=utf-8", share_page, sizeof (share_page) - 1);
  else if (!strcmp (path, "/ws"))
    {
      char *upgrade = header (in.data, "Upgrade");
      char *connection = header (in.data, "Connection");
      char *ws_key = header (in.data, "Sec-WebSocket-Key");
      char *version = header (in.data, "Sec-WebSocket-Version");
      char *origin = header (in.data, "Origin");
      char *host = header (in.data, "Host");
      bool ok = has_token (upgrade, "websocket") && has_token (connection, "upgrade")
                && ws_key && strlen (ws_key) < 64 && version && !strcmp (version, "13");

      // only our own page may connect: another web site open in the same
      // browser can't, even before it would need a key
      if (ok && origin)
        {
          const char *o = origin;
          if (!strncmp (o, "http://", 7)) o += 7;
          else if (!strncmp (o, "https://", 8)) o += 8;
          else o = "";
          ok = host && !strcasecmp (o, host);
        }

      if (ok)
        {
          rxvt_buf accept;
          accept.add (ws_key);
          accept.add ("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

          unsigned char digest[20];
          sha1 ((const unsigned char *)accept.data, accept.len, digest);
          char *b64 = rxvt_base64_encode ((const char *)digest, 20);

          out.printf ("HTTP/1.1 101 Switching Protocols\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Accept: %s\r\n\r\n", b64);
          free (b64);
          state = WEBSOCKET;
        }
      else
        {
          static const char msg[] = "not a WebSocket request from this page\n";
          respond ("400 Bad Request", "text/plain", msg, sizeof (msg) - 1);
        }

      free (upgrade);
      free (connection);
      free (ws_key);
      free (version);
      free (origin);
      free (host);
    }
  else
    {
      static const char msg[] = "not found\n";
      respond ("404 Not Found", "text/plain", msg, sizeof (msg) - 1);
    }

  in.consume (end + 4 - in.data);

  // frames may have come right after the handshake
  return state == WEBSOCKET ? process_frames () : true;
}

void
share_client::send_frame (int opcode, const char *data, size_t len)
{
  unsigned char h[10];
  int n;

  h[0] = 0x80 | opcode;
  if (len < 126)
    h[1] = len, n = 2;
  else if (len < 65536)
    h[1] = 126, h[2] = len >> 8, h[3] = len, n = 4;
  else
    {
      h[1] = 127;
      for (int i = 0; i < 8; i++)
        h[2 + i] = (uint64_t)len >> (56 - 8 * i);
      n = 10;
    }

  out.add (h, n);
  out.add (data, len);
}

void
share_client::send_close (int code, const char *reason)
{
  rxvt_buf b;
  b.add ((char)(code >> 8));
  b.add ((char)code);
  b.add (reason);
  send_frame (8, b.data, b.len);
  close_after_write = true;
}

bool
share_client::process_frames ()
{
  while (in.len >= 2 && !close_after_write)
    {
      unsigned char *p = (unsigned char *)in.data;
      bool fin = p[0] & 0x80;
      int opcode = p[0] & 0x0f;
      uint64_t len = p[1] & 0x7f;
      size_t head = 2;

      if (!(p[1] & 0x80))
        return false; // frames from browsers are always masked

      if (len == 126)
        {
          if (in.len < 4)
            return true;
          len = p[2] << 8 | p[3];
          head = 4;
        }
      else if (len == 127)
        {
          if (in.len < 10)
            return true;
          len = 0;
          for (int i = 0; i < 8; i++)
            len = len << 8 | p[2 + i];
          head = 10;
        }

      if (len > SHARE_MAX_MESSAGE || message.len + len > SHARE_MAX_MESSAGE)
        {
          send_close (1009, "message too big");
          return true;
        }

      if (in.len < head + 4 + len)
        return true;

      unsigned char *mask = p + head;
      unsigned char *data = mask + 4;
      for (uint64_t i = 0; i < len; i++)
        data[i] ^= mask[i & 3];

      switch (opcode)
        {
          case 0: // continuation
          case 1: // text
            message.add (data, len);
            if (fin)
              {
                bool ok = handle_message (message.data ? message.data : "");
                message.clear ();
                if (!ok)
                  return true; // closing
              }
            break;
          case 8: // close
            send_close (1000, "");
            break;
          case 9: // ping
            send_frame (10, (const char *)data, len);
            break;
          case 10: // pong
            break;
          default:
            send_close (1003, "only text messages");
            break;
        }

      in.consume (head + 4 + len);
    }

  return true;
}

// constant time, so the time an answer takes says nothing about the key
static bool
same_key (const char *a, const char *b)
{
  size_t n = strlen (b);
  unsigned char diff = strlen (a) != n;

  for (size_t i = 0; i < n; i++)
    diff |= a[i < strlen (a) ? i : 0] ^ b[i];

  return !diff;
}

// false: the connection is closing
bool
share_client::handle_message (const char *json)
{
  char *type = json_get_string (json, "type");

  if (!type)
    {
      free (type);
      return true;
    }

  rxvt_term *term = server->term;

  if (role == NONE)
    {
      char *key = !strcmp (type, "auth") ? json_get_string (json, "token") : 0;

      if (key && same_key (key, server->key[1]))
        role = CONTROL;
      else if (key && same_key (key, server->key[0]))
        role = VIEW;

      free (key);
      free (type);

      if (role == NONE)
        {
          rxvt_buf b;
          b.add ("{\"type\":\"error\",\"message\":\"This link doesn't work (any more): "
                 "sharing was stopped or started again.\"}");
          send_text (b);
          send_close (1008, "wrong key");
          return false;
        }

      server->hello (this);

      term->share_notify (role == CONTROL
                          ? "Someone opened the control link: they can type in this terminal."
                          : "Someone is watching this terminal (read-only link).");
      term->update_title ();
      return true;
    }

  if (role == CONTROL)
    {
      if (!strcmp (type, "text") || !strcmp (type, "paste"))
        {
          if (char *data = json_get_string (json, "data"))
            {
              wchar_t *ws = rxvt_utf8towcs (data);
              char *mb = rxvt_wcstombs (ws);

              if (!strcmp (type, "paste"))
                term->paste (mb, strlen (mb));
              else
                term->tt_write_user_input (mb, strlen (mb));

              free (mb);
              free (ws);
              free (data);
            }
        }
      else if (!strcmp (type, "key"))
        {
          if (char *name = json_get_string (json, "key"))
            {
              server->send_key (name, json_get_bool (json, "shift"), json_get_bool (json, "ctrl"),
                           json_get_bool (json, "alt"));
              free (name);
            }
        }
    }

  // what a watcher sends is ignored
  free (type);
  return true;
}

/*----------------------------------------------------------------------*/

share_server::~share_server ()
{
  accept_ev.stop ();
  update_ev.stop ();
  housekeeping_ev.stop ();

  for (int i = 0; i < nclients; i++)
    {
      share_client *c = clients[i];

      // say goodbye if the socket takes it at once
      if (c->state == share_client::WEBSOCKET && c->role != share_client::NONE)
        {
          c->out.clear ();
          rxvt_buf b;
          b.add ("{\"type\":\"bye\",\"message\":\"The terminal stopped sharing.\"}");
          c->send_text (b);
          c->send_close (1001, "sharing stopped");
          ssize_t ignored = write (c->fd, c->out.data, c->out.len);
          (void)ignored;
        }

      delete c;
    }

  if (fd >= 0)
    close (fd);

  free (url[0]);
  free (url[1]);
}

int
share_server::watching () const
{
  int n = 0;

  for (int i = 0; i < nclients; i++)
    n += clients[i]->role != share_client::NONE;

  return n;
}

void
share_server::remove (share_client *c)
{
  bool counted = c->role != share_client::NONE;

  for (int i = 0; i < nclients; i++)
    if (clients[i] == c)
      {
        clients[i] = clients[--nclients];
        break;
      }

  delete c;

  if (counted)
    term->update_title ();
}

static bool
loopback (const sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    return (ntohl (((const sockaddr_in *)sa)->sin_addr.s_addr) >> 24) == 127;

  if (sa->sa_family == AF_INET6)
    {
      const in6_addr &a = ((const sockaddr_in6 *)sa)->sin6_addr;
      return IN6_IS_ADDR_LOOPBACK (&a)
             || (IN6_IS_ADDR_V4MAPPED (&a) && a.s6_addr[12] == 127);
    }

  return false;
}

bool
share_server::start (const char *&error)
{
  const char *address = term->rs[Rs_shareAddress] ? term->rs[Rs_shareAddress] : "127.0.0.1";
  int port = term->rs[Rs_sharePort] ? atoi (term->rs[Rs_sharePort]) : 0;

  sockaddr_storage ss;
  memset (&ss, 0, sizeof (ss));
  socklen_t sslen;
  sockaddr_in *sin = (sockaddr_in *)&ss;
  sockaddr_in6 *sin6 = (sockaddr_in6 *)&ss;

  if (inet_pton (AF_INET, address, &sin->sin_addr) == 1)
    {
      sin->sin_family = AF_INET;
      sin->sin_port = htons (port);
      sslen = sizeof (*sin);
    }
  else if (inet_pton (AF_INET6, address, &sin6->sin6_addr) == 1)
    {
      sin6->sin6_family = AF_INET6;
      sin6->sin6_port = htons (port);
      sslen = sizeof (*sin6);
    }
  else
    {
      error = "shareAddress must be an IP address (like 127.0.0.1 or 192.168.1.10).";
      return false;
    }

  if (!loopback ((sockaddr *)&ss) && !term->option (Opt_insecure))
    {
      error = "shareAddress is not a local address: anyone on the network could read "
              "the unencrypted connection. Start wrxvt with -insecure to allow it, or "
              "share on 127.0.0.1 through ssh -L or a TLS proxy.";
      return false;
    }

  // the keys
  unsigned char random[32];
  int rfd = open ("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (rfd < 0 || read (rfd, random, sizeof (random)) != sizeof (random))
    {
      if (rfd >= 0)
        close (rfd);
      error = "no random numbers (/dev/urandom) for the links' keys.";
      return false;
    }
  close (rfd);

  for (int k = 0; k < 2; k++)
    for (int i = 0; i < 16; i++)
      sprintf (key[k] + 2 * i, "%02x", random[16 * k + i]);

  fd = socket (ss.ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  int on = 1;
  if (fd >= 0)
    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on));

  if (fd < 0 || bind (fd, (sockaddr *)&ss, sslen) < 0 || listen (fd, 8) < 0)
    {
      error = strerror (errno);
      return false;
    }

  getsockname (fd, (sockaddr *)&ss, &sslen);
  port = ntohs (ss.ss_family == AF_INET ? sin->sin_port : sin6->sin6_port);

  // the host part of the links
  char host[256];
  bool any = ss.ss_family == AF_INET ? sin->sin_addr.s_addr == htonl (INADDR_ANY)
                                     : IN6_IS_ADDR_UNSPECIFIED (&sin6->sin6_addr);
  if (any)
    {
      if (gethostname (host, sizeof (host) - 1) < 0)
        strcpy (host, "localhost");
      host[sizeof (host) - 1] = 0;
    }
  else if (ss.ss_family == AF_INET6)
    snprintf (host, sizeof (host), "[%s]", address);
  else
    snprintf (host, sizeof (host), "%s", address);

  for (int k = 0; k < 2; k++)
    {
      rxvt_buf u;
      u.printf ("http://%s:%d/#%s", host, port, key[k]);
      url[k] = strdup (u.data);
    }

  accept_ev.start (fd, ev::READ);
  housekeeping_ev.start (1., 1.);
  return true;
}

void
share_server::accept_cb (ev::io &w, int revents)
{
  int cfd = accept4 (fd, 0, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);

  if (cfd < 0)
    return;

  if (nclients == SHARE_MAX_CLIENTS)
    {
      close (cfd);
      return;
    }

  int on = 1;
  setsockopt (cfd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on));

  clients[nclients++] = new share_client (this, cfd);
}

void
share_server::housekeeping_cb (ev::timer &w, int revents)
{
  for (int i = nclients; i--; )
    if (i < nclients && clients[i]->role == share_client::NONE
        && ev::now () - clients[i]->opened > SHARE_LOGIN_TIME)
      remove (clients[i]);
}

// something on the screen changed: tell the clients soon
void
share_server::changed ()
{
  if (!update_ev.is_active () && watching ())
    {
      update_ev.set (1. / 30.);
      update_ev.start ();
    }
}

const char *
share_server::color (int idx, char buf[8])
{
  rgba c = term->pix_colors_focused[idx];
  snprintf (buf, 8, "#%02x%02x%02x", c.r >> 8, c.g >> 8, c.b >> 8);
  return buf;
}

void
share_server::hello (share_client *c)
{
  char fg[8], bg[8];
  rxvt_buf b;

  b.printf ("{\"type\":\"hello\",\"role\":\"%s\",\"cols\":%d,\"rows\":%d,\"fg\":\"%s\",\"bg\":\"%s\"}",
            c->role == share_client::CONTROL ? "control" : "view", term->ncol, term->nrow,
            color (Color_fg, fg), color (Color_bg, bg));
  c->send_text (b);
  c->need_full = true;
  changed ();
}

// one row of the screen: [[text, fg, bg, flags], ...]
void
share_server::encode_row (rxvt_buf &b, int row)
{
  line_t &l = ROW_of (term, row);
  bool valid = l.valid () && l.t && l.r;
  int ncol = term->ncol;
  rxvt_buf text;
  rend_t last = 0;
  int fg = 0, bg = 0, flags = 0;
  bool have = false, first = true;

  b.add ('[');

  // a run ends: [text, fg, bg, flags]
  auto end_run = [&] ()
  {
    if (!have)
      return;

    char cf[8], cb[8];
    b.printf ("%s[\"%s\",\"%s\",\"%s\",%d]", first ? "" : ",", text.data ? text.data : "",
              color (fg, cf), color (bg, cb), flags);
    text.clear ();
    have = false;
    first = false;
  };

  for (int col = 0; col < ncol; col++)
    {
      text_t t = valid ? l.t[col] : ' ';
      rend_t r = valid ? l.r[col] : DEFAULT_RSTYLE;

      if (t == NOCHAR)
        continue; // the second half of a wide character

      bool wide = valid && col + 1 < ncol && l.t[col + 1] == NOCHAR;

      // the attributes that show
      rend_t key = r & (RS_fgMask | RS_bgMask | RS_Bold | RS_Italic | RS_Uline | RS_RVid
                        | RS_Faint | RS_Strike | RS_Overline | RS_Conceal);

      // a wide character is a run of its own: the page gives it two cells
      if (!have || key != last || wide || (flags & 128))
        {
          end_run ();

          // the colours as the window draws them (bold in bright colours...)
          fg = term->fgcolor_of (r);
          bg = term->bgcolor_of (r);
#ifndef NO_BOLD_UNDERLINE_REVERSE
          if (r & RS_Bold && fg == Color_fg && term->rs[Rs_color + Color_BD])
            fg = Color_BD;
#endif
          if (r & RS_RVid)
            ::swap (fg, bg);

          flags = (r & RS_Bold ? 1 : 0) | (r & RS_Italic ? 2 : 0) | (r & RS_Uline ? 4 : 0)
                  | (r & RS_Strike ? 8 : 0) | (r & RS_Faint ? 16 : 0)
                  | (r & RS_Overline ? 32 : 0) | (wide ? 128 : 0);
          last = key;
          have = true;
        }

      if (r & RS_Conceal)
        t = ' ';

      if (IS_IMAGE_TILE (t))
        text.add_json_char (' ');   // pictures are not sent (yet)
#if ENABLE_COMBINING
      else if (IS_COMPOSE (t))
        {
          wchar_t chars[64];
          int n = rxvt_composite.expand (t);

          if (n <= 64)
            {
              rxvt_composite.expand (t, chars);
              for (int i = 0; i < n; i++)
                text.add_json_char (chars[i]);
            }
        }
#endif
      else
        text.add_json_char (t);
    }

  end_run ();
  b.add (']');
}

static uint32_t
row_hash (line_t &l, int ncol)
{
  uint32_t h = 2166136261u;

  if (!l.valid () || !l.t || !l.r)
    return h;

  const unsigned char *p = (const unsigned char *)l.t;
  for (size_t i = 0; i < ncol * sizeof (text_t); i++)
    h = (h ^ p[i]) * 16777619u;

  p = (const unsigned char *)l.r;
  for (size_t i = 0; i < ncol * sizeof (rend_t); i++)
    h = (h ^ p[i]) * 16777619u;

  return h ? h : 1; // 0 means "not sent"
}

void
share_server::update_cb (ev::timer &w, int revents)
{
  int nrow = term->nrow, ncol = term->ncol;
  uint32_t *hashes = (uint32_t *)rxvt_malloc (nrow * sizeof (uint32_t));
  rxvt_buf *rows = new rxvt_buf[nrow];   // each row's JSON, made once

  for (int row = 0; row < nrow; row++)
    hashes[row] = row_hash (ROW_of (term, row), ncol);

  int cursor[3] = { term->screen.cur.row, term->screen.cur.col,
                    !!(term->priv_modes & PrivMode_VisibleCursor) };
  char *title = term->app_title ? strdup (term->app_title) : strdup ("");

  for (int i = 0; i < nclients; i++)
    {
      share_client *c = clients[i];

      if (c->role == share_client::NONE || c->close_after_write)
        continue;

      // still sending the last screens: catch up with a whole one later
      if (c->out.len > 256 * 1024)
        {
          c->need_full = true;
          continue;
        }

      if (c->hash_rows != nrow)
        {
          free (c->hashes);
          c->hashes = (uint32_t *)rxvt_calloc (nrow, sizeof (uint32_t));
          c->hash_rows = nrow;
          c->need_full = true;
        }

      rxvt_buf b;
      b.printf ("{\"type\":\"screen\",\"cols\":%d,\"rows\":%d,\"lines\":[", ncol, nrow);
      bool changed = false;

      for (int row = 0; row < nrow; row++)
        if (c->need_full || c->hashes[row] != hashes[row])
          {
            if (!rows[row].len)
              encode_row (rows[row], row);

            b.printf ("%s[%d,", changed ? "," : "", row);
            b.add (rows[row].data, rows[row].len);
            b.add (']');
            c->hashes[row] = hashes[row];
            changed = true;
          }

      b.printf ("],\"cursor\":[%d,%d,%s]", cursor[0], cursor[1], cursor[2] ? "true" : "false");

      if (memcmp (c->cursor, cursor, sizeof (cursor)))
        {
          memcpy (c->cursor, cursor, sizeof (cursor));
          changed = true;
        }

      if (!c->title || strcmp (c->title, title))
        {
          free (c->title);
          c->title = strdup (title);
          b.add (",\"title\":");
          add_json_string (b, title);
          changed = true;
        }

      b.add ('}');

      if (changed)
        {
          c->send_text (b);
          c->update_events ();
        }

      c->need_full = false;
    }

  free (title);
  delete [] rows;
  free (hashes);
}

// a key from the page, as this terminal sends it (rxvt's sequences, since
// TERM says rxvt; xterm's modifier parameters for the arrows)
void
share_server::send_key (const char *name, bool shift, bool ctrl, bool alt)
{
  static const struct { const char *name; const char *seq; } tilde[] = {
    { "Insert", "2" }, { "Delete", "3" }, { "PageUp", "5" }, { "PageDown", "6" },
    { "Home", "7" }, { "End", "8" },
    { "F1", "11" }, { "F2", "12" }, { "F3", "13" }, { "F4", "14" }, { "F5", "15" },
    { "F6", "17" }, { "F7", "18" }, { "F8", "19" }, { "F9", "20" }, { "F10", "21" },
    { "F11", "23" }, { "F12", "24" },
  };
  rxvt_buf s;

  if (alt)
    s.add ('\033');

  if (!strcmp (name, "Enter"))
    s.add ('\r');
  else if (!strcmp (name, "Tab"))
    s.add (shift ? "\033[Z" : "\t");
  else if (!strcmp (name, "Backspace"))
    s.add (ctrl ? '\b' : '\177');
  else if (!strcmp (name, "Escape"))
    s.add ('\033');
  else if (!strncmp (name, "Arrow", 5))
    {
      char dir = !strcmp (name + 5, "Up") ? 'A' : !strcmp (name + 5, "Down") ? 'B'
               : !strcmp (name + 5, "Right") ? 'C' : 'D';
      int mod = 1 + shift + 2 * alt + 4 * ctrl;

      if (mod > 1)
        {
          s.clear ();
          s.printf ("\033[1;%d%c", mod, dir);
        }
      else
        s.printf ((term->priv_modes & PrivMode_aplCUR) ? "\033O%c" : "\033[%c", dir);
    }
  else
    {
      for (unsigned int i = 0; i < ecb_array_length (tilde); i++)
        if (!strcmp (name, tilde[i].name))
          {
            // rxvt marks Shift with $, Control with ^, both with @
            s.printf ("\033[%s%c", tilde[i].seq, shift && ctrl ? '@' : ctrl ? '^' : shift ? '$' : '~');
            break;
          }
    }

  if (s.len > (size_t)alt)
    term->tt_write_user_input (s.data, s.len);
}

/*----------------------------------------------------------------------*/
// the terminal's side

void
rxvt_term::share_notify (const char *text)
{
  notify_as ("wrxvt: sharing", text);
}

void
rxvt_term::share_start ()
{
  if (share)
    {
      share_copy_link (false);
      return;
    }

  share = new share_server (this);

  const char *error = 0;
  if (!share->start (error))
    {
      delete share;
      share = 0;

      char *text = (char *)rxvt_malloc (strlen (error) + 40);
      sprintf (text, "Can't share this terminal: %s", error);
      rxvt_warn ("%s\n", text);
      share_notify (text);
      free (text);
      return;
    }

  // the links on the window, for scripts: xprop _WRXVT_SHARE
  char *both = (char *)rxvt_malloc (strlen (share->url[0]) + strlen (share->url[1]) + 2);
  sprintf (both, "%s\n%s", share->url[0], share->url[1]);
  set_utf8_property (display->atom ("_WRXVT_SHARE"), both);
  free (both);

  update_title ();
  share_copy_link (false);
}

void
rxvt_term::share_stop ()
{
  if (!share)
    return;

  delete share;
  share = 0;

  XDeleteProperty (dpy, parent, display->atom ("_WRXVT_SHARE"));
  update_title ();
}

// put a link on the clipboard (and the primary selection), and say so
void
rxvt_term::share_copy_link (bool control)
{
  if (!share)
    return;

  const char *url = share->url[control];
  wchar_t *text = rxvt_mbstowcs (url);
  set_selection (text, true, true);
  free (text);

  rxvt_buf msg;
  msg.printf (control
              ? "Link to watch AND TYPE copied (give it only to someone you trust): %s"
              : "Link to watch copied: %s\nshare:control copies the link that can type.",
              url);
  share_notify (msg.data);
}

void
rxvt_term::share_changed ()
{
  if (share)
    share->changed ();
}

int
rxvt_term::share_watching ()
{
  return share ? share->watching () : 0;
}

#endif
