/* gfx: smooth drawing for the FM-1 screen. See gfx.h. Integers only: the chip has no FPU. */
#include "fm1.h"
#include "gfx.h"

#define TILE_ROWS 16
static uint32_t tile[SCREEN_W * TILE_ROWS];
int clip_x0, clip_y0, clip_x1, clip_y1;
static int tw; /* the tile's width */

/* ---------------------------------------------------------------- maths */

int isin(int ang) { return SIN1024[ang & 1023]; }
int icos(int ang) { return SIN1024[(ang + 256) & 1023]; }

uint32_t isqrt_u(uint32_t v) {
  uint32_t r = 0, b = 1u << 30;
  while (b > v) {
    b >>= 2;
  }
  while (b) {
    if (v >= r + b) {
      v -= r + b;
      r = (r >> 1) + b;
    } else {
      r >>= 1;
    }
    b >>= 2;
  }
  return r;
}

static int iabs(int v) { return v < 0 ? -v : v; }

static int atan_oct(int t) { /* t 0..256 (tan of 0..45 degrees, Q8) -> 0..128 */
  return (t >> 1) + ((89 * t * (256 - t)) >> 17);
}

int angle_of(int dx, int dy) {
  int x = dx, y = -dy, ax = iabs(x), ay = iabs(y), base;
  if (!ax && !ay) {
    return 0;
  }
  base = ax <= ay ? atan_oct((ax << 8) / ay) : 256 - atan_oct((ay << 8) / ax);
  if (x >= 0) {
    return y >= 0 ? base : 512 - base;
  }
  return (y < 0 ? 512 + base : 1024 - base) & 1023;
}

uint32_t mix(uint32_t a, uint32_t b, int t) {
  int r = (int)(a >> 16 & 255), g = (int)(a >> 8 & 255), bl = (int)(a & 255);
  r += (((int)(b >> 16 & 255) - r) * t) >> 8;
  g += (((int)(b >> 8 & 255) - g) * t) >> 8;
  bl += (((int)(b & 255) - bl) * t) >> 8;
  return (uint32_t)(r << 16 | g << 8 | bl);
}

static inline void blend(uint32_t *p, uint32_t c, int a) {
  if (a >= 256) {
    *p = c;
  } else if (a > 0) {
    *p = mix(*p, c, a);
  }
}
#define AT(x, y) (&tile[((y) - clip_y0) * tw + ((x) - clip_x0)])

/* the pixel span [x0, x1) x [y0, y1) inside the clip; 0 if nothing is */
static int span(int *x0, int *y0, int *x1, int *y1) {
  if (*x0 < clip_x0) *x0 = clip_x0;
  if (*y0 < clip_y0) *y0 = clip_y0;
  if (*x1 > clip_x1) *x1 = clip_x1;
  if (*y1 > clip_y1) *y1 = clip_y1;
  return *x0 < *x1 && *y0 < *y1;
}

/* coverage (0..256) of a pixel whose centre is sqrt(d2) from a circle's centre, radius r16 (all Q4):
 * a linear ramp one pixel wide across the edge. inv = (8 << 16) / r16 */
static inline int cover(int d2, int r16, int inv) {
  int in = r16 - 8, out = r16 + 8;
  if (d2 >= out * out) {
    return 0;
  }
  if (in > 0 && d2 <= in * in) {
    return 256;
  }
  int v = (int)((((int64_t)(r16 * r16 - d2)) * inv) >> 16) + 128;
  return v < 0 ? 0 : v > 256 ? 256
                             : v;
}

/* ---------------------------------------------------------------- the queue of rectangles to repaint */

#define QMAX 24
static struct {
  int16_t x0, y0, x1, y1;
} q[QMAX];
static int nq;

void gfx_dirty(int x0, int y0, int x1, int y1) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > SCREEN_W) x1 = SCREEN_W;
  if (y1 > SCREEN_H) y1 = SCREEN_H;
  if (x0 >= x1 || y0 >= y1) {
    return;
  }
  for (int i = 0; i < nq; i++) { /* inside one already queued, or overlapping it: grow that one */
    if (x0 <= q[i].x1 + 4 && x1 >= q[i].x0 - 4 && y0 <= q[i].y1 + 4 && y1 >= q[i].y0 - 4) {
      int ux0 = x0 < q[i].x0 ? x0 : q[i].x0, uy0 = y0 < q[i].y0 ? y0 : q[i].y0;
      int ux1 = x1 > q[i].x1 ? x1 : q[i].x1, uy1 = y1 > q[i].y1 ? y1 : q[i].y1;
      /* merge only when it does not waste much */
      int area = (ux1 - ux0) * (uy1 - uy0), parts = (x1 - x0) * (y1 - y0) + (q[i].x1 - q[i].x0) * (q[i].y1 - q[i].y0);
      if (area <= parts + parts / 2 || nq == QMAX) {
        q[i].x0 = (int16_t)ux0, q[i].y0 = (int16_t)uy0, q[i].x1 = (int16_t)ux1, q[i].y1 = (int16_t)uy1;
        return;
      }
    }
  }
  if (nq == QMAX) { /* full: fold everything into the last one */
    gfx_dirty_all();
    return;
  }
  q[nq].x0 = (int16_t)x0, q[nq].y0 = (int16_t)y0, q[nq].x1 = (int16_t)x1, q[nq].y1 = (int16_t)y1;
  nq++;
}

void gfx_dirty_all(void) {
  nq = 1;
  q[0].x0 = 0, q[0].y0 = 0, q[0].x1 = SCREEN_W, q[0].y1 = SCREEN_H;
}

/* the tile to the screen, dithered from 8 bits a channel to RGB565 */
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static void flush_tile(void) {
  for (int y = clip_y0; y < clip_y1; y++) {
    const uint32_t *p = AT(clip_x0, y);
    for (int x = clip_x0; x < clip_x1; x++, p++) {
      int d = BAYER[y & 3][x & 3];
      int r = (int)(*p >> 16 & 255) + (d >> 1), g = (int)(*p >> 8 & 255) + (d >> 2), b = (int)(*p & 255) + (d >> 1);
      r = r > 255 ? 255 : r, g = g > 255 ? 255 : g, b = b > 255 ? 255 : b;
      pixel(x, y, (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)));
    }
  }
}

/* what the FM-1 would send: per call, every row between the highest and lowest one drawn */
uint32_t gfx_rows_sent, gfx_px_drawn;

int gfx_render(scene_fn scene, int budget) {
  /* The FM-1 sends every row between the highest and lowest one drawn in a pass, at ~0.3 ms a row. So a pass
   * starts with the smallest rectangle waiting (what moves: a spark, a ripple) and takes only the others that
   * sit within those rows; the rest wait for the next pass. */
  int py0 = SCREEN_H, py1 = 0;
  while (nq && budget > 0) {
    int pick = -1;
    for (int i = 0; i < nq; i++) {
      int ok = py1 <= py0 || (q[i].y0 < py1 + 4 && q[i].y1 > py0 - 4); /* first pick, or within the pass's rows */
      if (ok && (pick < 0 || (q[i].x1 - q[i].x0) * (q[i].y1 - q[i].y0) < (q[pick].x1 - q[pick].x0) * (q[pick].y1 - q[pick].y0))) {
        pick = i;
      }
    }
    if (pick < 0) {
      break;
    }
    int w = q[pick].x1 - q[pick].x0, rows = budget / w;
    rows = rows < 1 ? 1 : rows > TILE_ROWS ? TILE_ROWS
                                           : rows;
    if (rows > q[pick].y1 - q[pick].y0) {
      rows = q[pick].y1 - q[pick].y0;
    }
    clip_x0 = q[pick].x0, clip_x1 = q[pick].x1, clip_y0 = q[pick].y0, clip_y1 = q[pick].y0 + rows;
    tw = w;
    py0 = clip_y0 < py0 ? clip_y0 : py0, py1 = clip_y1 > py1 ? clip_y1 : py1;
    gfx_px_drawn += (uint32_t)(w * rows);
    scene();
    flush_tile();
    budget -= w * rows;
    q[pick].y0 = (int16_t)(q[pick].y0 + rows);
    if (q[pick].y0 >= q[pick].y1) {
      for (int i = pick + 1; i < nq; i++) {
        q[i - 1] = q[i];
      }
      nq--;
    }
  }
  if (py1 > py0) {
    gfx_rows_sent += (uint32_t)(py1 - py0);
  }
  return nq > 0;
}

/* ---------------------------------------------------------------- painting */

#define BG_TOP 0x1c2058u
#define BG_BOTTOM 0x0d0f2eu
#define BG_GLOW 0x343a8cu
#define GLOW_CX 120
#define GLOW_CY 123
#define GLOW_R 118

void gfx_background(void) {
  static const int inv = (int)((256u << 16) / (GLOW_R * GLOW_R));
  for (int y = clip_y0; y < clip_y1; y++) {
    uint32_t row = mix(BG_TOP, BG_BOTTOM, y * 256 / SCREEN_H), *p = AT(clip_x0, y);
    int dy = y - GLOW_CY;
    for (int x = clip_x0; x < clip_x1; x++, p++) {
      int dx = x - GLOW_CX, d2 = dx * dx + dy * dy;
      if (d2 < GLOW_R * GLOW_R) {
        int t = 256 - ((d2 * inv) >> 16);
        *p = mix(row, BG_GLOW, (t * t) >> 9);
      } else {
        *p = row;
      }
    }
  }
}

void gfx_fill(int x0, int y0, int x1, int y1, uint32_t c, int a) {
  if (!span(&x0, &y0, &x1, &y1)) {
    return;
  }
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      blend(AT(x, y), c, a);
    }
  }
}

void gfx_disc(int cx16, int cy16, int r16, uint32_t c, int a) {
  int x0 = (cx16 - r16 - 16) >> 4, y0 = (cy16 - r16 - 16) >> 4, x1 = ((cx16 + r16 + 16) >> 4) + 1, y1 = ((cy16 + r16 + 16) >> 4) + 1;
  if (r16 <= 0 || !span(&x0, &y0, &x1, &y1)) {
    return;
  }
  int inv = (8 << 16) / r16;
  for (int y = y0; y < y1; y++) {
    int dy = y * 16 + 8 - cy16;
    for (int x = x0; x < x1; x++) {
      int dx = x * 16 + 8 - cx16, cv = cover(dx * dx + dy * dy, r16, inv);
      if (cv) {
        blend(AT(x, y), c, (cv * a) >> 8);
      }
    }
  }
}

void gfx_ring(int cx16, int cy16, int r16, int w16, uint32_t c, int a) {
  gfx_arc(cx16, cy16, r16, w16, 0, 1024, c, a);
}

void gfx_arc(int cx16, int cy16, int r16, int w16, int a0, int a1, uint32_t c, int a) {
  int ro = r16 + w16 / 2, ri = r16 - w16 / 2, span_a = a1 - a0;
  int x0 = (cx16 - ro - 16) >> 4, y0 = (cy16 - ro - 16) >> 4, x1 = ((cx16 + ro + 16) >> 4) + 1, y1 = ((cy16 + ro + 16) >> 4) + 1;
  if (span_a <= 0 || !span(&x0, &y0, &x1, &y1)) {
    return;
  }
  int invo = (8 << 16) / ro, invi = ri > 0 ? (8 << 16) / ri : 0;
  int k = (r16 * 201) >> 7; /* coverage per angle unit along the arc, Q4 radius -> 1/256 px per unit, x16 */
  for (int y = y0; y < y1; y++) {
    int dy = y * 16 + 8 - cy16;
    for (int x = x0; x < x1; x++) {
      int dx = x * 16 + 8 - cx16, d2 = dx * dx + dy * dy;
      int cv = cover(d2, ro, invo) - (ri > 0 ? cover(d2, ri, invi) : 0);
      if (cv <= 0) {
        continue;
      }
      if (span_a < 1024) {
        int rel = (angle_of(dx, dy) - a0) & 1023, dist, ac;
        if (rel <= span_a) {
          dist = rel < span_a - rel ? rel : span_a - rel;
          ac = 128 + ((dist * k) >> 4);
        } else {
          dist = rel - span_a < 1024 - rel ? rel - span_a : 1024 - rel;
          ac = 128 - ((dist * k) >> 4);
        }
        ac = ac < 0 ? 0 : ac > 256 ? 256
                                   : ac;
        cv = (cv * ac) >> 8;
      }
      blend(AT(x, y), c, (cv * a) >> 8);
    }
  }
}

static int fade_cx, fade_cy, fade_r0, fade_r1;
void gfx_line_fade(int cx, int cy, int r0, int r1) { fade_cx = cx, fade_cy = cy, fade_r0 = r0, fade_r1 = r1; }

void gfx_line(int x0_16, int y0_16, int x1_16, int y1_16, int w16, uint32_t c, int a) {
  int r16 = w16 / 2, pad = r16 + 16;
  int x0 = ((x0_16 < x1_16 ? x0_16 : x1_16) - pad) >> 4, x1 = (((x0_16 > x1_16 ? x0_16 : x1_16) + pad) >> 4) + 1;
  int y0 = ((y0_16 < y1_16 ? y0_16 : y1_16) - pad) >> 4, y1 = (((y0_16 > y1_16 ? y0_16 : y1_16) + pad) >> 4) + 1;
  if (r16 <= 0 || !span(&x0, &y0, &x1, &y1)) {
    return;
  }
  int ex = x1_16 - x0_16, ey = y1_16 - y0_16, len2 = ex * ex + ey * ey, len = (int)isqrt_u((uint32_t)len2);
  if (!len) {
    gfx_disc(x0_16, y0_16, r16, c, a);
    return;
  }
  int64_t invlen = (1ll << 24) / len;
  int inv = (8 << 16) / r16;
  for (int y = y0; y < y1; y++) {
    int py = y * 16 + 8 - y0_16;
    for (int x = x0; x < x1; x++) {
      int px = x * 16 + 8 - x0_16, dot = px * ex + py * ey, d2;
      if (dot <= 0) {
        d2 = px * px + py * py;
      } else if (dot >= len2) {
        int qx = px - ex, qy = py - ey;
        d2 = qx * qx + qy * qy;
      } else {
        int cr = px * ey - py * ex;
        int d = (int)(((int64_t)(cr < 0 ? -cr : cr) * invlen) >> 24);
        d2 = d * d;
      }
      int cv = cover(d2, r16, inv);
      if (cv && fade_r1) { /* fade out toward a centre: 0 inside r0, full beyond r1 */
        int fx = x - fade_cx, fy = y - fade_cy, f2 = fx * fx + fy * fy;
        cv = f2 <= fade_r0 * fade_r0 ? 0 : f2 >= fade_r1 * fade_r1 ? cv : (cv * (f2 - fade_r0 * fade_r0)) / (fade_r1 * fade_r1 - fade_r0 * fade_r0);
      }
      if (cv) {
        blend(AT(x, y), c, (cv * a) >> 8);
      }
    }
  }
}

void gfx_glow(int cx, int cy, int r, uint32_t c, int a) {
  int x0 = cx - r, y0 = cy - r, x1 = cx + r + 1, y1 = cy + r + 1;
  if (r <= 0 || a <= 0 || !span(&x0, &y0, &x1, &y1)) {
    return;
  }
  int inv = (int)((256u << 16) / (uint32_t)(r * r));
  for (int y = y0; y < y1; y++) {
    int dy = y - cy;
    for (int x = x0; x < x1; x++) {
      int dx = x - cx, d2 = dx * dx + dy * dy;
      if (d2 < r * r) {
        int t = 256 - ((d2 * inv) >> 16);
        blend(AT(x, y), c, (((t * t) >> 8) * a) >> 8);
      }
    }
  }
}

static void glyph(const glyph_t *g, const uint8_t *data, int x, int y, uint32_t c, int a) {
  int gx = x + g->x, gy = y + g->y, x0 = gx, y0 = gy, x1 = gx + g->w, y1 = gy + g->h;
  if (!span(&x0, &y0, &x1, &y1)) {
    return;
  }
  int stride = (g->w + 1) / 2;
  const uint8_t *d = data + g->off;
  for (int yy = y0; yy < y1; yy++) {
    const uint8_t *row = d + (yy - gy) * stride;
    for (int xx = x0; xx < x1; xx++) {
      int i = xx - gx, v = i & 1 ? row[i >> 1] & 15 : row[i >> 1] >> 4;
      if (v) {
        blend(AT(xx, yy), c, (v * 17 * a) >> 8);
      }
    }
  }
}

void gfx_image(const glyph_t *g, const uint8_t *data, int x, int y, uint32_t c, int a) { glyph(g, data, x, y, c, a); }

int gfx_text_width(const font_t *f, const char *s) {
  int w = 0;
  for (; *s; s++) {
    uint8_t i = (uint8_t)*s < 128 ? f->index[(uint8_t)*s] : 255;
    if (i != 255) {
      w += f->g[i].adv;
    }
  }
  return w;
}

int gfx_text(const font_t *f, int x, int y, const char *s, uint32_t c, int a) {
  if (y - f->ascent - 2 >= clip_y1 || y + f->descent + 2 <= clip_y0) {
    for (; *s; s++) { /* off this tile: only the width */
      uint8_t i = (uint8_t)*s < 128 ? f->index[(uint8_t)*s] : 255;
      x += i != 255 ? f->g[i].adv : 0;
    }
    return x;
  }
  for (; *s; s++) {
    uint8_t i = (uint8_t)*s < 128 ? f->index[(uint8_t)*s] : 255;
    if (i == 255) {
      continue;
    }
    glyph(&f->g[i], f->data, x, y, c, a);
    x += f->g[i].adv;
  }
  return x;
}

int gfx_text_c(const font_t *f, int cx, int y, const char *s, uint32_t c, int a) {
  return gfx_text(f, cx - gfx_text_width(f, s) / 2, y, s, c, a);
}

void gfx_splash(void) {
  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      pixel(x, y, SPLASH_PAL[SPLASH[y * SCREEN_W + x]]);
    }
  }
}
