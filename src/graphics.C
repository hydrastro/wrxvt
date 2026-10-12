/*----------------------------------------------------------------------*
 * File:	graphics.C
 *----------------------------------------------------------------------*
 *
 * Pictures from image files: iTerm2's inline images (OSC 1337 ; File=)
 * and the kitty graphics protocol (APC G). They are shown like sixel
 * pictures (see sixel.C): tiles in the cells they cover.
 *
 * PNG, JPEG, GIF and the other formats gdk-pixbuf knows need a build with
 * pixbuf; the kitty protocol's raw RGB and RGBA work in any build.
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

// an image file (PNG, JPEG...) as ARGB pixels
static uint32_t *
decode_image (const unsigned char *data, size_t len, int &w, int &h)
{
#if HAVE_PIXBUF
  GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
  uint32_t *px = 0;

  if (gdk_pixbuf_loader_write (loader, data, len, 0)
      && gdk_pixbuf_loader_close (loader, 0))
    {
      GdkPixbuf *pb = gdk_pixbuf_loader_get_pixbuf (loader);

      if (pb && gdk_pixbuf_get_bits_per_sample (pb) == 8)
        {
          w = gdk_pixbuf_get_width (pb);
          h = gdk_pixbuf_get_height (pb);

          if (w > 0 && h > 0 && w <= SIXEL_MAX_SIZE * 2 && h <= SIXEL_MAX_SIZE * 2)
            {
              int channels = gdk_pixbuf_get_n_channels (pb);
              bool alpha = gdk_pixbuf_get_has_alpha (pb);
              int stride = gdk_pixbuf_get_rowstride (pb);
              const unsigned char *pixels = gdk_pixbuf_get_pixels (pb);

              px = (uint32_t *)malloc ((size_t)w * h * sizeof (uint32_t));

              for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                  {
                    const unsigned char *p = pixels + (size_t)y * stride + x * channels;
                    px[(size_t)y * w + x] = (uint32_t)(alpha ? p[3] : 255) << 24
                                            | p[0] << 16 | p[1] << 8 | p[2];
                  }
            }
        }
    }
  else
    gdk_pixbuf_loader_close (loader, 0);

  g_object_unref (loader);
  return px;
#else
  return 0;
#endif
}

// scale ARGB pixels to nw x nh (area averaging down, bilinear up)
static uint32_t *
scale_image (const uint32_t *px, int w, int h, int nw, int nh)
{
  uint32_t *out = (uint32_t *)malloc ((size_t)nw * nh * sizeof (uint32_t));

  for (int y = 0; y < nh; y++)
    {
      // the source rows and columns this pixel covers
      int y0 = (int)((long long)y * h / nh), y1 = max (y0 + 1, (int)((long long)(y + 1) * h / nh));

      for (int x = 0; x < nw; x++)
        {
          int x0 = (int)((long long)x * w / nw), x1 = max (x0 + 1, (int)((long long)(x + 1) * w / nw));
          unsigned long a = 0, r = 0, g = 0, b = 0, n = 0;

          for (int sy = y0; sy < y1 && sy < h; sy++)
            for (int sx = x0; sx < x1 && sx < w; sx++)
              {
                uint32_t c = px[(size_t)sy * w + sx];
                unsigned int ca = c >> 24;
                // weigh the colour by its alpha, so transparent pixels don't darken edges
                a += ca;
                r += (c >> 16 & 0xff) * ca;
                g += (c >>  8 & 0xff) * ca;
                b += (c       & 0xff) * ca;
                n++;
              }

          uint32_t c = 0;
          if (a)
            c = (uint32_t)(a / n) << 24 | (uint32_t)(r / a) << 16 | (uint32_t)(g / a) << 8 | (uint32_t)(b / a);

          out[(size_t)y * nw + x] = c;
        }
    }

  return out;
}

/*
 * Fit pixels into cols x rows cells (0: whatever the other one and the
 * aspect ratio give, both 0: the natural size), keeping the aspect ratio
 * if asked. Returns the new pixels (the old ones are freed) and sizes.
 */
static uint32_t *
fit_image (uint32_t *px, int &w, int &h, int &cols, int &rows, int fw, int fh, bool keep_aspect)
{
  int nw = cols ? cols * fw : 0;
  int nh = rows ? rows * fh : 0;

  if (!nw && !nh)
    return px;

  if (!nw)
    nw = max (1, (int)((long long)w * nh / h));
  else if (!nh)
    nh = max (1, (int)((long long)h * nw / w));
  else if (keep_aspect)
    {
      // inside the box
      if ((long long)w * nh > (long long)h * nw)
        nh = max (1, (int)((long long)h * nw / w));
      else
        nw = max (1, (int)((long long)w * nh / h));
    }

  nw = min (nw, SIXEL_MAX_SIZE);
  nh = min (nh, SIXEL_MAX_SIZE);

  if (nw != w || nh != h)
    {
      uint32_t *n = scale_image (px, w, h, nw, nh);
      free (px);
      px = n;
      w = nw;
      h = nh;
    }

  return px;
}

/*
 * iTerm2: ESC ] 1337 ; File = [key=value;...] : <base64 file> ST
 *
 * inline=1 shows it (otherwise it would be a download: ignored); width
 * and height are N (cells), Npx, N% (of the window) or auto, and
 * preserveAspectRatio=0 stretches.
 */
void
rxvt_term::process_iterm_file (char *str)
{
  if (strncmp (str, "File=", 5))
    return;

  char *data = strchr (str, ':');
  if (!data)
    return;

  *data++ = 0;

  bool show = false, keep_aspect = true;
  int cols = 0, rows = 0, pw = 0, ph = 0;

  for (char *arg = strtok (str + 5, ";"); arg; arg = strtok (0, ";"))
    {
      char *eq = strchr (arg, '=');
      if (!eq)
        continue;

      *eq++ = 0;

      if (!strcmp (arg, "inline"))
        show = atoi (eq) == 1;
      else if (!strcmp (arg, "preserveAspectRatio"))
        keep_aspect = atoi (eq) != 0;
      else if (!strcmp (arg, "width") || !strcmp (arg, "height"))
        {
          bool width = arg[0] == 'w';
          int n = atoi (eq);
          const char *unit = eq + strspn (eq, "0123456789");

          if (n <= 0)
            continue; // auto

          if (!strcmp (unit, "px"))
            (width ? pw : ph) = n;
          else if (!strcmp (unit, "%"))
            (width ? cols : rows) = max (1, (width ? ncol : nrow) * min (n, 100) / 100);
          else
            (width ? cols : rows) = n;
        }
    }

  if (!show)
    return;

  int len = rxvt_base64_decode (data);
  if (len <= 0)
    return;

  int w, h;
  uint32_t *px = decode_image ((const unsigned char *)data, len, w, h);
  if (!px)
    return;

  // sizes in pixels are rounded up to cells
  if (pw)
    cols = (pw + fwidth - 1) / fwidth;
  if (ph)
    rows = (ph + fheight - 1) / fheight;

  min_it (cols, IMAGE_TILE_MAX);
  min_it (rows, IMAGE_TILE_MAX);

  px = fit_image (px, w, h, cols, rows, fwidth, fheight, keep_aspect);
  place_image (px, w, h, 0, 0, IMAGE_CURSOR_BELOW);
}

// APC strings: the kitty graphics protocol is the one we know
void
rxvt_term::process_apc_string (char *str)
{
  if (str[0] == 'G')
    process_kitty_graphics (str + 1);
}

void
rxvt_term::kitty_forget (unsigned int id)
{
  for (auto &s : kitty_store)
    if (s.pixels && (!id || s.id == id))
      {
        free (s.pixels);
        s.pixels = 0;
        s.id = 0;
      }
}

static bool
same_kitty_id (rxvt_term *, term_image *img, void *id)
{
  return img->kitty_id == *(unsigned int *)id;
}

static bool
any_kitty_image (rxvt_term *, term_image *img, void *)
{
  return img->kitty_id != 0;
}

/*
 * The kitty graphics protocol (https://sw.kovidgoyal.net/kitty/graphics-protocol/):
 *
 *   ESC _ G key=value,... ; base64 payload ESC \
 *
 * Supported: a= t (transmit), T (transmit and show), p (show a
 * transmitted image), d (delete), q (query); f= 24, 32 (raw RGB, RGBA,
 * with s= and v=) and 100 (PNG, any file format gdk-pixbuf reads); data
 * sent directly (t=d), in chunks (m=1); c= and r= (cells), C=1 (don't
 * move the cursor), i= (image ids), q= (quiet). Not supported (with an
 * error reply): compression, files and shared memory, animation,
 * placements relative to others, z-order.
 */
void
rxvt_term::process_kitty_graphics (char *str)
{
  char *payload = strchr (str, ';');
  if (payload)
    *payload++ = 0;
  else
    payload = (char *)"";

  // a continuation chunk only says m= (and maybe q=): use the keys of the first
  char keys[256];
  bool continuing = kitty_pending.data != 0;

  snprintf (keys, sizeof (keys), "%s", continuing ? kitty_pending.keys : str);

  char action = 't', medium = 'd', compression = 0;
  int format = 32, sw = 0, sh = 0, cols = 0, rows = 0, quiet = 0, more = 0, no_move = 0;
  unsigned int id = 0;
  char what = 'a'; // for a=d

  auto parse = [&](char *list)
  {
    for (char *kv = strtok (list, ","); kv; kv = strtok (0, ","))
      {
        if (!kv[0] || kv[1] != '=')
          continue;

        const char *v = kv + 2;
        int n = atoi (v);

        switch (kv[0])
          {
            case 'a': action = *v; break;
            case 't': medium = *v; break;
            case 'o': compression = *v; break;
            case 'f': format = n; break;
            case 's': sw = n; break;
            case 'v': sh = n; break;
            case 'c': cols = n; break;
            case 'r': rows = n; break;
            case 'q': quiet = n; break;
            case 'm': more = n; break;
            case 'C': no_move = n; break;
            case 'i': id = strtoul (v, 0, 10); break;
            case 'd': what = *v; break;
          }
      }
  };

  char first[256];
  snprintf (first, sizeof (first), "%s", keys);
  parse (first);

  if (continuing)
    {
      // m= and q= of this chunk
      more = 0;
      char chunk[256];
      snprintf (chunk, sizeof (chunk), "%s", str);
      for (char *kv = strtok (chunk, ","); kv; kv = strtok (0, ","))
        if (kv[0] == 'm' && kv[1] == '=')
          more = atoi (kv + 2);
        else if (kv[0] == 'q' && kv[1] == '=')
          quiet = atoi (kv + 2);
    }

  auto reply = [&](const char *msg)
  {
    bool ok = !strcmp (msg, "OK");

    if (!id || (ok && quiet >= 1) || (!ok && quiet >= 2))
      return;

    tt_printf ("\033_Gi=%u;%s\033\\", id, msg);
  };

  auto drop_pending = [&]()
  {
    free (kitty_pending.data);
    kitty_pending.data = 0;
    kitty_pending.len = kitty_pending.size = 0;
  };

  if (action == 'd')
    {
      // lower case keeps the image data, upper case frees it
      if (what == 'a' || what == 'A')
        {
          scr_erase_images (any_kitty_image, 0);
          if (what == 'A')
            kitty_forget (0);
        }
      else if ((what == 'i' || what == 'I') && id)
        {
          scr_erase_images (same_kitty_id, &id);
          if (what == 'I')
            kitty_forget (id);
        }
      return;
    }

  if (action != 't' && action != 'T' && action != 'p' && action != 'q')
    {
      reply ("EINVAL:unsupported action");
      return;
    }

  uint32_t *px = 0;
  int w = 0, h = 0;

  if (action == 'p')
    {
      // show an image transmitted before
      for (auto &s : kitty_store)
        if (s.pixels && s.id == id)
          {
            w = s.w;
            h = s.h;
            px = (uint32_t *)malloc ((size_t)w * h * sizeof (uint32_t));
            memcpy (px, s.pixels, (size_t)w * h * sizeof (uint32_t));
          }

      if (!px)
        {
          reply ("ENOENT:no such image");
          return;
        }
    }
  else
    {
      if (medium != 'd' || compression)
        {
          drop_pending ();
          reply ("EINVAL:only uncompressed data sent directly is supported");
          return;
        }

      // collect the chunks, decoded
      int len = rxvt_base64_decode (payload);
      if (len < 0)
        {
          drop_pending ();
          reply ("EINVAL:bad base64");
          return;
        }

      if (!continuing)
        {
          snprintf (kitty_pending.keys, sizeof (kitty_pending.keys), "%s", str);
          kitty_pending.len = 0;
        }

      if (kitty_pending.len + len + 1 > kitty_pending.size)
        {
          if (kitty_pending.len + len > (size_t)SIXEL_MAX_SIZE * SIXEL_MAX_SIZE * 4)
            {
              drop_pending ();
              reply ("EFBIG:too large");
              return;
            }

          kitty_pending.size = max (kitty_pending.size * 2, kitty_pending.len + len + 1);
          kitty_pending.data = (char *)rxvt_realloc (kitty_pending.data, kitty_pending.size);
        }

      memcpy (kitty_pending.data + kitty_pending.len, payload, len);
      kitty_pending.len += len;

      if (more)
        return; // more chunks follow

      const unsigned char *data = (const unsigned char *)kitty_pending.data;
      size_t total = kitty_pending.len;

      if (format == 24 || format == 32)
        {
          int bpp = format / 8;

          if (sw > 0 && sh > 0 && sw <= SIXEL_MAX_SIZE && sh <= SIXEL_MAX_SIZE
              && total >= (size_t)sw * sh * bpp)
            {
              w = sw;
              h = sh;
              px = (uint32_t *)malloc ((size_t)w * h * sizeof (uint32_t));

              for (size_t i = 0; i < (size_t)w * h; i++)
                {
                  const unsigned char *p = data + i * bpp;
                  px[i] = (uint32_t)(bpp == 4 ? p[3] : 255) << 24 | p[0] << 16 | p[1] << 8 | p[2];
                }
            }
        }
      else if (format == 100)
        px = decode_image (data, total, w, h);

      drop_pending ();

      if (!px)
        {
          reply ("EBADF:cannot decode the image");
          return;
        }

      if (action == 'q')
        {
          free (px);
          reply ("OK");
          return;
        }

      if (action == 't' || id)
        {
          // keep it for a=p: replace the same id, or the oldest
          kitty_forget (id);

          int slot = 0;
          for (int i = 0; i < (int)ecb_array_length (kitty_store); i++)
            if (!kitty_store[i].pixels)
              {
                slot = i;
                break;
              }
            else if (i == (int)ecb_array_length (kitty_store) - 1)
              {
                // all used: drop the first, move the others down
                free (kitty_store[0].pixels);
                memmove (kitty_store, kitty_store + 1, sizeof (kitty_store) - sizeof (kitty_store[0]));
                kitty_store[i].pixels = 0;
                slot = i;
              }

          kitty_store[slot].id = id;
          kitty_store[slot].w = w;
          kitty_store[slot].h = h;
          kitty_store[slot].pixels = (uint32_t *)malloc ((size_t)w * h * sizeof (uint32_t));
          memcpy (kitty_store[slot].pixels, px, (size_t)w * h * sizeof (uint32_t));
        }

      if (action == 't')
        {
          free (px);
          reply ("OK");
          return;
        }
    }

  // show it at the cursor, c x r cells (stretched, as kitty does)
  min_it (cols, IMAGE_TILE_MAX);
  min_it (rows, IMAGE_TILE_MAX);
  px = fit_image (px, w, h, cols, rows, fwidth, fheight, false);
  place_image (px, w, h, cols, rows, no_move ? IMAGE_CURSOR_STAY : IMAGE_CURSOR_AFTER, id);
  reply ("OK");
}

#endif
