/* Emoji-specific joining, using Unicode 17 properties. This is not a general
 * grapheme-width implementation. Terminal columns remain wcwidth-based.
 * Copyright 2026 rxvt-unicode contributors; same license as rxvt-unicode. */
#ifndef RXVT_EMOJI_SEQUENCES_H
#define RXVT_EMOJI_SEQUENCES_H
#include "emoji-properties.h"

enum { EMOJI_MAX_CODEPOINTS = 64, EMOJI_CACHE_ENTRIES = 256 };

static inline bool emoji_keycap_base(uint32_t c) {
  return (c >= '0' && c <= '9') || c == '#' || c == '*';
}

static inline bool emoji_can_join(const uint32_t *s, unsigned int n,
                                  uint32_t c) {
  if (!n || n >= EMOJI_MAX_CODEPOINTS)
    return false;

  unsigned int flags = emoji_properties(c);
  if (flags & EMOJI_REGIONAL)
    return n == 1 && (emoji_properties(s[0]) & EMOJI_REGIONAL);

  if (flags & EMOJI_MODIFIER) {
    unsigned int i = n;
    if (s[i - 1] == 0xfe0f && --i == 0)
      return false;
    return emoji_properties(s[i - 1]) & EMOJI_MODIFIER_BASE;
  }

  if (flags & EMOJI_PICTOGRAPHIC) {
    // UAX #29 GB11: Extended_Pictographic Extend* ZWJ × Extended_Pictographic.
    if (n < 2 || s[n - 1] != 0x200d)
      return false;
    unsigned int i = n - 1;
    while (i && (emoji_properties(s[i - 1]) & EMOJI_EXTEND))
      --i;
    return i && (emoji_properties(s[i - 1]) & EMOJI_PICTOGRAPHIC);
  }

  if (c == 0xfe0f)
    return emoji_properties(s[n - 1]) & EMOJI;
  if (c == 0xfe0e) // explicit text presentation must not select the colour path
    return false;
  if (c == 0x20e3)
    return emoji_keycap_base(s[0]) && (n == 1 || (n == 2 && s[1] == 0xfe0f));
  if (c >= 0xe0020 && c <= 0xe007f) {
    if (s[0] != 0x1f3f4 || (c == 0xe007f && n == 1))
      return false;
    for (unsigned int i = 1; i < n; ++i)
      if (s[i] < 0xe0020 || s[i] >= 0xe007f)
        return false;
    return true;
  }
  if (c == 0x200d || (flags & EMOJI_EXTEND)) {
    unsigned int i = n;
    while (i && (emoji_properties(s[i - 1]) & EMOJI_EXTEND))
      --i;
    return i && (emoji_properties(s[i - 1]) & EMOJI_PICTOGRAPHIC);
  }
  return false;
}

static inline bool emoji_is_sequence(const uint32_t *s, unsigned int n) {
  if (n < 2 || s[n - 1] == 0x200d)
    return false;
  bool special = false;
  for (unsigned int i = 1; i < n; ++i) {
    if (!emoji_can_join(s, i, s[i]))
      return false;
    special |= s[i] == 0xfe0f || s[i] == 0x20e3 || s[i] == 0x200d ||
               (emoji_properties(s[i]) & (EMOJI_MODIFIER | EMOJI_REGIONAL));
  }
  if (s[0] == 0x1f3f4 && n > 2 && s[1] >= 0xe0020 && s[1] < 0xe007f)
    return s[n - 1] == 0xe007f;
  return special;
}
#endif
