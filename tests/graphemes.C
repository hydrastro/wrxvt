// Grapheme cluster boundaries (src/grapheme.h): cases from each rule of
// UAX #29. Checked against an independent implementation on random text
// too; see doc/graphemes.md. Built and run by tests/test_units.py.
#include "../src/grapheme.h"
#include <stdio.h>
#include <string.h>

static int failures;

// the sizes of the clusters of s, as "1 2 1"
static void check(const char *name, const uint32_t *s, unsigned int n,
                  const char *want) {
  char got[256] = "";
  unsigned int start = 0;

  for (unsigned int i = 1; i <= n; i++)
    if (i == n || !grapheme_continues(s + start, i - start, s[i])) {
      snprintf(got + strlen(got), sizeof got - strlen(got), "%s%u",
               *got ? " " : "", i - start);
      start = i;
    }

  if (strcmp(got, want)) {
    printf("FAIL %s: %s, want %s\n", name, got, want);
    failures++;
  }
}

#define CHECK(name, want, ...)                                                 \
  do {                                                                         \
    uint32_t s[] = {__VA_ARGS__};                                              \
    check(name, s, sizeof s / sizeof *s, want);                                \
  } while (0)

int main() {
  CHECK("GB3 CR LF", "2", '\r', '\n');
  CHECK("GB4/5 controls", "1 1 1", 'a', 0x7, 0x301);
  CHECK("GB6 L L V", "3", 0x1100, 0x1100, 0x1161);
  CHECK("GB7 LV T", "2", 0xac00, 0x11a8);
  CHECK("GB8 LVT T", "2", 0xac01, 0x11a8);
  CHECK("Hangul break", "1 1", 0xac01, 0x1161);
  CHECK("GB9 marks", "3 1", 'e', 0x301, 0x308, 'x');
  CHECK("GB9 ZWJ", "2 1", 'a', 0x200d, 'b');
  CHECK("GB9a spacing mark", "2", 0x915, 0x93f);
  CHECK("GB9b prepend", "2", 0x600, '1');
  CHECK("GB9c conjunct", "4", 0x915, 0x94d, 0x937, 0x93f);
  CHECK("GB9c two linkers", "5", 0x915, 0x94d, 0x94d, 0x937, 0x93f);
  CHECK("GB9c needs a linker", "1 1", 0x915, 0x937);
  CHECK("GB9c needs a consonant first", "2 1", 'a', 0x94d, 0x937);
  CHECK("GB11 family", "5", 0x1f468, 0x200d, 0x1f469, 0x200d, 0x1f467);
  CHECK("GB11 with modifiers", "6", 0x1f469, 0x1f3fd, 0x200d, 0x1f469, 0x200d,
        0x1f467);
  CHECK("GB11 needs a pictograph", "2 1", 'a', 0x200d, 0x1f4bb);
  CHECK("GB12/13 flags", "2 2 1", 0x1f1ee, 0x1f1f9, 0x1f1fa, 0x1f1f8, 0x1f1e6);
  CHECK("keycap", "3", '1', 0xfe0f, 0x20e3);
  CHECK("tag flag", "7", 0x1f3f4, 0xe0067, 0xe0062, 0xe0065, 0xe006e, 0xe0067,
        0xe007f);
  CHECK("GB999", "1 1 1", 'a', 'b', 0x4e2d);

  uint32_t heart[] = {0x2764}, flag[] = {0x1f1ee}, a[] = {'a'};
  if (!grapheme_widens(heart, 1, 0xfe0f) ||
      !grapheme_widens(flag, 1, 0x1f1f9) || grapheme_widens(a, 1, 0xfe0f) ||
      grapheme_widens(a, 1, 0x301)) {
    puts("FAIL grapheme_widens");
    failures++;
  }

  if (failures)
    return 1;

  puts("grapheme cluster tests passed");
  return 0;
}
