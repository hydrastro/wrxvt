/*
 * Grapheme clusters (mode 2027): where Unicode's extended grapheme
 * clusters end (UAX #29, "Grapheme Cluster Boundaries", rules GB3 to
 * GB999), for a terminal that keeps each cluster in one cell (or two,
 * when wide). See screen.C, scr_add_lines.
 *
 * Copyright 2026 wrxvt contributors; same license as rxvt-unicode.
 */
#ifndef RXVT_GRAPHEME_H
#define RXVT_GRAPHEME_H

#include "grapheme-properties.h"

/*
 * Does c continue the cluster s[0] .. s[n - 1] (no boundary before c)?
 * The rules only look back within the cluster, which is all they need:
 * GB9c and GB11 look for their start inside it, and GB12/13 count the
 * regional indicators at its end (a pair is a cluster of its own).
 */
static inline bool grapheme_continues(const uint32_t *s, unsigned int n,
                                      uint32_t c) {
  if (!n)
    return false;

  unsigned int pa = grapheme_properties(s[n - 1]);
  unsigned int pb = grapheme_properties(c);
  unsigned int a = GRAPHEME_GCB(pa), b = GRAPHEME_GCB(pb);

  if (a == GCB_CR && b == GCB_LF) // GB3
    return true;
  if (a == GCB_Control || a == GCB_CR || a == GCB_LF) // GB4
    return false;
  if (b == GCB_Control || b == GCB_CR || b == GCB_LF) // GB5
    return false;
  if (a == GCB_L &&
      (b == GCB_L || b == GCB_V || b == GCB_LV || b == GCB_LVT)) // GB6
    return true;
  if ((a == GCB_LV || a == GCB_V) && (b == GCB_V || b == GCB_T)) // GB7
    return true;
  if ((a == GCB_LVT || a == GCB_T) && b == GCB_T) // GB8
    return true;
  if (b == GCB_Extend || b == GCB_ZWJ) // GB9
    return true;
  if (b == GCB_SpacingMark) // GB9a
    return true;
  if (a == GCB_Prepend) // GB9b
    return true;

  // GB9c: Consonant [Extend Linker]* Linker [Extend Linker]* x Consonant
  // (Indic conjuncts: a consonant, a virama, the next consonant)
  if (GRAPHEME_INCB(pb) == INCB_Consonant) {
    bool linker = false;

    for (unsigned int i = n; i--;) {
      unsigned int incb = GRAPHEME_INCB(grapheme_properties(s[i]));

      if (incb == INCB_Linker)
        linker = true;
      else if (incb == INCB_Consonant) {
        if (linker)
          return true;
        break;
      } else if (incb != INCB_Extend)
        break;
    }
  }

  // GB11: ExtPict Extend* ZWJ x ExtPict (emoji joined with ZWJ)
  if (a == GCB_ZWJ && (pb & GRAPHEME_EXTPICT)) {
    unsigned int i = n - 1;

    while (i && GRAPHEME_GCB(grapheme_properties(s[i - 1])) == GCB_Extend)
      i--;

    if (i && (grapheme_properties(s[i - 1]) & GRAPHEME_EXTPICT))
      return true;
  }

  // GB12, GB13: regional indicators go in pairs (flags)
  if (a == GCB_Regional_Indicator && b == GCB_Regional_Indicator) {
    unsigned int ri = 0;

    for (unsigned int i = n; i-- && GRAPHEME_GCB(grapheme_properties(s[i])) ==
                                        GCB_Regional_Indicator;)
      ri++;

    return ri % 2 == 1;
  }

  return false; // GB999
}

/*
 * Does adding c make the cluster s[0] .. s[n - 1] (which continues with
 * c) an emoji drawn two cells wide, when it started as a one-cell
 * character? VS16 after an emoji that has a text form (❤ U+2764,
 * digits for keycaps), the second regional indicator of a flag.
 */
static inline bool grapheme_widens(const uint32_t *s, unsigned int n,
                                   uint32_t c) {
  if (c == 0xfe0f)
    return n && (grapheme_properties(s[0]) & GRAPHEME_EMOJI);

  return n == 1 &&
         GRAPHEME_GCB(grapheme_properties(c)) == GCB_Regional_Indicator &&
         GRAPHEME_GCB(grapheme_properties(s[0])) == GCB_Regional_Indicator;
}

#endif
