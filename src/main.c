/* BE HERE NOW: a sarod-inspired string instrument that teaches you what you play.
 * Named after Ram Dass's book; love, serve, remember.
 *
 *   keys        SAROD mode: one string. Press a key while holding another = MEEND (a slide), no new pluck.
 *               CHORD mode: four strings, one per key, and the screen names the chord.
 *   SEL         switch SAROD / CHORD
 *   PLAY        the tanpura drone on / off (Pa, Sa, Sa, low Sa, round and round)
 *   PRESETS     the thaat (the parent scale): the sympathetic strings retune to it
 *   ALGORITHM   Sa, the home note (C .. B)
 *   OCT- OCT+   the octave
 *   KNOB1       TONE     bright / dark pluck
 *   KNOB2       TARAF    how loud the sympathetic strings ring
 *   KNOB3       SUSTAIN  how long a string rings
 *   KNOB4       MEEND    how slow the slides are (0: no slides, every key plucks)
 *   key lights  the notes of the thaat
 *
 * The sound: Karplus-Strong strings (a burst of noise in a delay line, filtered a little on every pass).
 * The main string feeds 11 more strings tuned to the thaat: they only ring when you play a note (or a
 * harmonic) they share, as a sarod's taraf strings do.
 *
 * Integers only (the FM-1 has no FPU). Built on foma by konsumer (GPL-3.0). */
#include "fm1.h"
#include "period.h"

/* ================================================================ music knowledge */

/* the 10 thaats of Hindustani music (Bhatkhande), as semitones above Sa, and their Western cousins */
typedef struct {
  const char *name, *western;
  uint16_t mask; /* bit n: n semitones above Sa is in the scale */
} thaat_t;
#define M(a, b, c, d, e, f, g) (uint16_t)(1u << a | 1u << b | 1u << c | 1u << d | 1u << e | 1u << f | 1u << g)
static const thaat_t THAAT[] = {
  {"BILAWAL", "MAJOR (IONIAN)", M(0, 2, 4, 5, 7, 9, 11)},
  {"KALYAN", "LYDIAN", M(0, 2, 4, 6, 7, 9, 11)},
  {"KHAMAJ", "MIXOLYDIAN", M(0, 2, 4, 5, 7, 9, 10)},
  {"KAFI", "DORIAN", M(0, 2, 3, 5, 7, 9, 10)},
  {"ASAVARI", "NATURAL MINOR", M(0, 2, 3, 5, 7, 8, 10)},
  {"BHAIRAVI", "PHRYGIAN", M(0, 1, 3, 5, 7, 8, 10)},
  {"BHAIRAV", "DOUBLE HARMONIC", M(0, 1, 4, 5, 7, 8, 11)},
  {"PURVI", "NO WESTERN NAME", M(0, 1, 4, 6, 7, 8, 11)},
  {"MARWA", "NO WESTERN NAME", M(0, 1, 4, 6, 7, 9, 11)},
  {"TODI", "NO WESTERN NAME", M(0, 1, 3, 6, 7, 8, 11)},
};
#define THAATS 10
static const char *const RAGA_EG[THAATS] = {"eg Alhaiya Bilawal", "eg Yaman", "eg Khamaj", "eg Kafi, Bageshri",
                                            "eg Darbari", "eg Bhairavi", "eg Ahir Bhairav",
                                            "eg Puriya Dhanashri", "eg Marwa", "eg Miyan ki Todi"};

static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const SARGAM[12] = {"Sa", "komal Re", "Re", "komal Ga", "Ga", "Ma",
                                       "tivra Ma", "Pa", "komal Dha", "Dha", "komal Ni", "Ni"};
static const char *const SHORT[12] = {"S", "r", "R", "g", "G", "m", "M", "P", "d", "D", "n", "N"};
static const char *const INTERVAL[12] = {"unison", "minor 2nd", "major 2nd", "minor 3rd", "major 3rd", "perfect 4th",
                                         "tritone", "perfect 5th", "minor 6th", "major 6th", "minor 7th", "major 7th"};

/* chords, as semitone masks above the root */
typedef struct {
  const char *name;
  uint16_t mask;
  const char *what;
} chord_t;
#define C3(a, b, c) (uint16_t)(1u << a | 1u << b | 1u << c)
#define C4(a, b, c, d) (uint16_t)(1u << a | 1u << b | 1u << c | 1u << d)
static const chord_t CHORD[] = {
  {"MAJ", C3(0, 4, 7), "major triad: root, M3, P5"},
  {"MIN", C3(0, 3, 7), "minor triad: root, m3, P5"},
  {"DIM", C3(0, 3, 6), "diminished: two minor 3rds"},
  {"AUG", C3(0, 4, 8), "augmented: two major 3rds"},
  {"SUS2", C3(0, 2, 7), "suspended: 2nd for the 3rd"},
  {"SUS4", C3(0, 5, 7), "suspended: 4th for the 3rd"},
  {"7", C4(0, 4, 7, 10), "dominant 7th: maj + m7"},
  {"MAJ7", C4(0, 4, 7, 11), "major 7th: maj + M7"},
  {"MIN7", C4(0, 3, 7, 10), "minor 7th: min + m7"},
  {"M7B5", C4(0, 3, 6, 10), "half-diminished: dim + m7"},
  {"DIM7", C4(0, 3, 6, 9), "diminished 7th: all m3s"},
  {"6", C4(0, 4, 7, 9), "major 6th: maj + M6"},
  {"MIN6", C4(0, 3, 7, 9), "minor 6th: min + M6"},
  {"POWER", (uint16_t)(1u << 0 | 1u << 7), "power chord: root + P5"},
};
#define CHORDS 14

/* ================================================================ the strings (Karplus-Strong) */

typedef struct {
  int16_t *buf;
  uint32_t mask; /* buffer length - 1 (a power of two) */
  uint32_t w;    /* write position */
  uint32_t d;    /* delay, samples, Q16 */
  int32_t lp;    /* the loop filter's state */
  int32_t a;     /* loop filter, Q14: 16384 = bright (no filtering), lower = darker */
  int32_t fb;    /* feedback, Q15: how much survives each pass (the sustain) */
} string_t;

static int16_t main_buf[2048] POOL;
static int16_t chord_buf[4][2048] POOL;
static int16_t drone_buf[4][1024] POOL;
static int16_t taraf_buf[11][256] POOL;

static string_t lead, chord[4], drone[4], taraf[11];

static int32_t clamp16(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767
                                                                           : v; }

/* one sample of a string: read the delay line, filter it, feed it back with `in` added */
static inline int32_t string_step(string_t *s, int32_t in) {
  uint32_t i = s->d >> 16, f = (s->d & 0xFFFFu) >> 2; /* fraction, Q14 */
  int32_t s0 = s->buf[(s->w - i) & s->mask], s1 = s->buf[(s->w - i - 1u) & s->mask];
  int32_t y = s0 + (((s1 - s0) * (int32_t)f) >> 14);
  s->lp += ((y - s->lp) * s->a) >> 14;
  s->buf[s->w] = (int16_t)clamp16(((s->lp * s->fb) >> 15) + in);
  s->w = (s->w + 1u) & s->mask;
  return y;
}

/* the delay that plays MIDI pitch p (Q8: note * 256), less what the loop filter adds */
static uint32_t delay_for(int32_t p, int32_t a) {
  if (p < 24 * 256) {
    p = 24 * 256;
  }
  if (p > 120 * 256) {
    p = 120 * 256;
  }
  uint32_t n = (uint32_t)p >> 8, fr = (uint32_t)p & 255u;
  uint32_t per = PERIOD[n] - (((PERIOD[n] - PERIOD[n + 1]) >> 8) * fr);
  uint32_t comp = ((uint32_t)(16384 - a) << 16) / (uint32_t)a + 65536u; /* the filter's delay + the one-sample write */
  return per > comp + 2 * 65536u ? per - comp : 2 * 65536u;
}

/* pluck: add a burst of filtered noise along one period of the string. `pick` < 1: where the plectrum strikes */
static void pluck(string_t *s, int32_t level, int32_t tone) {
  uint32_t len = s->d >> 16, k;
  int32_t lpn = 0;
  if (len > s->mask) {
    len = s->mask;
  }
  for (k = 0; k < len; k++) {
    int32_t n = (int32_t)(random32() >> 17) - 16384;
    lpn += ((n - lpn) * tone) >> 14;
    uint32_t at = (s->w - 1u - k) & s->mask;
    s->buf[at] = (int16_t)clamp16(s->buf[at] / 2 + ((lpn * level) >> 13));
  }
  /* the plectrum strikes near the bridge (1/7 of the string): take that out as a comb, brighter and twangier */
  uint32_t P = len / 7u;
  for (k = len; P && k-- > P;) {
    uint32_t at = (s->w - 1u - k) & s->mask, from = (s->w - 1u - (k - P)) & s->mask;
    s->buf[at] = (int16_t)clamp16(s->buf[at] - s->buf[from] / 2);
  }
}

/* ================================================================ state shared by loop() and audio() */

enum { MODE_SAROD,
       MODE_CHORD };

static struct {
  int8_t octave;
  uint8_t thaat, sa, mode;
  uint8_t knob[4]; /* TONE, TARAF, SUSTAIN, MEEND, 0..127 */
} saved = {0, 1, 0, MODE_SAROD, {80, 70, 90, 20}};

/* loop() writes, audio() reads */
static volatile int32_t lead_target;   /* Q8 MIDI pitch */
static volatile uint32_t lead_plucks;  /* counts up: a new pluck */
static volatile int32_t lead_vel;      /* 0..127 */
static volatile uint32_t chord_note[4], chord_plucks[4], chord_vel[4];
static volatile uint8_t drone_on, retune;

/* ================================================================ the sound */

void audio(int32_t *out, uint32_t frames) {
  static uint32_t lead_seen, chord_seen[4], drone_clock, drone_next;
  static int32_t lead_pitch;
  int32_t tone_a = 4096 + saved.knob[0] * 96;                 /* loop filter */
  int32_t fb = 32760 - (127 - saved.knob[2]) * 4;             /* 32252 .. 32760 */
  int32_t taraf_level = saved.knob[1];                         /* 0..127 */
  int32_t pluck_tone = 3000 + saved.knob[0] * 105;             /* the noise burst's brightness */
  uint32_t i;
  int v;

  if (retune) { /* the thaat or Sa changed: retune the sympathetic and drone strings */
    retune = 0;
    int n = 0, sa = 60 + saved.sa;
    uint16_t mask = THAAT[saved.thaat].mask;
    for (int oct = 0; oct < 2 && n < 11; oct++) {
      for (int s = 0; s < 12 && n < 11; s++) {
        if (mask >> s & 1u) {
          taraf[n].d = delay_for((sa + 12 * oct + s) * 256, taraf[n].a);
          n++;
        }
      }
    }
    static const int8_t DRONE_NOTE[4] = {-5, 0, 0, -12}; /* Pa below Sa, Sa, Sa, Sa an octave down */
    for (v = 0; v < 4; v++) {
      drone[v].d = delay_for((48 + saved.sa + DRONE_NOTE[v]) * 256, drone[v].a);
    }
  }

  /* the lead string: a pluck, or a slide (meend) toward the target */
  lead.a = tone_a;
  lead.fb = fb;
  if (lead_plucks != lead_seen) {
    lead_seen = lead_plucks;
    lead_pitch = lead_target;
    lead.d = delay_for(lead_pitch, lead.a);
    pluck(&lead, 200 + lead_vel * 200, pluck_tone);
  } else if (lead_pitch != lead_target) {
    int32_t t = lead_target, knob = saved.knob[3];
    int32_t step = knob ? (int32_t)(frames * 1486u / (1024u * (uint32_t)knob)) : 1 << 20;
    if (step < 1) {
      step = 1;
    }
    lead_pitch = lead_pitch < t ? (lead_pitch + step > t ? t : lead_pitch + step)
                                : (lead_pitch - step < t ? t : lead_pitch - step);
  }
  lead.d = delay_for(lead_pitch, lead.a);

  for (v = 0; v < 4; v++) {
    chord[v].a = tone_a;
    chord[v].fb = fb;
    if (chord_plucks[v] != chord_seen[v]) {
      chord_seen[v] = chord_plucks[v];
      chord[v].d = delay_for((int32_t)chord_note[v] * 256, chord[v].a);
      pluck(&chord[v], 150 + (int32_t)chord_vel[v] * 150, pluck_tone);
    }
  }

  for (i = 0; i < frames; i++) {
    /* the tanpura: one string every 0.6 s */
    if (drone_on && ++drone_clock >= drone_next) {
      static uint8_t which;
      drone_clock = 0;
      drone_next = 26460;
      pluck(&drone[which], 9000, 9000);
      which = (uint8_t)((which + 1) & 3);
    }
    int32_t played = string_step(&lead, 0);
    for (v = 0; v < 4; v++) {
      played += string_step(&chord[v], 0) / 2;
    }
    int32_t dr = 0;
    for (v = 0; v < 4; v++) {
      dr += string_step(&drone[v], 0);
    }
    dr /= 2;
    /* the sympathetic strings: excited by everything else, only ringing where they agree */
    int32_t excite = (played + dr) >> 9, l = 0, r = 0;
    for (v = 0; v < 11; v++) {
      int32_t y = string_step(&taraf[v], excite);
      if (v & 1) {
        l += y;
      } else {
        r += y;
      }
    }
    l = (l * taraf_level) >> 8;
    r = (r * taraf_level) >> 8;
    int32_t mid = played + (dr * 3) / 4;
    out[2 * i] += clamp16(mid + l + (r >> 2));
    out[2 * i + 1] += clamp16(mid + r + (l >> 2));
  }
}

/* ================================================================ the screen */

#define ACCENT RGB(255, 170, 60)
#define SAFFRON RGB(255, 120, 30)
#define TEAL RGB(80, 200, 190)
#define DIM RGB(100, 100, 100)
#define FAINT RGB(45, 45, 45)

static void fill_text(int x, int y, int w, const char *s, uint16_t c, int font) {
  rect(x, y, w, text_height(font), BLACK);
  text(x, y, s, c, font);
}
static void centered(int y, const char *s, uint16_t c, int font) {
  rect(0, y, SCREEN_W, text_height(font), BLACK);
  text((SCREEN_W - text_width(s, font)) / 2, y, s, c, font);
}
static char *cat(char *d, const char *s) {
  while (*s) {
    *d++ = *s++;
  }
  *d = 0;
  return d;
}

static void draw_header(void) {
  rect(0, 0, SCREEN_W, 56, BLACK);
  text(4, 2, "BE HERE NOW", ACCENT, FONT_SMALL);
  const char *mode = saved.mode == MODE_SAROD ? "SAROD" : "CHORD";
  text(SCREEN_W - 4 - text_width(mode, FONT_SMALL), 2, mode, TEAL, FONT_SMALL);
  if (drone_on) {
    text(104, 2, "DRONE", SAFFRON, FONT_SMALL);
  }
  char s[40], *p = s;
  p = cat(p, THAAT[saved.thaat].name);
  p = cat(p, " = ");
  cat(p, THAAT[saved.thaat].western);
  centered(20, s, WHITE, FONT_SMALL);
  p = cat(s, "Sa = ");
  cat(p, NOTE[saved.sa]);
  centered(38, s, DIM, FONT_SMALL);
}

/* the 12 notes above Sa, S r R g G m M P d D n N: the thaat's bright, the others faint, the played ones lit */
static void draw_strip(uint16_t lit) {
  uint16_t mask = THAAT[saved.thaat].mask;
  for (int s = 0; s < 12; s++) {
    int x = s * 20;
    uint16_t bg = lit >> s & 1 ? ACCENT : mask >> s & 1 ? RGB(60, 60, 60)
                                                        : BLACK;
    rect(x + 1, 170, 18, 20, bg);
    uint16_t fg = lit >> s & 1 ? BLACK : mask >> s & 1 ? WHITE
                                                       : FAINT;
    text(x + 6, 172, SHORT[s], fg, FONT_SMALL);
  }
}

/* the FM-1's 27 keys: thaat notes marked, Sa in saffron, held keys lit */
static void draw_keys(uint32_t lit, int base) {
  static const uint8_t BLACK_KEY[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0}; /* from F */
  uint16_t mask = THAAT[saved.thaat].mask;
  int x = 8;
  rect(0, 116, SCREEN_W, 52, BLACK);
  for (int pass = 0; pass < 2; pass++) {
    x = 8;
    for (int k = 0; k < KEY_COUNT; k++) {
      int pc = ((base + k) % 12 - saved.sa + 24) % 12; /* semitones above Sa */
      int black = BLACK_KEY[k % 12];
      int on = lit >> k & 1;
      if (!black && pass == 0) {
        rect(x, 118, 13, 48, on ? ACCENT : RGB(210, 210, 210));
        if (!on && mask >> pc & 1) {
          rect(x + 4, 156, 5, 5, pc == 0 ? SAFFRON : TEAL);
        }
      } else if (black && pass == 1) {
        rect(x - 5, 118, 9, 28, on ? ACCENT : RGB(20, 20, 20));
        if (!on && mask >> pc & 1) {
          rect(x - 3, 137, 5, 5, pc == 0 ? SAFFRON : TEAL);
        }
      }
      if (!black) {
        x += 14;
      }
    }
  }
}

static const char *const KNOB_NAME[4] = {"TONE", "TARAF", "SUSTAIN", "MEEND"};
static const char *const KNOB_TIP[4] = {"TONE: bright or dark pluck",
                                        "TARAF: sympathetic strings",
                                        "SUSTAIN: how long it rings",
                                        "MEEND: slide time, 0 = off"};
static void draw_knob(int k) {
  char v[12];
  int x = k * 60;
  format_int(v, saved.knob[k]);
  rect(x, 196, 60, 44, BLACK);
  text(x + (60 - text_width(KNOB_NAME[k], FONT_SMALL)) / 2, 196, KNOB_NAME[k], DIM, FONT_SMALL);
  text(x + (60 - text_width(v, FONT_BIG)) / 2, 210, v, ACCENT, FONT_BIG);
}

/* the middle: what you are playing, and what it is called */
static uint32_t tip_until;
static int tip_active, idle;
static void show_tip(const char *a, const char *b) {
  idle = 0;
  rect(0, 58, SCREEN_W, 58, BLACK);
  centered(64, a, ACCENT, FONT_SMALL);
  centered(86, b, WHITE, FONT_SMALL);
  tip_until = millis() + 2500u;
  tip_active = 1;
}

/* idle: a mala of 12 beads, one per note above Sa (Sa at the top, clockwise), breathing in and out */
static const int16_t SIN12[12] = {0, 128, 222, 256, 222, 128, 0, -128, -222, -256, -222, -128}; /* Q8 */
static void draw_mala(void) {
  uint32_t ph = millis() % 8000u;                                   /* one breath: 4 s in, 4 s out */
  int in = ph < 4000u, r = 14 + (int)((in ? ph : 8000u - ph) * 10u / 4000u);
  static int r_drawn = -1, in_drawn = -1;
  if (r == r_drawn && in == in_drawn) {
    return;
  }
  uint16_t mask = THAAT[saved.thaat].mask;
  rect(8, 58, 64, 58, BLACK);
  for (int s = 0; s < 12; s++) {
    int x = 40 + ((SIN12[s] * r) >> 8), y = 87 - ((SIN12[(s + 3) % 12] * r) >> 8);
    uint16_t c = s == 0 ? SAFFRON : mask >> s & 1 ? TEAL
                                                  : FAINT;
    rect(x - 2, y - 2, 5, 5, c);
  }
  rect(39, 86, 3, 3, r > 21 ? ACCENT : DIM); /* the guru bead */
  if (in != in_drawn) {
    rect(80, 98, 160, 16, BLACK);
    text(80, 98, in ? "breathe in" : "breathe out", FAINT, FONT_SMALL);
  }
  r_drawn = r, in_drawn = in;
}

static void draw_note(int note) {
  rect(0, 58, SCREEN_W, 58, BLACK);
  idle = note < 0;
  if (note < 0) {
    text(80, 62, "be here now", WHITE, FONT_SMALL);
    text(80, 80, RAGA_EG[saved.thaat], DIM, FONT_SMALL);
    text(80, 98, "breathe in", FAINT, FONT_SMALL);
    draw_mala();
    return;
  }
  int pc = note % 12, rel = (pc - saved.sa + 12) % 12;
  char s[40], *p;
  /* "G4" big, then "Pa" */
  p = cat(s, NOTE[pc]);
  format_int(p, note / 12 - 1);
  int w = text_width(s, FONT_BIG), w2 = text_width(SARGAM[rel], FONT_SMALL);
  int x = (SCREEN_W - w - 10 - w2) / 2;
  text(x, 60, s, WHITE, FONT_BIG);
  text(x + w + 10, 72, SARGAM[rel], rel ? TEAL : SAFFRON, FONT_SMALL);
  p = cat(s, INTERVAL[rel]);
  cat(p, " above Sa");
  if (!rel) {
    cat(s, "the home note");
  }
  int in = THAAT[saved.thaat].mask >> rel & 1;
  centered(96, in ? s : "not in this thaat", in ? DIM : SAFFRON, FONT_SMALL);
}

static void draw_chord(const int *notes, int n) {
  idle = 0;
  rect(0, 58, SCREEN_W, 58, BLACK);
  uint16_t pcs = 0;
  int low = 127;
  for (int i = 0; i < n; i++) {
    pcs |= (uint16_t)(1u << (notes[i] % 12));
    low = notes[i] < low ? notes[i] : low;
  }
  char s[40], *p;
  if (n == 1) {
    draw_note(notes[0]);
    return;
  }
  /* try each held note as the root, the lowest first */
  for (int t = 0; t < 12; t++) {
    int root = (low + t) % 12;
    if (!(pcs >> root & 1)) {
      continue;
    }
    uint16_t rel = (uint16_t)(((pcs >> root) | (pcs << (12 - root))) & 0xFFF);
    for (int c = 0; c < CHORDS; c++) {
      if (rel == CHORD[c].mask) {
        p = cat(s, NOTE[root]);
        p = cat(p, " ");
        p = cat(p, CHORD[c].name);
        if (root != low % 12) { /* an inversion: say which note is at the bottom */
          p = cat(p, "/");
          cat(p, NOTE[low % 12]);
        }
        centered(60, s, WHITE, FONT_BIG);
        centered(96, CHORD[c].what, DIM, FONT_SMALL);
        return;
      }
    }
  }
  if (n == 2) { /* two notes: name the interval */
    int a = notes[0] < notes[1] ? notes[0] : notes[1], b = notes[0] ^ notes[1] ^ a;
    p = cat(s, NOTE[a % 12]);
    p = cat(p, " + ");
    cat(p, NOTE[b % 12]);
    centered(60, s, WHITE, FONT_BIG);
    p = cat(s, (b - a) % 12 ? INTERVAL[(b - a) % 12] : "octave");
    if (b - a > 12) {
      cat(p, " + octave");
    }
    centered(96, s, DIM, FONT_SMALL);
    return;
  }
  p = s;
  for (int i = 0; i < 12; i++) {
    if (pcs >> i & 1) {
      p = cat(p, NOTE[i]);
      p = cat(p, " ");
    }
  }
  centered(64, s, WHITE, FONT_SMALL);
  centered(86, "no common chord name", DIM, FONT_SMALL);
}

/* ================================================================ notes */

static int stack[KEY_COUNT + 16], depth; /* the held notes, oldest first (SAROD mode) */
static uint8_t chord_age[4];

static void play_on(int note, int vel) {
  if (saved.mode == MODE_SAROD) {
    int legato = depth > 0 && saved.knob[3] > 0;
    if (depth < (int)(sizeof stack / sizeof stack[0])) {
      stack[depth++] = note;
    }
    lead_target = note * 256;
    if (!legato) {
      lead_vel = vel;
      lead_plucks++;
    }
  } else {
    int pick = 0;
    for (int v = 0; v < 4; v++) {
      chord_age[v]++;
      if (chord_age[v] > chord_age[pick]) {
        pick = v;
      }
    }
    chord_age[pick] = 0;
    chord_note[pick] = (uint32_t)note;
    chord_vel[pick] = (uint32_t)vel;
    chord_plucks[pick]++;
  }
}

static void play_off(int note) {
  int i, j;
  for (i = j = 0; i < depth; i++) {
    if (stack[i] != note) {
      stack[j++] = stack[i];
    }
  }
  depth = j;
  if (saved.mode == MODE_SAROD && depth > 0) {
    lead_target = stack[depth - 1] * 256; /* slide back to the note still held */
  }
}

/* ================================================================ setup / loop */

/* the start screen: a lotus on still water */
static void petal(int bx, int by, int tx, int ty, int w, uint16_t c) { /* a pointed petal from base to tip */
  for (int i = -w; i <= w; i++) {
    int mx = (bx + tx) / 2 + i, my = (by + ty) / 2 + (i < 0 ? -i : i) / 3;
    line(bx, by, mx, my, c);
    line(mx, my, tx, ty, c);
  }
}
static void draw_splash(void) {
  clear(BLACK);
  uint16_t pink = RGB(235, 120, 150), deep = RGB(190, 60, 100), leaf = RGB(40, 120, 90);
  for (int i = 0; i < 5; i++) { /* water */
    line(30 + i * 9, 132 + i * 4, 210 - i * 9, 132 + i * 4, RGB(30, 60 + i * 12, 90 + i * 15));
  }
  petal(120, 128, 40, 104, 6, leaf);
  petal(120, 128, 200, 104, 6, leaf);
  petal(120, 126, 62, 76, 8, deep);
  petal(120, 126, 178, 76, 8, deep);
  petal(120, 126, 88, 56, 9, pink);
  petal(120, 126, 152, 56, 9, pink);
  petal(120, 126, 120, 40, 10, RGB(255, 170, 190));
  rect(117, 18, 6, 6, SAFFRON); /* a drop of light */
  centered(160, "BE HERE NOW", WHITE, FONT_BIG);
  centered(200, "love  serve  remember", ACCENT, FONT_SMALL);
  centered(220, "after Ram Dass", DIM, FONT_SMALL);
}

void setup(void) {
  settings_load(&saved, sizeof saved);
  if (saved.octave < -2 || saved.octave > 2 || saved.thaat >= THAATS || saved.sa > 11 || saved.mode > 1) {
    saved.octave = 0, saved.thaat = 1, saved.sa = 0, saved.mode = MODE_SAROD;
  }
  for (int k = 0; k < 4; k++) {
    if (saved.knob[k] > 127) {
      saved.knob[k] = 64;
    }
  }
  lead = (string_t){main_buf, 2047, 0, 200u << 16, 0, 12000, 32500};
  for (int v = 0; v < 4; v++) {
    chord[v] = (string_t){chord_buf[v], 2047, 0, 200u << 16, 0, 12000, 32500};
    drone[v] = (string_t){drone_buf[v], 1023, 0, 200u << 16, 0, 13500, 32720};
  }
  for (int v = 0; v < 11; v++) {
    taraf[v] = (string_t){taraf_buf[v], 255, 0, 100u << 16, 0, 11000, 32690};
  }
  lead_target = (60 + saved.sa) * 256;
  retune = 1;
  led_backlight(1);
  draw_splash();
  print("BE HERE NOW: love, serve, remember");
}

void loop(void) {
  static uint8_t held[KEY_COUNT];
  static uint32_t lit_drawn = ~0u, changed_at;
  static int dirty, base_drawn = -999, note_dirty = 1;
  static uint16_t strip_drawn = 0xFFFF;
  int base = KEY_FIRST_NOTE + 12 * saved.octave;
  uint8_t st, d1, d2;

  /* the keys, and MIDI notes from a computer or keyboard */
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_pressed(k)) {
      held[k] = (uint8_t)(base + k);
      play_on(held[k], 100);
      midi_send(0x90, held[k], 100);
      note_dirty = 1;
    }
    if (key_released(k)) {
      play_off(held[k]);
      midi_send(0x80, held[k], 0);
      note_dirty = 1;
    }
  }
  while (midi_read(&st, &d1, &d2)) {
    if ((st & 0xF0) == 0x90 && d2) {
      play_on(d1, d2);
      note_dirty = 1;
    } else if ((st & 0xF0) == 0x80 || (st & 0xF0) == 0x90) {
      play_off(d1);
      note_dirty = 1;
    }
  }

  /* the start screen stays 3 s, or until a key is played */
  static int ui;
  if (!ui) {
    if (millis() < 3000 && !keys()) {
      return;
    }
    ui = 1;
    clear(BLACK);
    draw_header();
    for (int k = 0; k < 4; k++) {
      draw_knob(k);
    }
    note_dirty = 1, lit_drawn = ~0u, strip_drawn = 0xFFFF;
  }

  /* the controls */
  int changed = 0;
  int oct = saved.octave + button_pressed(BTN_OCT_UP) - button_pressed(BTN_OCT_DOWN);
  if (oct != saved.octave && oct >= -2 && oct <= 2) {
    saved.octave = (int8_t)oct, changed = 1;
  }
  int32_t e = encoder(ENC_PRESETS);
  if (e) {
    saved.thaat = (uint8_t)((saved.thaat + THAATS + (e > 0 ? 1 : -1)) % THAATS);
    retune = 1, changed = 1;
    draw_header();
    show_tip("THAAT: parent scale of ragas", RAGA_EG[saved.thaat]);
  }
  e = encoder(ENC_ALGORITHM);
  if (e) {
    saved.sa = (uint8_t)((saved.sa + 12 + (e > 0 ? 1 : -1)) % 12);
    retune = 1, changed = 1;
    draw_header();
    show_tip("SA: the home note, the tonic", "every other note is heard against it");
  }
  if (button_pressed(BTN_SEL)) {
    saved.mode ^= 1;
    depth = 0;
    changed = 1;
    draw_header();
    show_tip(saved.mode == MODE_SAROD ? "SAROD: one string" : "CHORD: four strings",
             saved.mode == MODE_SAROD ? "hold a key, press another: meend" : "hold keys to name the chord");
  }
  if (button_pressed(BTN_PLAY)) {
    drone_on ^= 1;
    draw_header();
    show_tip("DRONE: a tanpura plays Pa Sa Sa Sa", "hear each note against home");
  }
  for (int k = 0; k < 4; k++) {
    int32_t d = encoder(ENC_KNOB1 + k);
    if (d) {
      int v = saved.knob[k] + d * 2;
      v = v < 0 ? 0 : v > 127 ? 127
                              : v;
      if (v != saved.knob[k]) {
        saved.knob[k] = (uint8_t)v;
        draw_knob(k);
        changed = 1;
        show_tip(KNOB_TIP[k], "");
      }
    }
  }
  if (changed) {
    dirty = 1, changed_at = millis();
  }
  led_button(BTN_OCT_DOWN, saved.octave < 0);
  led_button(BTN_OCT_UP, saved.octave > 0);
  led_button(BTN_PLAY, drone_on);
  led_button(BTN_SEL, saved.mode == MODE_CHORD);

  /* what is held: the keyboard picture, the strip, the key lights */
  base = KEY_FIRST_NOTE + 12 * saved.octave;
  uint32_t lit = 0;
  uint16_t strip = 0;
  int notes[KEY_COUNT], n = 0;
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_held(k)) {
      lit |= 1u << k;
      notes[n++] = held[k];
      strip |= (uint16_t)(1u << ((held[k] % 12 - saved.sa + 12) % 12));
    }
    int pc = ((base + k) % 12 - saved.sa + 24) % 12;
    led_key(k, THAAT[saved.thaat].mask >> pc & 1);
  }
  if (lit != lit_drawn || base != base_drawn || changed) {
    lit_drawn = lit, base_drawn = base;
    draw_keys(lit, base);
  }
  if (strip != strip_drawn || changed) {
    strip_drawn = strip;
    draw_strip(strip);
  }
  if (tip_active && millis() >= tip_until) {
    tip_active = 0, note_dirty = 1;
  }
  if (!tip_active && !note_dirty && idle) {
    static uint32_t anim;
    if (millis() - anim >= 120u) { /* the breathing mala: a few frames a second, only its corner */
      anim = millis();
      draw_mala();
    }
  }
  if (!tip_active && note_dirty) {
    note_dirty = 0;
    if (saved.mode == MODE_SAROD) {
      draw_note(depth ? stack[depth - 1] : (n ? notes[n - 1] : -1));
    } else {
      if (n) {
        draw_chord(notes, n);
      } else {
        draw_note(-1);
      }
    }
  }

  if (dirty && !n && millis() - changed_at > 3000) { /* saving stalls the sound for a moment: wait */
    settings_save(&saved, sizeof saved);
    dirty = 0;
  }
}
