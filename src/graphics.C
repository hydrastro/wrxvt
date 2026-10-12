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
static uint32_t *decode_image(const unsigned char *data, size_t len, int &w,
                              int &h) {
#if HAVE_PIXBUF
  GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
  uint32_t *px = 0;

  if (gdk_pixbuf_loader_write(loader, data, len, 0) &&
      gdk_pixbuf_loader_close(loader, 0)) {
    GdkPixbuf *pb = gdk_pixbuf_loader_get_pixbuf(loader);

    if (pb && gdk_pixbuf_get_bits_per_sample(pb) == 8) {
      w = gdk_pixbuf_get_width(pb);
      h = gdk_pixbuf_get_height(pb);

      if (w > 0 && h > 0 && w <= SIXEL_MAX_SIZE * 2 &&
          h <= SIXEL_MAX_SIZE * 2) {
        int channels = gdk_pixbuf_get_n_channels(pb);
        bool alpha = gdk_pixbuf_get_has_alpha(pb);
        int stride = gdk_pixbuf_get_rowstride(pb);
        const unsigned char *pixels = gdk_pixbuf_get_pixels(pb);

        px = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));

        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) {
            const unsigned char *p = pixels + (size_t)y * stride + x * channels;
            px[(size_t)y * w + x] = (uint32_t)(alpha ? p[3] : 255) << 24 |
                                    p[0] << 16 | p[1] << 8 | p[2];
          }
      }
    }
  } else
    gdk_pixbuf_loader_close(loader, 0);

  g_object_unref(loader);
  return px;
#else
  return 0;
#endif
}

// scale ARGB pixels to nw x nh (area averaging down, bilinear up)
static uint32_t *scale_image(const uint32_t *px, int w, int h, int nw, int nh) {
  uint32_t *out = (uint32_t *)malloc((size_t)nw * nh * sizeof(uint32_t));

  for (int y = 0; y < nh; y++) {
    // the source rows and columns this pixel covers
    int y0 = (int)((long long)y * h / nh),
        y1 = max(y0 + 1, (int)((long long)(y + 1) * h / nh));

    for (int x = 0; x < nw; x++) {
      int x0 = (int)((long long)x * w / nw),
          x1 = max(x0 + 1, (int)((long long)(x + 1) * w / nw));
      unsigned long a = 0, r = 0, g = 0, b = 0, n = 0;

      for (int sy = y0; sy < y1 && sy < h; sy++)
        for (int sx = x0; sx < x1 && sx < w; sx++) {
          uint32_t c = px[(size_t)sy * w + sx];
          unsigned int ca = c >> 24;
          // weigh the colour by its alpha, so transparent pixels don't darken
          // edges
          a += ca;
          r += (c >> 16 & 0xff) * ca;
          g += (c >> 8 & 0xff) * ca;
          b += (c & 0xff) * ca;
          n++;
        }

      uint32_t c = 0;
      if (a)
        c = (uint32_t)(a / n) << 24 | (uint32_t)(r / a) << 16 |
            (uint32_t)(g / a) << 8 | (uint32_t)(b / a);

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
static uint32_t *fit_image(uint32_t *px, int &w, int &h, int &cols, int &rows,
                           int fw, int fh, bool keep_aspect) {
  int nw = cols ? cols * fw : 0;
  int nh = rows ? rows * fh : 0;

  if (!nw && !nh)
    return px;

  if (!nw)
    nw = max(1, (int)((long long)w * nh / h));
  else if (!nh)
    nh = max(1, (int)((long long)h * nw / w));
  else if (keep_aspect) {
    // inside the box
    if ((long long)w * nh > (long long)h * nw)
      nh = max(1, (int)((long long)h * nw / w));
    else
      nw = max(1, (int)((long long)w * nh / h));
  }

  nw = min(nw, SIXEL_MAX_SIZE);
  nh = min(nh, SIXEL_MAX_SIZE);

  if (nw != w || nh != h) {
    uint32_t *n = scale_image(px, w, h, nw, nh);
    free(px);
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
void rxvt_term::process_iterm_file(char *str) {
  if (strncmp(str, "File=", 5))
    return;

  char *data = strchr(str, ':');
  if (!data)
    return;

  *data++ = 0;

  bool show = false, keep_aspect = true;
  int cols = 0, rows = 0, pw = 0, ph = 0;

  for (char *arg = strtok(str + 5, ";"); arg; arg = strtok(0, ";")) {
    char *eq = strchr(arg, '=');
    if (!eq)
      continue;

    *eq++ = 0;

    if (!strcmp(arg, "inline"))
      show = atoi(eq) == 1;
    else if (!strcmp(arg, "preserveAspectRatio"))
      keep_aspect = atoi(eq) != 0;
    else if (!strcmp(arg, "width") || !strcmp(arg, "height")) {
      bool width = arg[0] == 'w';
      int n = atoi(eq);
      const char *unit = eq + strspn(eq, "0123456789");

      if (n <= 0)
        continue; // auto

      if (!strcmp(unit, "px"))
        (width ? pw : ph) = n;
      else if (!strcmp(unit, "%"))
        (width ? cols : rows) =
            max(1, (width ? ncol : nrow) * min(n, 100) / 100);
      else
        (width ? cols : rows) = n;
    }
  }

  if (!show)
    return;

  int len = rxvt_base64_decode(data);
  if (len <= 0)
    return;

  int w, h;
  uint32_t *px = decode_image((const unsigned char *)data, len, w, h);
  if (!px)
    return;

  // sizes in pixels are rounded up to cells
  if (pw)
    cols = (pw + fwidth - 1) / fwidth;
  if (ph)
    rows = (ph + fheight - 1) / fheight;

  min_it(cols, IMAGE_TILE_MAX);
  min_it(rows, IMAGE_TILE_MAX);

  px = fit_image(px, w, h, cols, rows, fwidth, fheight, keep_aspect);
  place_image(px, w, h, 0, 0, IMAGE_CURSOR_BELOW);
}

// APC strings: the kitty graphics protocol is the one we know
void rxvt_term::process_apc_string(char *str) {
  if (str[0] == 'G')
    process_kitty_graphics(str + 1);
}

void rxvt_term::kitty_forget(unsigned int id) {
  for (auto &s : kitty_store)
    if (s.pixels && (!id || s.id == id)) {
      free(s.pixels);
      s.pixels = 0;
      s.id = 0;
    }

  // new data for an id: its placeholders' picture is made again
  for (auto &v : kitty_virtuals)
    if (v.id && v.fitted && (!id || v.id == id)) {
      if (v.fitted->pixmap)
        XFreePixmap(dpy, v.fitted->pixmap);
      free(v.fitted->pixels);
      delete v.fitted;
      v.fitted = 0;
    }
}

static bool same_kitty_id(rxvt_term *, term_image *img, void *id) {
  return img->kitty_id == *(unsigned int *)id;
}

static bool any_kitty_image(rxvt_term *, term_image *img, void *) {
  return img->kitty_id != 0;
}

/*
 * The kitty graphics protocol
 * (https://sw.kovidgoyal.net/kitty/graphics-protocol/):
 *
 *   ESC _ G key=value,... ; base64 payload ESC \
 *
 * Supported: a= t (transmit), T (transmit and show), p (show a
 * transmitted image), d (delete), q (query); f= 24, 32 (raw RGB, RGBA,
 * with s= and v=) and 100 (PNG, any file format gdk-pixbuf reads); data
 * sent directly (t=d), in chunks (m=1); c= and r= (cells), C=1 (don't
 * move the cursor), i= (image ids), q= (quiet), U=1 (virtual placements,
 * for Unicode placeholders: see the end of this file). Not supported
 * (with an error reply): compression, files and shared memory,
 * animation, placements relative to others, z-order.
 */
void rxvt_term::process_kitty_graphics(char *str) {
  char *payload = strchr(str, ';');
  if (payload)
    *payload++ = 0;
  else
    payload = (char *)"";

  // a continuation chunk only says m= (and maybe q=): use the keys of the first
  char keys[256];
  bool continuing = kitty_pending.data != 0;

  snprintf(keys, sizeof(keys), "%s", continuing ? kitty_pending.keys : str);

  char action = 't', medium = 'd', compression = 0;
  int format = 32, sw = 0, sh = 0, cols = 0, rows = 0, quiet = 0, more = 0,
      no_move = 0;
  int virtual_placement = 0;
  unsigned int id = 0, placement = 0;
  char what = 'a'; // for a=d

  auto parse = [&](char *list) {
    for (char *kv = strtok(list, ","); kv; kv = strtok(0, ",")) {
      if (!kv[0] || kv[1] != '=')
        continue;

      const char *v = kv + 2;
      int n = atoi(v);

      switch (kv[0]) {
      case 'a':
        action = *v;
        break;
      case 't':
        medium = *v;
        break;
      case 'o':
        compression = *v;
        break;
      case 'f':
        format = n;
        break;
      case 's':
        sw = n;
        break;
      case 'v':
        sh = n;
        break;
      case 'c':
        cols = n;
        break;
      case 'r':
        rows = n;
        break;
      case 'q':
        quiet = n;
        break;
      case 'm':
        more = n;
        break;
      case 'C':
        no_move = n;
        break;
      case 'i':
        id = strtoul(v, 0, 10);
        break;
      case 'p':
        placement = strtoul(v, 0, 10);
        break;
      case 'U':
        virtual_placement = n;
        break;
      case 'd':
        what = *v;
        break;
      }
    }
  };

  char first[256];
  snprintf(first, sizeof(first), "%s", keys);
  parse(first);

  if (continuing) {
    // m= and q= of this chunk
    more = 0;
    char chunk[256];
    snprintf(chunk, sizeof(chunk), "%s", str);
    for (char *kv = strtok(chunk, ","); kv; kv = strtok(0, ","))
      if (kv[0] == 'm' && kv[1] == '=')
        more = atoi(kv + 2);
      else if (kv[0] == 'q' && kv[1] == '=')
        quiet = atoi(kv + 2);
  }

  auto reply = [&](const char *msg) {
    bool ok = !strcmp(msg, "OK");

    if (!id || (ok && quiet >= 1) || (!ok && quiet >= 2))
      return;

    tt_printf("\033_Gi=%u;%s\033\\", id, msg);
  };

  auto drop_pending = [&]() {
    free(kitty_pending.data);
    kitty_pending.data = 0;
    kitty_pending.len = kitty_pending.size = 0;
  };

  if (action == 'd') {
    // lower case keeps the image data, upper case frees it
    if (what == 'a' || what == 'A') {
      scr_erase_images(any_kitty_image, 0);
      if (what == 'A')
        kitty_forget(0);
    } else if ((what == 'i' || what == 'I') && id) {
      scr_erase_images(same_kitty_id, &id);
      kitty_virtual_free(id); // (only i and I delete virtual placements)
      if (what == 'I')
        kitty_forget(id);
      scr_touch(true); // placeholders on the screen change
    }
    return;
  }

  if (action != 't' && action != 'T' && action != 'p' && action != 'q') {
    reply("EINVAL:unsupported action");
    return;
  }

  uint32_t *px = 0;
  int w = 0, h = 0;

  if (action == 'p') {
    // show an image transmitted before
    for (auto &s : kitty_store)
      if (s.pixels && s.id == id) {
        w = s.w;
        h = s.h;
        px = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
        memcpy(px, s.pixels, (size_t)w * h * sizeof(uint32_t));
      }

    if (!px) {
      reply("ENOENT:no such image");
      return;
    }
  } else {
    if (medium != 'd' || compression) {
      drop_pending();
      reply("EINVAL:only uncompressed data sent directly is supported");
      return;
    }

    // collect the chunks, decoded
    int len = rxvt_base64_decode(payload);
    if (len < 0) {
      drop_pending();
      reply("EINVAL:bad base64");
      return;
    }

    if (!continuing) {
      snprintf(kitty_pending.keys, sizeof(kitty_pending.keys), "%s", str);
      kitty_pending.len = 0;
    }

    if (kitty_pending.len + len + 1 > kitty_pending.size) {
      if (kitty_pending.len + len >
          (size_t)SIXEL_MAX_SIZE * SIXEL_MAX_SIZE * 4) {
        drop_pending();
        reply("EFBIG:too large");
        return;
      }

      kitty_pending.size =
          max(kitty_pending.size * 2, kitty_pending.len + len + 1);
      kitty_pending.data =
          (char *)rxvt_realloc(kitty_pending.data, kitty_pending.size);
    }

    memcpy(kitty_pending.data + kitty_pending.len, payload, len);
    kitty_pending.len += len;

    if (more)
      return; // more chunks follow

    const unsigned char *data = (const unsigned char *)kitty_pending.data;
    size_t total = kitty_pending.len;

    if (format == 24 || format == 32) {
      int bpp = format / 8;

      if (sw > 0 && sh > 0 && sw <= SIXEL_MAX_SIZE && sh <= SIXEL_MAX_SIZE &&
          total >= (size_t)sw * sh * bpp) {
        w = sw;
        h = sh;
        px = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));

        for (size_t i = 0; i < (size_t)w * h; i++) {
          const unsigned char *p = data + i * bpp;
          px[i] = (uint32_t)(bpp == 4 ? p[3] : 255) << 24 | p[0] << 16 |
                  p[1] << 8 | p[2];
        }
      }
    } else if (format == 100)
      px = decode_image(data, total, w, h);

    drop_pending();

    if (!px) {
      reply("EBADF:cannot decode the image");
      return;
    }

    if (action == 'q') {
      free(px);
      reply("OK");
      return;
    }

    if (action == 't' || id) {
      // keep it for a=p: replace the same id, or the oldest
      kitty_forget(id);

      // the stored ones take at most half of imageMemory: the oldest go
      for (;;) {
        size_t total = (size_t)w * h * 4;
        for (auto &st : kitty_store)
          if (st.pixels)
            total += (size_t)st.w * st.h * 4;

        int first = 0;
        while (first < (int)ecb_array_length(kitty_store) &&
               !kitty_store[first].pixels)
          first++;

        if (total <= image_memory_max() / 2 ||
            first == (int)ecb_array_length(kitty_store))
          break;

        free(kitty_store[first].pixels);
        kitty_store[first].pixels = 0;
        kitty_store[first].id = 0;
      }

      int slot = 0;
      for (int i = 0; i < (int)ecb_array_length(kitty_store); i++)
        if (!kitty_store[i].pixels) {
          slot = i;
          break;
        } else if (i == (int)ecb_array_length(kitty_store) - 1) {
          // all used: drop the first, move the others down
          free(kitty_store[0].pixels);
          memmove(kitty_store, kitty_store + 1,
                  sizeof(kitty_store) - sizeof(kitty_store[0]));
          kitty_store[i].pixels = 0;
          slot = i;
        }

      kitty_store[slot].id = id;
      kitty_store[slot].w = w;
      kitty_store[slot].h = h;
      kitty_store[slot].pixels =
          (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
      memcpy(kitty_store[slot].pixels, px, (size_t)w * h * sizeof(uint32_t));
    }

    if (action == 't') {
      free(px);
      reply("OK");
      return;
    }
  }

  if (virtual_placement && id) {
    // nothing shown: the placeholders show it (kitty_virtual_image)
    free(px);
    kitty_virtual *v = 0;

    for (auto &vp : kitty_virtuals)
      if (vp.id == id && vp.placement == placement)
        v = &vp;

    if (!v)
      for (auto &vp : kitty_virtuals)
        if (!vp.id) {
          v = &vp;
          break;
        }

    if (!v) {
      // all taken: the first goes
      kitty_virtual_free(kitty_virtuals[0].id);
      v = &kitty_virtuals[0];
      for (auto &vp : kitty_virtuals)
        if (!vp.id) {
          v = &vp;
          break;
        }
    }

    if (v->fitted) {
      if (v->fitted->pixmap)
        XFreePixmap(dpy, v->fitted->pixmap);
      free(v->fitted->pixels);
      delete v->fitted;
    }

    v->id = id;
    v->placement = placement;
    // the image's natural size in cells, unless given
    v->cols = clamp(cols ? cols : (w + fwidth - 1) / fwidth, 1, IMAGE_TILE_MAX);
    v->rows =
        clamp(rows ? rows : (h + fheight - 1) / fheight, 1, IMAGE_TILE_MAX);
    v->fitted = 0;
    scr_touch(true); // placeholders already on the screen show it now
    reply("OK");
    return;
  }

  // show it at the cursor, c x r cells (stretched, as kitty does)
  min_it(cols, IMAGE_TILE_MAX);
  min_it(rows, IMAGE_TILE_MAX);
  px = fit_image(px, w, h, cols, rows, fwidth, fheight, false);
  place_image(px, w, h, cols, rows,
              no_move ? IMAGE_CURSOR_STAY : IMAGE_CURSOR_AFTER, id);
  reply("OK");
}

/*----------------------------------------------------------------------*/
/*
 * Unicode placeholders (kitty graphics protocol): a program creates a
 * virtual placement of an image (a=p,U=1 or a=T,U=1, with c= columns and
 * r= rows), then prints U+10EEEE where the picture goes, as text, so that
 * tmux, vim and the like move it around like text. Its foreground colour
 * is the image id (the 256-colour index, or the 24-bit RGB value), the
 * first diacritic the row in the picture, the second the column, a third
 * the id's top byte. Missing ones come from the cell to the left. The
 * picture is fitted into the placement's cells, keeping its aspect ratio.
 */

// the diacritics, in the order of the numbers they stand for
// (kitty's gen/rowcolumn-diacritics.txt: class 230 combining marks)
static const uint32_t kitty_diacritics[] = {
    0x0305,  0x030d,  0x030e,  0x0310,  0x0312,  0x033d,  0x033e,  0x033f,
    0x0346,  0x034a,  0x034b,  0x034c,  0x0350,  0x0351,  0x0352,  0x0357,
    0x035b,  0x0363,  0x0364,  0x0365,  0x0366,  0x0367,  0x0368,  0x0369,
    0x036a,  0x036b,  0x036c,  0x036d,  0x036e,  0x036f,  0x0483,  0x0484,
    0x0485,  0x0486,  0x0487,  0x0592,  0x0593,  0x0594,  0x0595,  0x0597,
    0x0598,  0x0599,  0x059c,  0x059d,  0x059e,  0x059f,  0x05a0,  0x05a1,
    0x05a8,  0x05a9,  0x05ab,  0x05ac,  0x05af,  0x05c4,  0x0610,  0x0611,
    0x0612,  0x0613,  0x0614,  0x0615,  0x0616,  0x0617,  0x0657,  0x0658,
    0x0659,  0x065a,  0x065b,  0x065d,  0x065e,  0x06d6,  0x06d7,  0x06d8,
    0x06d9,  0x06da,  0x06db,  0x06dc,  0x06df,  0x06e0,  0x06e1,  0x06e2,
    0x06e4,  0x06e7,  0x06e8,  0x06eb,  0x06ec,  0x0730,  0x0732,  0x0733,
    0x0735,  0x0736,  0x073a,  0x073d,  0x073f,  0x0740,  0x0741,  0x0743,
    0x0745,  0x0747,  0x0749,  0x074a,  0x07eb,  0x07ec,  0x07ed,  0x07ee,
    0x07ef,  0x07f0,  0x07f1,  0x07f3,  0x0816,  0x0817,  0x0818,  0x0819,
    0x081b,  0x081c,  0x081d,  0x081e,  0x081f,  0x0820,  0x0821,  0x0822,
    0x0823,  0x0825,  0x0826,  0x0827,  0x0829,  0x082a,  0x082b,  0x082c,
    0x082d,  0x0951,  0x0953,  0x0954,  0x0f82,  0x0f83,  0x0f86,  0x0f87,
    0x135d,  0x135e,  0x135f,  0x17dd,  0x193a,  0x1a17,  0x1a75,  0x1a76,
    0x1a77,  0x1a78,  0x1a79,  0x1a7a,  0x1a7b,  0x1a7c,  0x1b6b,  0x1b6d,
    0x1b6e,  0x1b6f,  0x1b70,  0x1b71,  0x1b72,  0x1b73,  0x1cd0,  0x1cd1,
    0x1cd2,  0x1cda,  0x1cdb,  0x1ce0,  0x1dc0,  0x1dc1,  0x1dc3,  0x1dc4,
    0x1dc5,  0x1dc6,  0x1dc7,  0x1dc8,  0x1dc9,  0x1dcb,  0x1dcc,  0x1dd1,
    0x1dd2,  0x1dd3,  0x1dd4,  0x1dd5,  0x1dd6,  0x1dd7,  0x1dd8,  0x1dd9,
    0x1dda,  0x1ddb,  0x1ddc,  0x1ddd,  0x1dde,  0x1ddf,  0x1de0,  0x1de1,
    0x1de2,  0x1de3,  0x1de4,  0x1de5,  0x1de6,  0x1dfe,  0x20d0,  0x20d1,
    0x20d4,  0x20d5,  0x20d6,  0x20d7,  0x20db,  0x20dc,  0x20e1,  0x20e7,
    0x20e9,  0x20f0,  0x2cef,  0x2cf0,  0x2cf1,  0x2de0,  0x2de1,  0x2de2,
    0x2de3,  0x2de4,  0x2de5,  0x2de6,  0x2de7,  0x2de8,  0x2de9,  0x2dea,
    0x2deb,  0x2dec,  0x2ded,  0x2dee,  0x2def,  0x2df0,  0x2df1,  0x2df2,
    0x2df3,  0x2df4,  0x2df5,  0x2df6,  0x2df7,  0x2df8,  0x2df9,  0x2dfa,
    0x2dfb,  0x2dfc,  0x2dfd,  0x2dfe,  0x2dff,  0xa66f,  0xa67c,  0xa67d,
    0xa6f0,  0xa6f1,  0xa8e0,  0xa8e1,  0xa8e2,  0xa8e3,  0xa8e4,  0xa8e5,
    0xa8e6,  0xa8e7,  0xa8e8,  0xa8e9,  0xa8ea,  0xa8eb,  0xa8ec,  0xa8ed,
    0xa8ee,  0xa8ef,  0xa8f0,  0xa8f1,  0xaab0,  0xaab2,  0xaab3,  0xaab7,
    0xaab8,  0xaabe,  0xaabf,  0xaac1,  0xfe20,  0xfe21,  0xfe22,  0xfe23,
    0xfe24,  0xfe25,  0xfe26,  0x10a0f, 0x10a38, 0x1d185, 0x1d186, 0x1d187,
    0x1d188, 0x1d189, 0x1d1aa, 0x1d1ab, 0x1d1ac, 0x1d1ad, 0x1d242, 0x1d243,
    0x1d244,
};

// the number a diacritic stands for, or -1
static int kitty_diacritic(uint32_t c) {
  int lo = 0, hi = ecb_array_length(kitty_diacritics);

  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (c < kitty_diacritics[mid])
      hi = mid;
    else if (c > kitty_diacritics[mid])
      lo = mid + 1;
    else
      return mid;
  }

  return -1;
}

void rxvt_term::kitty_virtual_free(unsigned int id) {
  for (auto &v : kitty_virtuals)
    if (v.id && (!id || v.id == id)) {
      if (v.fitted) {
        if (v.fitted->pixmap)
          XFreePixmap(dpy, v.fitted->pixmap);
        free(v.fitted->pixels);
        delete v.fitted;
      }

      v.id = 0;
      v.fitted = 0;
    }
}

/*
 * The picture of image id fitted into its virtual placement's cells (of
 * the current size), made from the stored image when needed; 0 if there
 * is none.
 */
term_image *rxvt_term::kitty_virtual_image(unsigned int id) {
  kitty_virtual *v = 0;

  for (auto &vp : kitty_virtuals)
    if (vp.id == id) {
      v = &vp;
      break;
    }

  if (!v)
    return 0;

  if (v->fitted && v->fitted->cell_w == fwidth && v->fitted->cell_h == fheight)
    return v->fitted;

  kitty_stored *stored = 0;
  for (auto &s : kitty_store)
    if (s.pixels && s.id == id)
      stored = &s;

  if (!stored)
    return v->fitted; // the data is gone: the old size will do

  int box_w = min(v->cols * fwidth, SIXEL_MAX_SIZE);
  int box_h = min(v->rows * fheight, SIXEL_MAX_SIZE);

  // fitted inside the box, keeping the aspect ratio, centred
  int w = stored->w, h = stored->h;
  uint32_t *px = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
  memcpy(px, stored->pixels, (size_t)w * h * sizeof(uint32_t));

  int fit_w = box_w, fit_h = (int)((long long)h * box_w / w);
  if (fit_h > box_h) {
    fit_h = box_h;
    fit_w = max(1, (int)((long long)w * box_h / h));
  }
  max_it(fit_h, 1);

  if (fit_w != w || fit_h != h) {
    uint32_t *scaled = scale_image(px, w, h, fit_w, fit_h);
    free(px);
    px = scaled;
  }

  uint32_t *box = (uint32_t *)calloc((size_t)box_w * box_h,
                                     sizeof(uint32_t)); // transparent
  int ox = (box_w - fit_w) / 2, oy = (box_h - fit_h) / 2;

  for (int y = 0; y < fit_h; y++)
    memcpy(box + (size_t)(oy + y) * box_w + ox, px + (size_t)y * fit_w,
           fit_w * sizeof(uint32_t));

  free(px);

  if (v->fitted) {
    if (v->fitted->pixmap)
      XFreePixmap(dpy, v->fitted->pixmap);
    free(v->fitted->pixels);
    delete v->fitted;
  }

  term_image *img = new term_image;
  img->kitty_id = id;
  img->serial = ++image_serial;
  img->pixmap = 0;
  img->pixels = box;
  img->w = box_w;
  img->h = box_h;
  img->cell_w = fwidth;
  img->cell_h = fheight;

  return v->fitted = img;
}

/*
 * Draw the placeholder cells of a run (called by the built-in font): x,
 * y is its first cell, in the window, width its cells. The cells are read
 * from the screen line, as their meaning depends on the cells to the left.
 */
void rxvt_term::draw_kitty_placeholder(Drawable d, GC gc, int x, int y,
                                       int width) {
  int first = x / fwidth, vrow = y / fheight;

  if (first < 0 || first >= ncol || vrow < 0 || vrow >= nrow)
    return;

  line_t &l = ROW(view_start + vrow);
  if (!l.valid() || !l.t)
    return;

  // from the first placeholder of this stretch, so that missing
  // diacritics can be inferred from the cell to the left
  int start = first;
  while (start > 0 && is_kitty_placeholder(l.t[start - 1]))
    start--;

  bool have_prev = false;
  int prev_row = 0, prev_col = 0, prev_msb = 0, prev_fg = -1;

  for (int col = start; col < first + width && col < ncol; col++) {
    text_t t = l.t[col];
    if (!is_kitty_placeholder(t)) {
      have_prev = false;
      continue;
    }

    int fg = GET_BASEFG(l.r[col]);
    int nums[3], n = 0;

#if ENABLE_COMBINING
    text_t parts[8];
    int len = rxvt_composite.expand(t);
    if (len <= 8) {
      rxvt_composite.expand(t, parts);
      for (int i = 1; i < len && n < 3; i++) {
        int num = kitty_diacritic(parts[i]);
        if (num >= 0)
          nums[n++] = num;
      }
    }
#endif

    int row = n > 0 ? nums[0] : 0;
    int pcol = n > 1 ? nums[1] : 0;
    int msb = n > 2 ? nums[2] : 0;
    bool same = have_prev && fg == prev_fg;

    if (same && n == 0)
      row = prev_row, pcol = prev_col + 1, msb = prev_msb;
    else if (same && n == 1 && row == prev_row)
      pcol = prev_col + 1, msb = prev_msb;
    else if (same && n == 2 && row == prev_row && pcol == prev_col + 1)
      msb = prev_msb;

    have_prev = true;
    prev_row = row, prev_col = pcol, prev_msb = msb, prev_fg = fg;

    if (col < first)
      continue;

    // the image id: the colour
    unsigned int id;
    if (fg >= minCOLOR && fg <= maxTermCOLOR)
      id = fg - minCOLOR;
    else if (fg >= minTermCOLOR24 && fg <= maxTermCOLOR24)
      id = rgb24_color[fg - minTermCOLOR24] & 0xffffff;
    else
      continue;

    id |= (unsigned int)(msb & 0xff) << 24;

    term_image *img = kitty_virtual_image(id);
    if (!img)
      continue;

    Pixmap pm = image_pixmap(img);
    if (!pm)
      continue;

    int sx = pcol * img->cell_w, sy = row * img->cell_h;
    int cw = min(fwidth, img->w - sx), ch = min(fheight, img->h - sy);

    if (cw > 0 && ch > 0)
      XCopyArea(dpy, pm, d, gc, sx, sy, cw, ch, x + (col - first) * fwidth, y);
  }
}

#endif
