/* gfx: smooth (anti-aliased) drawing for the FM-1's 240 x 240 screen, integers only.
 *
 * The screen is drawn in tiles: gfx_render() calls your scene function once per tile with the clip set, and the
 * scene paints everything that might touch it (each call skips itself when outside). Colours are 0xRRGGBB, mixed
 * at 8 bits per channel, and dithered down to the screen's RGB565 at the end, so gradients and glows stay smooth. */
#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "assets.h"

typedef void (*scene_fn)(void);

/* the dirty-rectangle queue: mark what changed; gfx_render() repaints up to `budget` pixels a call */
void gfx_dirty(int x0, int y0, int x1, int y1);
void gfx_dirty_all(void);
int gfx_render(scene_fn scene, int budget); /* returns 1 while there is more to draw */

/* the current tile (for scenes that want to skip work) */
extern int clip_x0, clip_y0, clip_x1, clip_y1;
static inline int gfx_touches(int x0, int y0, int x1, int y1) {
  return x1 > clip_x0 && x0 < clip_x1 && y1 > clip_y0 && y0 < clip_y1;
}

/* painting: a = opacity 0..256. Coordinates in Q4 (1/16 px) where noted */
void gfx_background(void);                                                      /* the lapis gradient and the wheel's glow */
void gfx_fill(int x0, int y0, int x1, int y1, uint32_t c, int a);                /* px */
void gfx_disc(int cx16, int cy16, int r16, uint32_t c, int a);                   /* Q4 */
void gfx_ring(int cx16, int cy16, int r16, int w16, uint32_t c, int a);          /* Q4: centre radius, width */
void gfx_arc(int cx16, int cy16, int r16, int w16, int a0, int a1, uint32_t c, int a); /* angles 0..1023, 0 = up, clockwise; a1 > a0 */
void gfx_line(int x0_16, int y0_16, int x1_16, int y1_16, int w16, uint32_t c, int a);  /* Q4, round caps */
void gfx_line_fade(int cx, int cy, int r0, int r1);                             /* lines fade out inside r0..r1 of (cx, cy); r1 = 0: off */
void gfx_glow(int cx, int cy, int r, uint32_t c, int a);                         /* px: a soft light, a at the centre */
int gfx_text(const font_t *f, int x, int y, const char *s, uint32_t c, int a);   /* px, y = baseline; returns the end x */
int gfx_text_width(const font_t *f, const char *s);
int gfx_text_c(const font_t *f, int cx, int y, const char *s, uint32_t c, int a); /* centred on cx */
void gfx_image(const glyph_t *g, const uint8_t *data, int x, int y, uint32_t c, int a); /* a 4-bit image at baseline x, y */
void gfx_splash(void);                                                          /* the start screen, straight to the screen */

uint32_t mix(uint32_t a, uint32_t b, int t); /* t 0..256: a -> b */
int isin(int ang);                           /* 0..1023 -> Q14 */
int icos(int ang);
int angle_of(int dx, int dy);                /* 0..1023, 0 = up, clockwise */
uint32_t isqrt_u(uint32_t v);
#endif
