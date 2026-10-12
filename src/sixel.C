/*----------------------------------------------------------------------*
 * File:	sixel.C
 *----------------------------------------------------------------------*
 *
 * Sixel graphics: DCS P1 ; P2 ; P3 q <data> ST draws a picture at the
 * cursor.
 *
 * A picture is kept in an image slot, and the cells it covers hold
 * special characters (IMAGE_TILE) that name the slot and the tile of the
 * picture the cell shows. As they are ordinary cell contents, pictures
 * scroll, go into the scrollback, are erased and overwritten exactly like
 * text; the built-in font draws the tiles (rxvtfont.C).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#if ENABLE_SIXEL

#include <stdlib.h>
#include <string.h>

// the 16 colours a VT340 starts with (percent, as sixel colours are)
static const unsigned char vt340_palette[16][3] = {
  {  0,  0,  0 }, { 20, 20, 80 }, { 80, 13, 13 }, { 20, 80, 20 },
  { 80, 20, 80 }, { 20, 80, 80 }, { 80, 80, 20 }, { 53, 53, 53 },
  { 26, 26, 26 }, { 33, 33, 60 }, { 60, 26, 26 }, { 33, 60, 33 },
  { 60, 33, 60 }, { 33, 60, 60 }, { 60, 60, 33 }, { 80, 80, 80 },
};

static uint32_t
rgb_percent (int r, int g, int b)
{
  r = r * 255 / 100;
  g = g * 255 / 100;
  b = b * 255 / 100;
  return 0xff000000 | min (r, 255) << 16 | min (g, 255) << 8 | min (b, 255);
}

// HLS as the VT340 has it: hue 0 is blue, lightness and saturation in percent
static uint32_t
hls_percent (int h, int l, int s)
{
  double hue = ((h + 240) % 360) / 360.;
  double lum = l / 100., sat = s / 100.;

  if (sat == 0)
    return rgb_percent (l, l, l);

  double q = lum < .5 ? lum * (1 + sat) : lum + sat - lum * sat;
  double p = 2 * lum - q;

  auto channel = [&](double t)
  {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1. / 6) return p + (q - p) * 6 * t;
    if (t < 1. / 2) return q;
    if (t < 2. / 3) return p + (q - p) * (2. / 3 - t) * 6;
    return p;
  };

  return rgb_percent ((int)(channel (hue + 1. / 3) * 100 + .5),
                      (int)(channel (hue) * 100 + .5),
                      (int)(channel (hue - 1. / 3) * 100 + .5));
}

/*
 * Decode sixel data into 32-bit ARGB pixels (alpha 0: not drawn).
 * Returns false if there is no picture.
 */
static bool
sixel_decode (const char *p, sixel_image &img)
{
  uint32_t palette[SIXEL_COLORS];

  for (int i = 0; i < SIXEL_COLORS; i++)
    palette[i] = i < 16 ? rgb_percent (vt340_palette[i][0], vt340_palette[i][1], vt340_palette[i][2])
                        : 0xff000000;

  int cap_w = 0, cap_h = 0;         // allocated size
  uint32_t *px = 0;
  int w = 0, h = 0;                 // size drawn so far, or declared
  int x = 0, y = 0;                 // the sixel cursor: x, top of the band
  int color = 0;
  int aspect = 1;                   // pixels per sixel bit, vertically

  // make room for pixels up to (nw, nh)
  auto grow = [&](int nw, int nh) -> bool
  {
    if (nw <= cap_w && nh <= cap_h)
      return true;

    if (nw > SIXEL_MAX_SIZE || nh > SIXEL_MAX_SIZE)
      return false;

    int ncw = max (nw, min (SIXEL_MAX_SIZE, cap_w * 2)), nch = max (nh, min (SIXEL_MAX_SIZE, cap_h * 2));
    ncw = max (ncw, 64); nch = max (nch, 64);
    ncw = min (ncw, SIXEL_MAX_SIZE); nch = min (nch, SIXEL_MAX_SIZE);

    uint32_t *n = (uint32_t *)calloc ((size_t)ncw * nch, sizeof (uint32_t));
    if (!n)
      return false;

    for (int row = 0; row < cap_h; row++)
      memcpy (n + (size_t)row * ncw, px + (size_t)row * cap_w, cap_w * sizeof (uint32_t));

    free (px);
    px = n;
    cap_w = ncw;
    cap_h = nch;
    return true;
  };

  // read numeric parameters separated by ';'
  auto params = [&](int *v, int max_n) -> int
  {
    int n = 0;
    for (;;)
      {
        int value = -1;
        while (*p >= '0' && *p <= '9')
          value = (value < 0 ? 0 : value) * 10 + (*p++ - '0'), value = min (value, 100000);

        if (n < max_n)
          v[n++] = value;

        if (*p != ';')
          break;
        p++;
      }
    return n;
  };

  auto put = [&](int bits, int count)
  {
    if (count <= 0)
      return;

    if (bits && !grow (x + count, y + 6 * aspect))
      {
        x += count;
        return;
      }

    for (int bit = 0; bit < 6; bit++)
      if (bits & (1 << bit))
        for (int a = 0; a < aspect; a++)
          {
            uint32_t *row = px + (size_t)(y + bit * aspect + a) * cap_w;
            for (int i = 0; i < count; i++)
              row[x + i] = palette[color];
          }

    if (bits)
      {
        max_it (w, x + count);
        // up to the highest bit set counts towards the height
        int top = 5;
        while (!(bits & (1 << top)))
          top--;
        max_it (h, y + (top + 1) * aspect);
      }

    x += count;
  };

  int declared_w = 0, declared_h = 0;

  while (*p)
    {
      char c = *p;

      if (c >= '?' && c <= '~')
        {
          put (c - '?', 1);
          p++;
        }
      else if (c == '!') // repeat
        {
          p++;
          int v[1];
          params (v, 1);
          int count = v[0] < 1 ? 1 : v[0];
          if (*p >= '?' && *p <= '~')
            put (*p++ - '?', count);
        }
      else if (c == '#') // colour: select, or define
        {
          p++;
          int v[5];
          int n = params (v, 5);

          if (v[0] >= 0)
            color = v[0] % SIXEL_COLORS;

          if (n >= 5)
            {
              if (v[1] == 1)
                palette[color] = hls_percent (max (v[2], 0) % 361, clamp (v[3], 0, 100), clamp (v[4], 0, 100));
              else if (v[1] == 2)
                palette[color] = rgb_percent (clamp (v[2], 0, 100), clamp (v[3], 0, 100), clamp (v[4], 0, 100));
            }
        }
      else if (c == '"') // raster attributes: Pan ; Pad ; Ph ; Pv
        {
          p++;
          int v[4] = { -1, -1, -1, -1 };
          int n = params (v, 4);

          if (n >= 2 && v[0] > 0 && v[1] > 0)
            aspect = clamp ((v[0] + v[1] / 2) / v[1], 1, 10);

          if (n >= 4 && v[2] > 0 && v[3] > 0)
            {
              declared_w = min (v[2], SIXEL_MAX_SIZE);
              declared_h = min (v[3], SIXEL_MAX_SIZE);
            }
        }
      else if (c == '$') // carriage return
        {
          x = 0;
          p++;
        }
      else if (c == '-') // next band
        {
          x = 0;
          y += 6 * aspect;
          p++;

          if (y >= SIXEL_MAX_SIZE)
            break;
        }
      else
        p++; // anything else is ignored
    }

  // the raster attributes give the size, even where nothing was drawn
  max_it (w, declared_w);
  max_it (h, declared_h);

  if (w <= 0 || h <= 0 || !grow (w, h))
    {
      free (px);
      return false;
    }

  // pack the rows
  img.w = w;
  img.h = h;
  img.pixels = (uint32_t *)malloc ((size_t)w * h * sizeof (uint32_t));

  for (int row = 0; row < h; row++)
    memcpy (img.pixels + (size_t)row * w, px + (size_t)row * cap_w, w * sizeof (uint32_t));

  free (px);
  return true;
}

// a free image slot (1..SIXEL_SLOTS-1); slots no cell shows any more are freed
int
rxvt_term::image_slot ()
{
  for (int i = 1; i < SIXEL_SLOTS; i++)
    if (!images[i])
      return i;

  // all taken: find the ones still on the screen or in the scrollback
  bool used[SIXEL_SLOTS] = { };

  for (int row = top_row; row < nrow; row++)
    {
      line_t &l = ROW (row);
      if (!l.valid ())
        continue;

      for (int col = 0; col < ncol; col++)
        if (IS_IMAGE_TILE (l.t[col]))
          used[IMAGE_TILE_SLOT (l.t[col])] = true;
    }

  // and on the other screen (normal or alternate), kept aside
  if (swap_buf)
    for (int row = 0; row < nrow; row++)
      {
        line_t &l = swap_buf[row];
        if (!l.valid () || !l.t)
          continue;

        for (int col = 0; col < ncol; col++)
          if (IS_IMAGE_TILE (l.t[col]))
            used[IMAGE_TILE_SLOT (l.t[col])] = true;
      }

  int free_slot = 0;

  for (int i = 1; i < SIXEL_SLOTS; i++)
    if (!used[i])
      {
        free_image (i);
        if (!free_slot)
          free_slot = i;
      }

  if (free_slot)
    return free_slot;

  // still none: reuse the oldest
  image_next = image_next % (SIXEL_SLOTS - 1) + 1;
  free_image (image_next);
  return image_next;
}

void
rxvt_term::free_image (int slot)
{
  if (sixel_image *img = images[slot])
    {
      if (img->pixmap)
        XFreePixmap (dpy, img->pixmap);
      free (img->pixels);
      delete img;
      images[slot] = 0;
    }
}

/*
 * DCS P1 ; P2 ; P3 q <sixel data> ST
 */
void
rxvt_term::process_sixel (const char *params, const char *data)
{
  sixel_image *img = new sixel_image;
  img->pixmap = 0;
  img->pixels = 0;

  if (!sixel_decode (data, *img))
    {
      delete img;
      return;
    }

  // P2 1: unset pixels stay transparent (we draw them in the background)
  img->cell_w = fwidth;
  img->cell_h = fheight;

  int slot = image_slot ();
  images[slot] = img;

  int cols = (img->w + fwidth - 1) / fwidth;
  int rows = (img->h + fheight - 1) / fheight;

  scr_add_image (slot, min (cols, IMAGE_TILE_MAX), min (rows, IMAGE_TILE_MAX));
}

// an XImage of the picture for this visual, in a pixmap the tiles are copied from
Pixmap
rxvt_term::image_pixmap (sixel_image *img)
{
  if (img->pixmap)
    return img->pixmap;

  if (visual->c_class != TrueColor && visual->c_class != DirectColor)
    return None;

  XImage *xi = XCreateImage (dpy, visual, depth, ZPixmap, 0, 0, img->w, img->h, 32, 0);
  if (!xi)
    return None;

  xi->data = (char *)malloc ((size_t)xi->bytes_per_line * img->h);

  rgba bg = pix_colors[Color_bg];

  auto shift = [](unsigned long mask, int &sh, int &bits)
  {
    sh = 0; bits = 0;
    while (mask && !(mask & 1)) mask >>= 1, sh++;
    while (mask & 1) mask >>= 1, bits++;
  };

  int rs, rb, gs, gb, bs, bb;
  shift (visual->red_mask,   rs, rb);
  shift (visual->green_mask, gs, gb);
  shift (visual->blue_mask,  bs, bb);

  for (int y = 0; y < img->h; y++)
    for (int x = 0; x < img->w; x++)
      {
        uint32_t c = img->pixels[(size_t)y * img->w + x];
        unsigned int r, g, b;

        if (c >> 24)
          r = c >> 16 & 0xff, g = c >> 8 & 0xff, b = c & 0xff;
        else
          r = bg.r >> 8, g = bg.g >> 8, b = bg.b >> 8; // transparent

        unsigned long pixel = (unsigned long)(r >> (8 - min (rb, 8))) << rs
                            | (unsigned long)(g >> (8 - min (gb, 8))) << gs
                            | (unsigned long)(b >> (8 - min (bb, 8))) << bs;

        // an alpha channel (32-bit visuals): opaque
        unsigned long rest = ~(visual->red_mask | visual->green_mask | visual->blue_mask);
        if (depth == 32)
          pixel |= rest & 0xffffffffUL;

        XPutPixel (xi, x, y, pixel);
      }

  img->pixmap = XCreatePixmap (dpy, vt, img->w, img->h, depth);

  GC gc = XCreateGC (dpy, img->pixmap, 0, 0);
  XPutImage (dpy, img->pixmap, gc, xi, 0, 0, 0, 0, img->w, img->h);
  XFreeGC (dpy, gc);
  XDestroyImage (xi);

  // the pixmap is all we need from now on
  free (img->pixels);
  img->pixels = 0;

  return img->pixmap;
}

// draw the tile in cell code at x, y (called by the built-in font)
void
rxvt_term::draw_image_tile (Drawable d, GC gc, int x, int y, text_t code)
{
  sixel_image *img = images[IMAGE_TILE_SLOT (code)];
  if (!img)
    return;

  Pixmap pm = image_pixmap (img);
  if (!pm)
    return;

  int sx = IMAGE_TILE_COL (code) * img->cell_w;
  int sy = IMAGE_TILE_ROW (code) * img->cell_h;

  int w = min (fwidth, img->w - sx);
  int h = min (fheight, img->h - sy);

  if (w > 0 && h > 0)
    XCopyArea (dpy, pm, d, gc, sx, sy, w, h, x, y);
}

#endif
