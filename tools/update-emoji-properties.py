#!/usr/bin/env python3
"""Generate emoji-properties.h from Unicode 17.0.0 data (no network at build time).

Usage: update-emoji-properties.py emoji-data.txt GraphemeBreakProperty.txt
Sources: https://www.unicode.org/Public/17.0.0/ucd/emoji/emoji-data.txt
         https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakProperty.txt
Unicode data license: doc/UNICODE-LICENSE.txt
"""
import hashlib
from pathlib import Path
import sys

if len(sys.argv) != 3:
    sys.exit("usage: update-emoji-properties.py emoji-data.txt GraphemeBreakProperty.txt")

properties = {"Emoji": 1, "Extended_Pictographic": 2, "Emoji_Modifier": 4,
              "Emoji_Modifier_Base": 8, "Extend": 16, "Regional_Indicator": 32}
values = bytearray(0x110000)
for filename in sys.argv[1:]:
    for line in Path(filename).read_text().splitlines():
        fields = line.split("#", 1)[0].strip().split(";")
        if len(fields) != 2 or fields[1].strip() not in properties:
            continue
        limits = fields[0].strip().split("..")
        first, last = int(limits[0], 16), int(limits[-1], 16)
        for codepoint in range(first, last + 1):
            values[codepoint] |= properties[fields[1].strip()]
print("/* Generated from Unicode 17.0.0; see tools/update-emoji-properties.py.")
for filename in sys.argv[1:]:
    print(" * " + Path(filename).name + " SHA256 " + hashlib.sha256(Path(filename).read_bytes()).hexdigest())
print(" * Unicode license: doc/UNICODE-LICENSE.txt. Do not edit by hand. */")
print("#ifndef RXVT_EMOJI_PROPERTIES_H\n#define RXVT_EMOJI_PROPERTIES_H\n#include <stdint.h>")
print("enum { EMOJI = 1, EMOJI_PICTOGRAPHIC = 2, EMOJI_MODIFIER = 4,\n       EMOJI_MODIFIER_BASE = 8, EMOJI_EXTEND = 16, EMOJI_REGIONAL = 32 };")
print("static const struct { uint32_t first, last; unsigned char flags; } emoji_ranges[] = {")
first = 0
for end in range(1, len(values) + 1):
    if end == len(values) or values[end] != values[first]:
        if values[first]:
            print("  { 0x%x, 0x%x, %d }," % (first, end - 1, values[first]))
        first = end
print("};\nstatic inline unsigned int emoji_properties (uint32_t c)\n{")
print("  unsigned int lo = 0, hi = sizeof emoji_ranges / sizeof *emoji_ranges;\n  while (lo < hi)\n    {\n      unsigned int mid = lo + (hi - lo) / 2;\n      if (c < emoji_ranges[mid].first) hi = mid;\n      else if (c > emoji_ranges[mid].last) lo = mid + 1;\n      else return emoji_ranges[mid].flags;\n    }\n  return 0;\n}\n#endif")
