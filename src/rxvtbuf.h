// A growable byte buffer, for building text (share.C, record.C). The
// terminal doesn't link the C++ standard library, so no std::string.
#ifndef RXVT_BUF_H_
#define RXVT_BUF_H_

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct rxvt_buf {
  char *data; // always 0-terminated once something was added
  size_t len, cap;

  rxvt_buf() : data(0), len(0), cap(0) {}
  ~rxvt_buf() { free(data); }

  void reserve(size_t n) {
    if (len + n + 1 > cap) {
      cap = max(cap * 2, len + n + 1);
      data = (char *)rxvt_realloc(data, cap);
    }
  }

  void add(const void *p, size_t n) {
    reserve(n);
    memcpy(data + len, p, n);
    len += n;
    data[len] = 0;
  }

  void add(const char *s) { add(s, strlen(s)); }
  void add(char c) { add(&c, 1); }

  void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(0, 0, fmt, ap);
    va_end(ap);
    reserve(n);
    va_start(ap, fmt);
    vsnprintf(data + len, n + 1, fmt, ap);
    va_end(ap);
    len += n;
  }

  void consume(size_t n) {
    memmove(data, data + n, len - n);
    len -= n;
    if (data)
      data[len] = 0;
  }

  void clear() {
    len = 0;
    if (data)
      data[0] = 0;
  }

  // the text, given to the caller (free () it); the buffer is empty after
  char *release() {
    char *d = data ? data : strdup("");
    data = 0;
    len = cap = 0;
    return d;
  }

  void add_utf8(unicode_t c) {
    char s[4];
    int n;

    if (c < 0x80)
      s[0] = c, n = 1;
    else if (c < 0x800)
      s[0] = 0xc0 | c >> 6, s[1] = 0x80 | (c & 0x3f), n = 2;
    else if (c < 0x10000)
      s[0] = 0xe0 | c >> 12, s[1] = 0x80 | (c >> 6 & 0x3f),
      s[2] = 0x80 | (c & 0x3f), n = 3;
    else if (c < 0x110000)
      s[0] = 0xf0 | c >> 18, s[1] = 0x80 | (c >> 12 & 0x3f),
      s[2] = 0x80 | (c >> 6 & 0x3f), s[3] = 0x80 | (c & 0x3f), n = 4;
    else
      s[0] = '?', n = 1;

    add(s, n);
  }

  // a character inside a JSON string
  void add_json_char(unicode_t c) {
    if (c == '"' || c == '\\') {
      add('\\');
      add((char)c);
    } else if (c < 0x20 || c == 0x7f || (c >= 0xd800 && c < 0xe000))
      printf("\\u%04x", c < 0xd800 ? c : 0xfffd);
    else
      add_utf8(c);
  }
};

#endif
