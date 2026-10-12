#include "../src/emoji-sequences.h"
#include <assert.h>
#include <stdio.h>

int main ()
{
  uint32_t flag[] = {0x1f1ee, 0x1f1f9};
  assert (emoji_can_join (flag, 1, flag[1]));
  assert (emoji_is_sequence (flag, 2));
  assert (!emoji_can_join (flag, 2, 0x1f1e6));
  uint32_t family[] = {0x1f469, 0x1f3fd, 0x200d, 0x1f469, 0x200d, 0x1f467};
  assert (emoji_is_sequence (family, 6));
  assert (!emoji_is_sequence (family, 3));
  uint32_t invalid[] = {0x1f680, 0x1f3fd};
  assert (!emoji_is_sequence (invalid, 2));
  uint32_t heart[] = {0x2764, 0xfe0f};
  assert (emoji_is_sequence (heart, 2));
  heart[1] = 0xfe0e;
  assert (!emoji_is_sequence (heart, 2));
  uint32_t keycap[] = {'1', 0xfe0f, 0x20e3};
  assert (emoji_is_sequence (keycap, 3));
  uint32_t england[] = {0x1f3f4, 0xe0067, 0xe0062, 0xe0065, 0xe006e, 0xe0067, 0xe007f};
  assert (emoji_is_sequence (england, 7));
  assert (!emoji_is_sequence (england, 6));
  assert (!emoji_can_join (england, 7, 0xe0067));
  uint32_t combining[] = {0x1f469, 0x0308, 0x200d, 0x1f4bb};
  assert (emoji_is_sequence (combining, 4));
  uint32_t text[] = {'a', 0x200d, 0x1f4bb};
  assert (!emoji_is_sequence (text, 3));
  assert (!(emoji_properties (0x2500) & EMOJI_PICTOGRAPHIC));
  assert (!emoji_can_join (family, EMOJI_MAX_CODEPOINTS, 0x200d));
  puts ("emoji sequence tests passed");
}
