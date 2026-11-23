/* BE HERE NOW: a sarod-inspired string instrument that shows you what you play.
 * After Ram Dass: love, serve, remember.
 *
 * The controls, as they sit around the screen:
 *   KNOB1..4    Tone, Taraf, Sustain, Meend: the four gauges along the top of the screen
 *   PRESETS     the thaat (parent scale): bottom left of the screen
 *   ALGORITHM   Sa, the home note: next to it
 *   SELECT      the drone's level
 *   MASTER      the volume
 *   OCT- OCT+   the octave
 *   SEL         Sarod (one string, slides) or Chord (four strings, names and shapes)
 *   PLAY        the tanpura drone on / off: the four strings at the bottom right
 *   ARP         jhala: the high drone strings strum along with the note you hold
 *   FX          reverb: hall, room, dry
 *   HOME        for Sylmira
 *   SAVE        keep the settings through power-off (they are also kept on their own when all is silent)
 *   GLO         how busy the sound engine is
 *   SEQ         Learn mode: Raga studio, Find the note, Alankars, Taal (see learn.h)
 *   REC         the scale on its own: the thaat from Sa up to Sa and back down, over and over, the keys lighting up
 *               as it climbs (the note sounding blinks); KNOB4 is its tempo; play a key and it stops for your turn
 *
 * Over USB MIDI, notes in play, and the notes you play go out. The Coach (coach.html) sets the instrument up
 * with control changes: CC 20 Sa (0..11), CC 21 thaat (0..9), CC 22 drone (>= 64 on), CC 23 mode (>= 64 Chord),
 * CC 24 Learn mode (>= 64 on). The key lights follow the Coach while it is open: CC 25 / 26 the notes that blink
 * (what the lesson wants next) and CC 27 / 28 the notes lit steadily (the scale on its screen), each a mask of
 * pitch classes from C (25 and 27: C..F#, bit 0 = C; 26 and 28: G..B, bit 0 = G); turning PRESETS or ALGORITHM, and
 * SEL and PLAY, send the same CC 20..23 back, so the Coach follows the FM-1; CC 29 >= 64 keeps them on,
 * sent every 2 s, and 3 s of silence (or CC 29 < 64, or Learn mode) gives the lights back to the FM-1.
 *   keys        play; in Sarod mode press a key while holding another to slide (meend)
 *   key lights  the notes of the thaat
 *
 * The screen: the 12 notes as a mala of beads, Sa at the top. What you play glows marigold; the sympathetic
 * strings that answer it glow rose; an arc from Sa shows how far your note is from home; a slide carries a spark
 * around the ring; a chord draws its shape. When you stop, the centre breathes.
 *
 * The sound: Karplus-Strong strings (a burst of noise in a delay line, filtered a little on every pass), with a
 * stiffness filter for the metal, a skin body, 11 sympathetic strings tuned to the thaat, a tanpura with a buzzing
 * bridge (jawari), two high chikari strings for jhala, and a hall. Integers only: the FM-1 has no FPU.
 * Built on foma by konsumer (GPL-3.0). */
#include "fm1.h"
#include "gfx.h"
#include "period.h"

/* ================================================================ music knowledge */

typedef struct {
  const char *name, *western, *raga;
  uint16_t mask; /* bit n: n semitones above Sa is in the scale */
} thaat_t;
#define M(a, b, c, d, e, f, g) (uint16_t)(1u << a | 1u << b | 1u << c | 1u << d | 1u << e | 1u << f | 1u << g)
static const thaat_t THAAT[] = {
  {"Bilawal", "major scale", "Alhaiya Bilawal", M(0, 2, 4, 5, 7, 9, 11)},
  {"Kalyan", "Lydian mode", "Yaman", M(0, 2, 4, 6, 7, 9, 11)},
  {"Khamaj", "Mixolydian", "Khamaj", M(0, 2, 4, 5, 7, 9, 10)},
  {"Kafi", "Dorian mode", "Kafi", M(0, 2, 3, 5, 7, 9, 10)},
  {"Asavari", "natural minor", "Darbari", M(0, 2, 3, 5, 7, 8, 10)},
  {"Bhairavi", "Phrygian mode", "Bhairavi", M(0, 1, 3, 5, 7, 8, 10)},
  {"Bhairav", "double harmonic", "Ahir Bhairav", M(0, 1, 4, 5, 7, 8, 11)},
  {"Purvi", "no Western twin", "Purvi", M(0, 1, 4, 6, 7, 8, 11)},
  {"Marwa", "no Western twin", "Marwa", M(0, 1, 4, 6, 7, 9, 11)},
  {"Todi", "no Western twin", "Miyan ki Todi", M(0, 1, 3, 6, 7, 8, 11)},
};
#define THAATS 10

/* "\x01" is a sharp sign in the fonts */
static const char *const NOTE[12] = {"C", "C\x01", "D", "D\x01", "E", "F", "F\x01", "G", "G\x01", "A", "A\x01", "B"};
static const char *const SARGAM[12] = {"Sa", "komal Re", "Re", "komal Ga", "Ga", "Ma",
                                       "tivra Ma", "Pa", "komal Dha", "Dha", "komal Ni", "Ni"};
static const uint8_t SYL[12] = {0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6};  /* Devanagari syllable: sa re ga ma pa dha ni */
static const uint8_t MARK[12] = {0, 1, 0, 1, 0, 0, 2, 0, 1, 0, 1, 0}; /* 1 komal (line below), 2 tivra (line above) */
static const char *const INTERVAL[12] = {"the home note", "minor 2nd", "major 2nd", "minor 3rd", "major 3rd", "perfect 4th",
                                         "tritone", "perfect 5th", "minor 6th", "major 6th", "minor 7th", "major 7th"};

typedef struct {
  const char *sym, *what, *feel;
  uint16_t mask;
} chord_t;
#define C3(a, b, c) (uint16_t)(1u << a | 1u << b | 1u << c)
#define C4(a, b, c, d) (uint16_t)(1u << a | 1u << b | 1u << c | 1u << d)
static const chord_t CHORD[] = {
  {"", "major triad", "bright, settled", C3(0, 4, 7)},
  {"m", "minor triad", "soft, inward", C3(0, 3, 7)},
  {"dim", "diminished", "tense, uneasy", C3(0, 3, 6)},
  {"aug", "augmented", "dreamy, adrift", C3(0, 4, 8)},
  {"sus2", "suspended 2nd", "open, airy", C3(0, 2, 7)},
  {"sus4", "suspended 4th", "waiting", C3(0, 5, 7)},
  {"7", "dominant 7th", "wants to move", C4(0, 4, 7, 10)},
  {"maj7", "major 7th", "warm, glowing", C4(0, 4, 7, 11)},
  {"m7", "minor 7th", "mellow, soulful", C4(0, 3, 7, 10)},
  {"m7\x02" "5", "half-diminished", "dark, searching", C4(0, 3, 6, 10)},
  {"dim7", "diminished 7th", "suspense", C4(0, 3, 6, 9)},
  {"6", "major 6th", "sweet, wistful", C4(0, 4, 7, 9)},
  {"m6", "minor 6th", "bittersweet", C4(0, 3, 7, 9)},
  {"5", "power chord", "root and 5th", (uint16_t)(1u << 0 | 1u << 7)},
};
#define CHORDS 14

static char *cat(char *d, const char *s) {
  while (*s) {
    *d++ = *s++;
  }
  *d = 0;
  return d;
}

/* ================================================================ the strings */

typedef struct {
  int16_t *buf;
  uint32_t mask; /* buffer length - 1 (a power of two) */
  uint32_t w;    /* write position */
  uint32_t d;    /* delay, samples, Q16 */
  int32_t lp;    /* loop filter state */
  int32_t a;     /* loop filter, Q14: 16384 = no filtering, lower = darker */
  int32_t fb;    /* feedback, Q15: the sustain */
  int32_t ap, apx, apy; /* stiffness: an allpass in the loop (Q15 coefficient, 0 = off) and its state */
  int32_t jaw;   /* jawari: the bridge the string buzzes against (0 = none) */
  int32_t quiet; /* blocks in a row it has been silent: past SLEEP it is not computed until plucked */
  int32_t pk;    /* this block's loudest sample */
} string_t;
#define SLEEP 12
/* the sympathetic strings ring about 4 s on Sa, as a sarod's taraf does (they rang ~10 s, and a drone kept them humming) */
#define TARAF_FB 32560
#define TARAF_SHIFT 8

static int16_t lead_buf[2048] POOL;
static int16_t chord_buf[4][2048] POOL;
static int16_t drone_buf[4][1024] POOL;
static int16_t taraf_buf[11][256] POOL;
static int16_t chik_buf[2][256] POOL;
static string_t lead, chord[4], drone[4], taraf[11], chik[2];

static int32_t clamp16(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767
                                                                           : v; }
/* a soft ceiling: loud moments round off instead of cracking */
static int32_t soft16(int32_t v) {
  if (v > 22000) {
    v = 22000 + ((v - 22000) >> 2);
  } else if (v < -22000) {
    v = -22000 + ((v + 22000) >> 2);
  }
  return clamp16(v);
}

static inline int32_t string_step(string_t *s, int32_t in) {
  if (s->quiet >= SLEEP && !in) { /* asleep: silent strings cost nothing */
    return 0;
  }
  uint32_t i = s->d >> 16, f = (s->d & 0xFFFFu) >> 2;
  int32_t s0 = s->buf[(s->w - i) & s->mask], s1 = s->buf[(s->w - i - 1u) & s->mask];
  int32_t y = s0 + (((s1 - s0) * (int32_t)f) >> 14);
  s->lp += ((y - s->lp) * s->a) >> 14;
  int32_t v = (s->lp * s->fb) >> 15;
  if (s->ap) { /* stiffness: high partials come round a little sooner, as on a steel string */
    int32_t o = ((s->ap * v) >> 15) + s->apx - ((s->ap * s->apy) >> 15);
    s->apx = v, s->apy = o, v = o;
  }
  if (s->jaw) { /* jawari: the string grazes a curved bridge, which folds energy into bright overtones */
    if (v > s->jaw) {
      v = s->jaw + ((v - s->jaw) >> 1);
    } else if (v < -s->jaw * 3) {
      v = -s->jaw * 3 + ((v + s->jaw * 3) >> 1);
    }
  }
  s->buf[s->w] = (int16_t)clamp16(v + in);
  s->w = (s->w + 1u) & s->mask;
  s->pk |= y < 0 ? -y : y;
  return y;
}
/* end of a block: asleep after SLEEP silent blocks */
static void string_block(string_t *s) {
  s->quiet = s->pk < 8 ? (s->quiet < SLEEP ? s->quiet + 1 : SLEEP) : 0;
  s->pk = 0;
}

/* the delay that plays pitch p (Q8 MIDI), less what the loop's filters add */
#define AP_STIFF (-13500) /* the lead's stiffness: about -0.41 */
static uint32_t delay_for(int32_t p, const string_t *s) {
  p = p < 24 * 256 ? 24 * 256 : p > 120 * 256 ? 120 * 256
                                              : p;
  uint32_t n = (uint32_t)p >> 8, fr = (uint32_t)p & 255u;
  uint32_t per = PERIOD[n] - (((PERIOD[n] - PERIOD[n + 1]) >> 8) * fr);
  uint32_t comp = ((uint32_t)(16384 - s->a) << 16) / (uint32_t)s->a;
  if (s->ap) { /* an allpass delays low notes by (1 - c) / (1 + c) samples */
    comp += (uint32_t)(((32768 - s->ap) << 15) / (32768 + s->ap)) << 1;
  }
  return per > comp + 2 * 65536u ? per - comp : 2 * 65536u;
}

/* a pluck: the plectrum's click, then a burst of filtered noise along one period, struck at 1/pick of the string */
static void pluck(string_t *s, int32_t level, int32_t tone, int pick) {
  uint32_t len = s->d >> 16, k;
  int32_t lpn = 0;
  if (len > s->mask) {
    len = s->mask;
  }
  s->quiet = 0;
  for (k = 0; k < len; k++) {
    int32_t n = (int32_t)(random32() >> 17) - 16384;
    if (k < 3) { /* the java, a coconut-shell plectrum: a hard edge */
      n += 22000 - (int32_t)k * 7000;
    }
    lpn += ((n - lpn) * tone) >> 14;
    uint32_t at = (s->w - 1u - k) & s->mask;
    s->buf[at] = (int16_t)clamp16(s->buf[at] / 3 + ((lpn * level) >> 14));
  }
  uint32_t P = len / (uint32_t)pick;
  for (k = len; P && k-- > P;) {
    uint32_t at = (s->w - 1u - k) & s->mask, from = (s->w - 1u - (k - P)) & s->mask;
    s->buf[at] = (int16_t)clamp16(s->buf[at] - s->buf[from] / 2);
  }
}

/* the skin: three resonances of the sarod's goatskin belly (state-variable bandpass filters) */
typedef struct {
  int32_t lp, bp, f, q;
} svf_t;
static inline int32_t svf_bp(svf_t *s, int32_t x) {
  s->lp += (s->f * s->bp) >> 15;
  int32_t hp = x - s->lp - ((s->q * s->bp) >> 15);
  s->bp += (s->f * hp) >> 15;
  s->bp = s->bp > 400000 ? 400000 : s->bp < -400000 ? -400000
                                                     : s->bp;
  return s->bp;
}
static svf_t body[3] = {{0, 0, 980, 5461}, {0, 0, 3173, 8192}, {0, 0, 8370, 16384}}; /* 210 Hz, 680 Hz, 1.8 kHz */

/* the hall: four combs and two allpasses a side (after Freeverb), 16-bit lines */
static const uint16_t COMB_LEN[8] = {1116, 1188, 1277, 1356, 1139, 1211, 1300, 1379};
static const uint16_t AP_LEN[4] = {556, 441, 579, 464};
static int16_t rv_comb[1116 + 1188 + 1277 + 1356 + 1139 + 1211 + 1300 + 1379] POOL;
static int16_t rv_ap[556 + 441 + 579 + 464] POOL;
static int16_t *comb_at[8], *ap_at[4];
static uint16_t comb_i[8], ap_i[4];
static int32_t comb_lp[8];

static int32_t reverb_side(int side, int32_t in, int32_t fb, int32_t damp) {
  int32_t sum = 0;
  for (int c = side * 4; c < side * 4 + 4; c++) {
    int32_t o = comb_at[c][comb_i[c]];
    comb_lp[c] = ((o * (32768 - damp)) >> 15) + ((comb_lp[c] * damp) >> 15);
    comb_at[c][comb_i[c]] = (int16_t)clamp16(in + ((comb_lp[c] * fb) >> 15));
    if (++comb_i[c] >= COMB_LEN[c]) {
      comb_i[c] = 0;
    }
    sum += o;
  }
  sum >>= 2;
  for (int a = side * 2; a < side * 2 + 2; a++) {
    int32_t b = ap_at[a][ap_i[a]];
    ap_at[a][ap_i[a]] = (int16_t)clamp16(sum + (b >> 1));
    sum = b - sum;
    if (++ap_i[a] >= AP_LEN[a]) {
      ap_i[a] = 0;
    }
  }
  return sum;
}

/* ================================================================ state shared by loop() and audio() */

enum { MODE_SAROD,
       MODE_CHORD };

static struct {
  int8_t octave;
  uint8_t thaat, sa, mode;
  uint8_t knob[4];   /* Tone, Taraf, Sustain, Meend: 0..127 */
  uint8_t drone_lvl; /* 0..8 */
  uint8_t reverb;    /* 0 hall, 1 room, 2 dry */
} saved = {0, 1, 0, MODE_SAROD, {80, 70, 90, 20}, 4, 0};

/* loop() writes, audio() reads */
static volatile int32_t lead_target;  /* Q8 MIDI pitch */
static volatile uint32_t lead_plucks; /* counts up: a new pluck */
static volatile int32_t lead_vel;     /* 0..127 */
static volatile uint8_t lead_held;    /* a key is down (for jhala) */
static volatile uint32_t chord_note[4], chord_plucks[4], chord_vel[4];
static volatile uint8_t drone_on, retune, jhala;
/* setup() has built the strings and the hall: on the FM-1 the sound starts before setup() runs, so audio()
 * must not touch them until this is set */
static volatile uint8_t ready;
/* Learn mode: the pulse (tabla, count-in) runs inside audio(), to the sample */
static volatile uint8_t pulse_on, pulse_restart, pulse_len, pulse_bol_now;
static const uint8_t *volatile pulse_bols;
static volatile uint32_t pulse_spb, pulse_beats;
static volatile uint8_t meend_force;   /* a lesson's demonstration slides at its own speed */
static volatile uint16_t scale_mask;   /* the notes the sympathetic strings are tuned to */

/* audio() writes, loop() reads: what is sounding, for the screen */
static volatile int32_t lead_now;    /* the lead's pitch now, Q8 (it slides) */
static volatile uint16_t lvl_lead, lvl_chord[4], lvl_drone[4], lvl_taraf[11];
static volatile uint32_t lead_struck, chord_struck[4]; /* counts the plucks heard (jhala strikes too) */
static volatile uint32_t load_us, load_blocks, load_peak; /* how long audio() takes: for the GLO readout */
static uint8_t taraf_pc[11]; /* which note each sympathetic string is tuned to (semitones above Sa) */

#include "learn.h"

/* ================================================================ the tabla */

static int16_t dayan_buf[512] POOL;
static string_t dayan;             /* the right drum: a short, bright string tuned to Sa */
static int32_t bay_amp;            /* the left drum: a low boom that falls in pitch */
static uint32_t bay_ph, bay_inc, bay_end;

static void tabla_hit(int bol) {
  if (BOL_BAYAN[bol]) {
    bay_amp = 15000, bay_inc = note_inc(46), bay_end = note_inc(39);
  }
  int kind = BOL_DAYAN[bol];
  if (kind) {
    static const int32_t A[4] = {0, 14500, 12500, 8000}, FB[4] = {0, 32250, 32600, 27500}, LVL[4] = {0, 7000, 6000, 6500};
    static const int32_t TONE[4] = {0, 15000, 11000, 8000};
    dayan.a = A[kind], dayan.fb = FB[kind];
    dayan.d = delay_for((60 + saved.sa) * 256, &dayan);
    pluck(&dayan, LVL[kind], TONE[kind], kind == 3 ? 3 : 4);
  }
}

/* ================================================================ the sound */

static void peak(uint16_t *p, int32_t y) {
  int32_t a = y < 0 ? -y : y;
  if (a > *p) {
    *p = (uint16_t)(a > 65535 ? 65535 : a);
  }
}
static void settle(volatile uint16_t *lvl, uint16_t pk) { /* a meter: rises at once, falls over ~0.2 s */
  uint16_t l = *lvl;
  *lvl = pk > l ? pk : (uint16_t)(l - (l >> 4));
}

void audio(int32_t *out, uint32_t frames) {
  static uint32_t lead_seen, chord_seen[4], drone_next = 1, jhala_clock;
  static uint8_t drone_which, jhala_step, pulse_pos;
  static uint32_t pulse_next;
  static int32_t lead_pitch;
  int32_t tone_a = 4096 + saved.knob[0] * 96;
  int32_t fb = 32760 - (127 - saved.knob[2]) * 4;
  int32_t taraf_level = saved.knob[1] * saved.knob[1] / 127; /* a gentle curve: the middle is a faint halo */
  int32_t pluck_tone = 3000 + saved.knob[0] * 105;
  int32_t drone_gain = saved.drone_lvl * 22; /* 0..176, /512 */
  static const int32_t RV_FB[3] = {28300, 25000, 0}, RV_WET[3] = {150, 90, 0};
  int32_t rv = saved.reverb < 3 ? saved.reverb : 0, rv_fb = RV_FB[rv], rv_wet = RV_WET[rv];
  uint16_t pk_lead = 0, pk_chord[4] = {0}, pk_drone[4] = {0}, pk_taraf[11] = {0};
  uint32_t i;
  int v;
  if (!ready) {
    return;
  }
  uint32_t t_start = micros();
  static int32_t rv_quiet;

  if (retune) { /* the thaat or Sa changed: retune the sympathetic, drone and chikari strings */
    retune = 0;
    int n = 0, sa = 60 + saved.sa;
    uint16_t mask = scale_mask;
    for (int oct = 0; oct < 2 && n < 11; oct++) {
      for (int s = 0; s < 12 && n < 11; s++) {
        if (mask >> s & 1u) {
          taraf[n].d = delay_for((sa + 12 * oct + s) * 256, &taraf[n]);
          taraf_pc[n] = (uint8_t)s;
          n++;
        }
      }
    }
    static const int8_t DRONE_NOTE[4] = {-5, 0, 0, -12}; /* Pa below Sa, Sa, Sa, Sa an octave down */
    for (v = 0; v < 4; v++) {
      drone[v].d = delay_for((48 + saved.sa + DRONE_NOTE[v]) * 256, &drone[v]);
    }
    chik[0].d = delay_for((72 + saved.sa) * 256, &chik[0]);
    chik[1].d = delay_for((84 + saved.sa) * 256, &chik[1]);
    dayan.d = delay_for((60 + saved.sa) * 256, &dayan);
  }

  /* the lead string: a pluck, or a slide (meend) toward the target */
  lead.a = tone_a;
  lead.fb = fb;
  if (lead_plucks != lead_seen) {
    lead_seen = lead_plucks;
    lead_pitch = lead_target;
    lead.d = delay_for(lead_pitch, &lead);
    pluck(&lead, 200 + lead_vel * 200, pluck_tone, 9);
    lead_struck++;
    jhala_clock = 0, jhala_step = 1;
  } else if (lead_pitch != lead_target) {
    int32_t t = lead_target, knob = meend_force ? meend_force : saved.knob[3];
    int32_t step = knob ? (int32_t)(frames * 1486u / (1024u * (uint32_t)knob)) : 1 << 20;
    step = step < 1 ? 1 : step;
    lead_pitch = lead_pitch < t ? (lead_pitch + step > t ? t : lead_pitch + step)
                                : (lead_pitch - step < t ? t : lead_pitch - step);
  }
  lead.d = delay_for(lead_pitch, &lead);
  lead_now = lead_pitch;

  for (v = 0; v < 4; v++) {
    chord[v].a = tone_a;
    chord[v].fb = fb;
    if (chord_plucks[v] != chord_seen[v]) {
      chord_seen[v] = chord_plucks[v];
      chord[v].d = delay_for((int32_t)chord_note[v] * 256, &chord[v]);
      pluck(&chord[v], 150 + (int32_t)chord_vel[v] * 150, pluck_tone, 8);
      chord_struck[v]++;
    }
  }

  for (i = 0; i < frames; i++) {
    /* the tanpura: one string every 0.6 s, a little unevenly, as a hand plays it */
    if (drone_on && --drone_next == 0) {
      drone_next = 25000 + (random32() & 2047);
      pluck(&drone[drone_which], 9000, 11000, 7);
      drone_which = (uint8_t)((drone_which + 1) & 3);
    }
    /* jhala: while a note is held, the chikari strings strum three strokes between its own: da ra ra ra */
    if (jhala && lead_held && saved.mode == MODE_SAROD && ++jhala_clock >= 5900) {
      jhala_clock = 0;
      if (jhala_step == 0) {
        pluck(&lead, 4000, pluck_tone, 9);
        lead_struck++;
      } else {
        pluck(&chik[jhala_step & 1], 5200, 12000, 6);
      }
      jhala_step = (uint8_t)((jhala_step + 1) & 3);
    }

    /* Learn mode's pulse: the tabla's next bol, exactly on the beat */
    if (pulse_on) {
      if (pulse_restart) {
        pulse_restart = 0, pulse_next = 1, pulse_pos = 0;
      }
      if (--pulse_next == 0) {
        pulse_next = pulse_spb;
        int bol = pulse_bols[pulse_pos];
        tabla_hit(bol);
        pulse_bol_now = (uint8_t)bol;
        pulse_pos = (uint8_t)((pulse_pos + 1) % pulse_len);
        pulse_beats++;
      }
    }
    int32_t tb = string_step(&dayan, 0);
    if (bay_amp > 16) {
      bay_ph += bay_inc;
      bay_inc -= (bay_inc - bay_end) >> 11;
      tb += (sine(bay_ph) * bay_amp) >> 15;
      bay_amp -= bay_amp >> 12;
    }

    int32_t y = string_step(&lead, 0), played = y;
    peak(&pk_lead, y);
    for (v = 0; v < 4; v++) {
      y = string_step(&chord[v], 0);
      peak(&pk_chord[v], y);
      played += y / 2;
    }
    int32_t chk = string_step(&chik[0], 0) + string_step(&chik[1], 0);
    played += chk / 2;
    int32_t dr = 0;
    for (v = 0; v < 4; v++) {
      y = string_step(&drone[v], 0);
      peak(&pk_drone[v], y);
      dr += y;
    }
    dr = (dr * drone_gain) >> 9;
    /* the skin colours what is plucked */
    int32_t sk = played + ((svf_bp(&body[0], played) * 7000) >> 15) + ((svf_bp(&body[1], played) * 6000) >> 15) +
                 ((svf_bp(&body[2], played) * 4200) >> 15);
    /* the sympathetic strings: they answer what you play (the drone only brushes them), ringing only where they agree
     * with it; a halo under the music, not over it. At KNOB2 = 0 they rest and cost nothing */
    int32_t excite = (played + (dr >> 5)) >> 8, l = 0, r = 0;
    if (taraf_level) {
      for (v = 0; v < 11; v++) {
        y = string_step(&taraf[v], excite);
        peak(&pk_taraf[v], y);
        if (v & 1) {
          l += y;
        } else {
          r += y;
        }
      }
      l = (l * taraf_level) >> TARAF_SHIFT;
      r = (r * taraf_level) >> TARAF_SHIFT;
    }
    int32_t dl = sk + (dr * 3) / 4 + l + (r >> 2) + (tb * 3) / 4, dright = sk + (dr * 3) / 4 + r + (l >> 2) + (tb * 3) / 4;
    if (rv_wet) {
      int32_t send = (dl + dright) >> 4;
      if (send || rv_quiet < 700) { /* the hall rests once its tail has died away (~4 s of silence) */
        int32_t wl = reverb_side(0, send, rv_fb, 7000), wr = reverb_side(1, send, rv_fb, 7000);
        if (send || wl > 6 || wl < -6) {
          rv_quiet = -1; /* (counted per block below) */
        }
        dl += (wl * rv_wet) >> 7;
        dright += (wr * rv_wet) >> 7;
      }
    }
    out[2 * i] += soft16((dl * 5) >> 3);
    out[2 * i + 1] += soft16((dright * 5) >> 3);
  }
  settle(&lvl_lead, pk_lead);
  for (v = 0; v < 4; v++) {
    settle(&lvl_chord[v], pk_chord[v]);
    settle(&lvl_drone[v], pk_drone[v]);
  }
  for (v = 0; v < 11; v++) {
    settle(&lvl_taraf[v], pk_taraf[v]);
  }
  string_block(&lead);
  for (v = 0; v < 4; v++) {
    string_block(&chord[v]), string_block(&drone[v]);
  }
  for (v = 0; v < 11; v++) {
    string_block(&taraf[v]);
  }
  string_block(&chik[0]), string_block(&chik[1]), string_block(&dayan);
  rv_quiet = rv_quiet < 0 ? 0 : rv_quiet < 100000 ? rv_quiet + 1 : rv_quiet;
  /* the load: time spent here against the block's own length (frames / 44.1 kHz) */
  uint32_t us = micros() - t_start;
  load_us += us, load_blocks += frames;
  if (us * 441u / frames > load_peak) {
    load_peak = us * 441u / frames; /* percent x 100 of the block's time */
  }
}

/* ================================================================ the screen */

#define IVORY 0xf4ecd8u
#define MARIGOLD 0xf5a623u
#define ROSE 0xe8879cu
#define MIST 0x9295c6u
#define DUSK 0x3d4290u
#define INK 0x1d2152u
#define WARM 0xfff4dcu

#define CX 120
#define CY 124
#define RING_R 62
#define BEAD_R 11
#define LETTER_R 81

static int bead_x[12], bead_y[12]; /* Q4 */

enum { C_IDLE,
       C_NOTE,
       C_CHORD,
       C_TIP,
       C_LEARN };
enum { TIP_TONE,
       TIP_TARAF,
       TIP_SUSTAIN,
       TIP_MEEND,
       TIP_THAAT,
       TIP_SA,
       TIP_MODE,
       TIP_DRONE,
       TIP_OCTAVE,
       TIP_DRONE_LVL,
       TIP_REVERB,
       TIP_JHALA,
       TIP_SAVED,
       TIP_LOAD,
       TIP_LEARN,
       TIP_LESSON,
       TIP_ITEM,
       TIP_TEMPO,
       TIP_SCALE };
static uint8_t scale_on, scale_turn; /* REC: the scale playing on its own; stopped by a key (your turn) */
static uint32_t scale_keys;          /* the keys it has played this time up and down */
static int scale_now = -1;           /* the key sounding now */
static uint8_t scale_note;           /* the note it is playing */
static uint32_t load_avg, load_max; /* percent, over the last second */
static uint32_t gap_max_us, gap_show; /* the longest time between two readings of the keys, for GLO */

/* what the screen shows: loop() works it out, compares it with what is drawn, and repaints what differs */
typedef struct {
  uint8_t thaat, sa, mode, drone, reverb, jhala;
  int8_t octave, active_knob;
  uint8_t knob[4];
  uint8_t play[12], symp[12], ripple[12]; /* 0..16 */
  int16_t comet;                          /* angle, or -1 */
  int16_t arc;                            /* the interval arc's end angle, or -1 */
  uint16_t chord_pcs;
  uint8_t center, breath, tip;
  int16_t note; /* C_NOTE: MIDI note */
  uint8_t root, kind, bass, interval; /* C_CHORD: kind 255 = two notes (an interval), 254 = unnamed */
  uint8_t dstr[4];
  uint8_t egg, twinkle; /* for Sylmira */
  /* Learn mode */
  uint8_t learn, lesson, litem, lstate, phase, hint, verdict, count, tbeat, tbol, tempo;
  uint8_t n1, n2;
  int8_t vadi, samv, target;
  uint16_t mask; /* the notes that belong: the thaat's, or the raga's */
} view_t;
static view_t cur, drawn;

static int bead_ang(int s) { return (s * 1024 + 6) / 12; }

/* ---- the parts of the picture */

static void draw_gauges(void) {
  static const char *const NAME[4] = {"Tone", "Taraf", "Sustain", "Meend"};
  if (!gfx_touches(0, 0, SCREEN_W, 42)) {
    return;
  }
  int tempo = cur.learn && (cur.lesson == L_ALANKAR || cur.lesson == L_TAAL);
  for (int k = 0; k < 4; k++) {
    int x = 30 + 60 * k, on = cur.active_knob == k;
    int val = k == 3 && tempo ? cur.tempo : cur.knob[k];
    int end = 640 + (768 * val) / 127;
    gfx_arc(x * 16, 16 * 16, 10 * 16, 48, 640, 640 + 768, DUSK, 256);
    if (on) {
      gfx_glow(x, 16, 18, MARIGOLD, 50);
    }
    gfx_arc(x * 16, 16 * 16, 10 * 16, 48, 640, end > 641 ? end : 641, on ? WARM : MARIGOLD, 256);
    int ex = x * 16 + ((10 * 16 * isin(end)) >> 14), ey = 16 * 16 - ((10 * 16 * icos(end)) >> 14);
    gfx_disc(ex, ey, 40, on ? WARM : MARIGOLD, 256);
    gfx_text_c(&F_SMALL, x, 39, k == 3 && tempo ? "Tempo" : NAME[k], on ? IVORY : MIST, 256);
  }
}

static void draw_band(void) {
  if (!gfx_touches(0, 206, SCREEN_W, SCREEN_H)) {
    return;
  }
  const thaat_t *t = &THAAT[cur.thaat];
  char buf[24];
  if (cur.learn) { /* the lesson at the left (PRESETS, SELECT), what to do at the right (PLAY) */
    gfx_text(&F_MED, 10, 225, learn_item_name(buf), IVORY, 256);
    gfx_text(&F_SMALL, 10, 238, LESSON_NAME[cur.lesson], MIST, 256);
  } else {
    gfx_text(&F_MED, 10, 225, t->name, IVORY, 256);
    gfx_text(&F_SMALL, 10, 238, t->western, MIST, 256);
  }
  gfx_text(&F_MED, 112, 225, NOTE[cur.sa], IVORY, 256);
  gfx_text(&F_SMALL, 112, 238, "Sa", MIST, 256);
  if (cur.learn) {
    gfx_text(&F_MED, 152, 225, "Learn", MARIGOLD, 256);
    gfx_text(&F_SMALL, 152, 238, learn_status(buf), MIST, 256);
  } else {
    gfx_text(&F_MED, 152, 225, cur.mode == MODE_SAROD ? "Sarod" : "Chord", IVORY, 256);
    gfx_text(&F_SMALL, 152, 238, cur.jhala ? "jhala on" : cur.drone ? "drone on" : "drone off", MIST, 256);
  }
  /* the tanpura's four strings: lit as each is plucked */
  for (int s = 0; s < 4; s++) {
    int x = (216 + s * 5) * 16 + 8;
    uint32_t c = cur.drone ? mix(IVORY, MARIGOLD, cur.dstr[s] * 16) : DUSK;
    int a = cur.drone ? 110 + cur.dstr[s] * 9 : 256;
    if (cur.drone && cur.dstr[s] > 2) {
      gfx_glow(216 + s * 5, 224, 7, MARIGOLD, cur.dstr[s] * 6);
    }
    gfx_line(x, 211 * 16, x, 237 * 16, 22, c, a > 256 ? 256 : a);
  }
}

static void draw_bead(int s) {
  int bx = bead_x[s], by = bead_y[s], px = bx >> 4, py = by >> 4;
  if (!gfx_touches(px - 24, py - 24, px + 25, py + 25)) {
    return;
  }
  int in = cur.mask >> s & 1, p = cur.play[s], y = cur.symp[s];
  if (s == cur.target) { /* the next note to play: a marigold halo */
    gfx_glow(px, py, 21, MARIGOLD, 120);
  }
  if (y) { /* a sympathetic string answering: a rose aura */
    gfx_glow(px, py, 20, ROSE, y * 12);
    gfx_ring(bx, by, BEAD_R * 16 + 40, 26, ROSE, y * 15 > 240 ? 240 : y * 15);
  }
  if (p) {
    gfx_glow(px, py, 21, MARIGOLD, p * 13);
  }
  if (cur.ripple[s]) {
    int r = BEAD_R * 16 + cur.ripple[s] * 72;
    gfx_ring(bx, by, r, 24, MARIGOLD, 220 - cur.ripple[s] * 70);
  }
  uint32_t ink;
  if (s == 0 || in) {
    uint32_t base = s == 0 ? MARIGOLD : IVORY;
    gfx_disc(bx, by, BEAD_R * 16, p ? mix(base, WARM, p * 12) : base, 256);
    ink = INK;
  } else {
    gfx_disc(bx, by, BEAD_R * 16, p ? mix(INK, MARIGOLD, p * 10) : INK, 220);
    gfx_ring(bx, by, BEAD_R * 16 - 8, 18, p ? MARIGOLD : DUSK, 256);
    ink = p ? IVORY : MIST;
  }
  if (s == cur.vadi) { /* the raga's king note: a marigold crown */
    gfx_ring(bx, by, BEAD_R * 16 + 40, 30, MARIGOLD, 256);
  } else if (s == cur.samv) { /* its queen: an ivory one */
    gfx_ring(bx, by, BEAD_R * 16 + 40, 24, IVORY, 220);
  }
  if (s == cur.target) {
    gfx_ring(bx, by, BEAD_R * 16 + 44, 34, WARM, 256);
  }
  const glyph_t *g = &DEV_S[SYL[s]];
  int gx = px - g->w / 2 - g->x, gy = py - g->y - (g->h + 1) / 2;
  if (MARK[s] == 1) {
    gy -= 2;
  }
  gfx_image(g, DEV_S_data, gx, gy, ink, 256);
  if (MARK[s] == 1) { /* komal: a line below */
    gfx_line((px - 4) * 16, (py + 6) * 16 + 4, (px + 4) * 16, (py + 6) * 16 + 4, 18, ink, 256);
  } else if (MARK[s] == 2) { /* tivra: a stroke above */
    gfx_line(px * 16 + 8, (py - 9) * 16, px * 16 + 8, (py - 6) * 16, 18, ink, 256);
  }
}

static void draw_letter(int s) {
  int ang = bead_ang(s);
  int lx = CX + ((LETTER_R * isin(ang)) >> 14), ly = CY - ((LETTER_R * icos(ang)) >> 14);
  if (!gfx_touches(lx - 12, ly - 10, lx + 12, ly + 10)) {
    return;
  }
  gfx_text_c(&F_SMALL, lx, ly + 4, NOTE[(cur.sa + s) % 12], cur.play[s] ? IVORY : mix(MIST, DUSK, 90), 256);
}

/* the chord's shape: its notes joined around the circle */
static void draw_shape(void) {
  if (!cur.chord_pcs || !gfx_touches(CX - 70, CY - 70, CX + 70, CY + 70)) {
    return;
  }
  int first = -1, prev = -1;
  for (int s = 0; s < 12; s++) {
    if (cur.chord_pcs >> s & 1) {
      if (prev >= 0) {
        gfx_line(bead_x[prev], bead_y[prev], bead_x[s], bead_y[s], 30, MARIGOLD, 190);
      } else {
        first = s;
      }
      prev = s;
    }
  }
  if (first >= 0 && prev != first) {
    gfx_line(bead_x[prev], bead_y[prev], bead_x[first], bead_y[first], 30, MARIGOLD, 190);
  }
}

/* centre text with a soft dark edge, readable over the chord's lines */
#define HALO 0x0f1132u
static int text_h(const font_t *f, int x, int y, const char *s, uint32_t c) {
  gfx_text(f, x - 1, y, s, HALO, 150);
  gfx_text(f, x + 1, y, s, HALO, 150);
  gfx_text(f, x, y - 1, s, HALO, 150);
  gfx_text(f, x, y + 1, s, HALO, 150);
  return gfx_text(f, x, y, s, c, 256);
}
static void text_hc(const font_t *f, int cx, int y, const char *s, uint32_t c) {
  text_h(f, cx - gfx_text_width(f, s) / 2, y, s, c);
}

static void center_note(void) {
  int n = cur.note, pc = n % 12, rel = (pc - cur.sa + 12) % 12, in = cur.mask >> rel & 1;
  char oct[4];
  format_int(oct, n / 12 - 1);
  int w = gfx_text_width(&F_BIG, NOTE[pc]) + 2 + gfx_text_width(&F_MED, oct);
  int x = gfx_text(&F_BIG, CX - w / 2, CY - 11, NOTE[pc], IVORY, 256);
  gfx_text(&F_MED, x + 2, CY - 27, oct, MIST, 256);
  /* the sargam: Devanagari and its name */
  const glyph_t *g = &DEV_L[SYL[rel]];
  int w2 = g->adv + 6 + gfx_text_width(&F_BODY, SARGAM[rel]), x2 = CX - w2 / 2;
  gfx_image(g, DEV_L_data, x2, CY + 15, MARIGOLD, 256);
  int gx = x2 + g->x, gw = g->w;
  if (MARK[rel] == 1) {
    gfx_line((gx + 1) * 16, (CY + 19) * 16, (gx + gw - 1) * 16, (CY + 19) * 16, 22, MARIGOLD, 256);
  } else if (MARK[rel] == 2) {
    gfx_line((gx + gw / 2) * 16, (CY - 5) * 16, (gx + gw / 2) * 16, (CY + 0) * 16, 22, MARIGOLD, 256);
  }
  gfx_text(&F_BODY, x2 + g->adv + 6, CY + 14, SARGAM[rel], IVORY, 256);
  if (cur.verdict == 1) {
    gfx_text_c(&F_SMALL, CX, CY + 31, "right!", MARIGOLD, 256);
  } else if (in) {
    gfx_text_c(&F_SMALL, CX, CY + 31, INTERVAL[rel], MIST, 256);
  } else {
    char s[32];
    cat(cat(s, "not in "), cur.learn && cur.lesson == L_RAGA ? RAGA[cur.litem].name : THAAT[cur.thaat].name);
    gfx_text_c(&F_SMALL, CX, CY + 31, s, ROSE, 256);
  }
}

static void center_chord(void) {
  char s[24];
  if (cur.kind == 255) { /* two notes: the interval between them */
    cat(cat(cat(s, NOTE[cur.root]), " to "), NOTE[cur.bass]);
    text_hc(&F_MED, CX, CY - 4, s, IVORY);
    text_hc(&F_SMALL, CX, CY + 14, cur.interval ? INTERVAL[cur.interval] : "octave", MARIGOLD);
    char n[12];
    format_int(n, cur.interval);
    cat(cat(s, n), cur.interval == 1 ? " semitone" : " semitones");
    text_hc(&F_SMALL, CX, CY + 28, s, MIST);
    return;
  }
  if (cur.kind == 254) {
    text_hc(&F_MED, CX, CY + 2, "no common name", MIST);
    return;
  }
  const chord_t *c = &CHORD[cur.kind];
  char q[16];
  char *e = cat(q, c->sym);
  if (cur.bass != cur.root) {
    cat(cat(e, "/"), NOTE[cur.bass]);
  }
  int w = gfx_text_width(&F_BIG, NOTE[cur.root]) + 2 + gfx_text_width(&F_MED, q);
  int x = text_h(&F_BIG, CX - w / 2, CY - 4, NOTE[cur.root], IVORY);
  text_h(&F_MED, x + 2, CY - 4, q, MARIGOLD);
  text_hc(&F_SMALL, CX, CY + 14, c->what, IVORY);
  text_hc(&F_SMALL, CX, CY + 28, c->feel, MIST);
}

static void tip_text(int tip, const char **title, const char **a, const char **b, char *buf) {
  static const char *const KT[4][3] = {{"Tone", "how bright", "the pluck is"},
                                       {"Taraf", "strings that ring", "in sympathy"},
                                       {"Sustain", "how long", "each note rings"},
                                       {"Meend", "hold a key, press", "another: glide"}};
  static const char *const RV[3][3] = {{"Hall", "reverb: a warm,", "open space"},
                                       {"Room", "reverb: a small", "quiet room"},
                                       {"Dry", "no reverb,", "just the strings"}};
  *a = *b = "";
  switch (tip) {
    case TIP_TONE:
    case TIP_TARAF:
    case TIP_SUSTAIN:
    case TIP_MEEND:
      *title = KT[tip][0], *a = KT[tip][1], *b = KT[tip][2];
      break;
    case TIP_THAAT:
      *title = THAAT[cur.thaat].name, *a = "parent scale of", *b = THAAT[cur.thaat].raga;
      break;
    case TIP_SA:
      cat(cat(buf, "Sa is "), NOTE[cur.sa]);
      *title = buf, *a = "the home note:", *b = "the drone follows";
      break;
    case TIP_MODE:
      *title = cur.mode == MODE_SAROD ? "Sarod" : "Chord";
      *a = cur.mode == MODE_SAROD ? "one string that" : "hold keys to see";
      *b = cur.mode == MODE_SAROD ? "slides (meend)" : "a chord's shape";
      break;
    case TIP_DRONE:
      *title = cur.drone ? "Drone on" : "Drone off";
      *a = cur.drone ? "tanpura strings:" : "PLAY starts";
      *b = cur.drone ? "play over them" : "the tanpura";
      break;
    case TIP_OCTAVE: {
      char n[8];
      format_int(n, 3 + cur.octave);
      cat(cat(buf, "keys start at F"), n);
      *title = "Octave", *a = buf;
      break;
    }
    case TIP_DRONE_LVL: {
      char n[8];
      format_int(n, saved.drone_lvl);
      cat(cat(buf, n), " of 8");
      *title = "Drone level", *a = buf;
      break;
    }
    case TIP_REVERB:
      *title = RV[cur.reverb][0], *a = RV[cur.reverb][1], *b = RV[cur.reverb][2];
      break;
    case TIP_SAVED:
      *title = "Saved", *a = "your settings stay", *b = "after power-off";
      break;
    case TIP_LOAD: {
      char n[12];
      format_int(n, (int32_t)load_avg);
      char *e = cat(cat(buf, "busy "), n);
      format_int(n, (int32_t)load_max);
      cat(cat(cat(cat(e, "%, peak "), n), "%"), "");
      *title = "Sound engine", *a = buf;
      {
        static char kb[32];
        char m[12];
        format_int(m, (int32_t)((gap_show + 500u) / 1000u));
        cat(cat(cat(kb, "keys read every "), m), " ms");
        *b = kb;
      }
      break;
    }
    case TIP_LEARN:
      *title = cur.learn ? "Learn" : "Play";
      *a = cur.learn ? "PRESETS: lesson" : "back to playing";
      *b = cur.learn ? "SELECT: choose" : "";
      break;
    case TIP_LESSON: {
      static const char *const LA[LESSONS] = {"hear a raga,", "a note plays:", "practice patterns", "rhythm cycles"};
      static const char *const LB[LESSONS] = {"then echo it", "find it by ear", "in time", "on the tabla"};
      *title = LESSON_NAME[cur.lesson], *a = LA[cur.lesson], *b = LB[cur.lesson];
      break;
    }
    case TIP_ITEM:
      if (cur.lesson == L_RAGA) {
        *title = RAGA[cur.litem].name, *a = RAGA[cur.litem].thaat, *b = RAGA[cur.litem].time;
      } else if (cur.lesson == L_FIND) {
        *title = LEVEL_NAME[cur.litem], *a = "the notes it", *b = "may play";
      } else if (cur.lesson == L_ALANKAR) {
        *title = ALANKAR_NAME[cur.litem], *a = ALANKAR_EG[cur.litem];
      } else {
        *title = TAAL[cur.litem].name, *a = TAAL[cur.litem].shape;
      }
      break;
    case TIP_TEMPO: {
      char n[12];
      format_int(n, (int32_t)learn_tempo());
      cat(cat(buf, n), " beats a minute");
      *title = "Tempo", *a = buf;
      break;
    }
    case TIP_SCALE:
      if (scale_turn) {
        *title = "Your turn", *a = "play it back", *b = "REC: hear it again";
      } else if (scale_on) {
        *title = THAAT[cur.thaat].name, *a = "up and back down", *b = "KNOB4: tempo";
      } else {
        *title = "Scale off", *a = "REC plays the", *b = "scale for you";
      }
      break;
    case TIP_JHALA:
      *title = cur.jhala ? "Jhala on" : "Jhala off";
      *a = cur.jhala ? "hold a note: high" : "ARP turns on";
      *b = cur.jhala ? "strings strum" : "the strumming";
      break;
  }
}

/* Learn mode's centre */
static void center_three(const char *title, const char *a, const char *b, uint32_t bcol) {
  text_hc(&F_MED, CX, CY - 12, title, IVORY);
  text_hc(&F_SMALL, CX, CY + 5, a, MIST);
  text_hc(&F_SMALL, CX, CY + 19, b, bcol);
}
static void center_sargam(int pc, const char *line, uint32_t col) { /* a big Devanagari note, its name, a line */
  const glyph_t *g = &DEV_L[SYL[pc]];
  gfx_image(g, DEV_L_data, CX - g->w / 2 - g->x, CY + 2, col, 256);
  int gx = CX - g->w / 2, gw = g->w;
  if (MARK[pc] == 1) {
    gfx_line((gx + 1) * 16, (CY + 6) * 16, (gx + gw - 1) * 16, (CY + 6) * 16, 22, col, 256);
  } else if (MARK[pc] == 2) {
    gfx_line((gx + gw / 2) * 16, (CY - 18) * 16, (gx + gw / 2) * 16, (CY - 13) * 16, 22, col, 256);
  }
  text_hc(&F_BODY, CX, CY + 22, SARGAM[pc], IVORY);
  text_hc(&F_SMALL, CX, CY + 37, line, MIST);
}
static void center_learn(void) {
  char a[32], b[32], n1[12], n2[12];
  switch (cur.lesson) {
    case L_RAGA: {
      const raga_t *r = &RAGA[cur.litem];
      if (cur.lstate == S_ECHO) {
        if (cur.hint != 255) {
          cat(cat(b, "next: "), SARGAM[cur.hint]);
        } else {
          format_int(n1, cur.n1), format_int(n2, cur.n2);
          cat(cat(cat(b, n1), " of "), n2);
        }
        center_three("Your turn", "play the pakad", b, cur.hint != 255 ? ROSE : MARIGOLD);
      } else if (cur.lstate == S_DONE) {
        format_int(n1, cur.n1), format_int(n2, cur.n2);
        cat(cat(cat(a, "all "), n1), " notes");
        if (cur.n2) {
          cat(cat(b, n2), cur.n2 == 1 ? " slip" : " slips");
        }
        center_three(cur.n2 ? "Well played" : "Beautiful", a, cur.n2 ? b : "no slips", MARIGOLD);
      } else {
        center_three(r->name, r->time, r->mood, MIST);
      }
      break;
    }
    case L_FIND:
      if (cur.lstate == S_WAIT) {
        text_hc(&F_BIG, CX, CY + 8, "?", IVORY);
        text_hc(&F_SMALL, CX, CY + 28, "play it back", MIST);
      } else if (cur.lstate == S_WRONG) {
        center_three("Not quite", "you played", cur.n2 < 12 ? SARGAM[cur.n2] : "", ROSE);
      } else {
        center_three("Listen", "PLAY: hear it", LEVEL_NAME[cur.litem], MARIGOLD);
      }
      break;
    case L_ALANKAR:
      if (cur.lstate == S_COUNT) {
        format_int(n1, cur.count);
        text_hc(&F_BIG, CX, CY + 14, n1, IVORY);
      } else if (cur.lstate == S_LISTEN) {
        center_sargam(cur.n1, "listen", MARIGOLD);
      } else if (cur.lstate == S_YOURS) {
        format_int(n1, cur.n2), format_int(n2, alk_n);
        cat(cat(cat(b, n1), " of "), n2);
        center_sargam(cur.n1, b, MARIGOLD);
      } else if (cur.lstate == S_DONE) {
        format_int(n1, cur.n1);
        char *pe = n1;
        while (*pe) {
          pe++;
        }
        cat(pe, "% in time");
        format_int(n2, cur.n2), format_int(a, alk_n);
        cat(cat(cat(cat(b, n2), " of "), a), " notes");
        center_three(n1, b, "PLAY: again", MARIGOLD);
      } else {
        center_three(ALANKAR_NAME[cur.litem], ALANKAR_EG[cur.litem], "PLAY: start", MARIGOLD);
      }
      break;
    default: {
      const taal_t *tl = &TAAL[cur.litem];
      if (cur.lstate != S_PLAYING) {
        center_three(tl->name, tl->shape, "PLAY: start", MARIGOLD);
        break;
      }
      /* the cycle: a dot a beat, sam (beat one) marigold, khali hollow, the beat now large */
      for (int i = 0; i < tl->len; i++) {
        int ang = i * 1024 / tl->len;
        int x = CX * 16 + ((40 * 16 * isin(ang)) >> 14), y = CY * 16 - ((40 * 16 * icos(ang)) >> 14);
        char m = tl->mark[i];
        if (i == cur.tbeat) {
          gfx_glow(x >> 4, y >> 4, 11, MARIGOLD, 170);
          gfx_disc(x, y, 80, m == 'X' || (m == '0' && i == 0) ? MARIGOLD : WARM, 256);
        } else if (m == 'X' || (m == '0' && i == 0)) {
          gfx_disc(x, y, 56, MARIGOLD, 256);
        } else if (m == '0') {
          gfx_ring(x, y, 48, 22, IVORY, 220);
        } else if (m != ' ') {
          gfx_disc(x, y, 48, IVORY, 230);
        } else {
          gfx_disc(x, y, 30, MIST, 200);
        }
      }
      text_hc(&F_MED, CX, CY + 4, BOL[cur.tbol], IVORY);
      format_int(n1, cur.tbeat + 1), format_int(n2, tl->len);
      cat(cat(cat(b, n1), " of "), n2);
      text_hc(&F_SMALL, CX, CY + 20, b, MIST);
      break;
    }
  }
}

static void center_tip(void) {
  const char *title, *a, *b;
  char buf[32];
  tip_text(cur.tip, &title, &a, &b, buf);
  gfx_text_c(&F_MED, CX, CY - 12, title, IVORY, 256);
  gfx_text_c(&F_SMALL, CX, CY + 5, a, MIST, 256);
  gfx_text_c(&F_SMALL, CX, CY + 19, b, MIST, 256);
}

static void center_idle(void) {
  int b = (16384 - icos(cur.breath * 16)) >> 7; /* 0..256: in, then out */
  gfx_glow(CX, CY, 22 + (b * 16 >> 8), mix(ROSE, IVORY, 120), 34 + (b * 46 >> 8));
  gfx_glow(CX, CY, 8 + (b * 8 >> 8), IVORY, 40 + (b * 40 >> 8));
  gfx_text_c(&F_BODY, CX, CY + 5, "be here now", IVORY, 200 + (b * 56 >> 8));
  gfx_text_c(&F_SMALL, CX, CY + 22, cur.breath < 32 ? "breathe in" : "breathe out", MIST, 140 + (b * 60 >> 8));
}

/* ---- for Sylmira: the mala becomes a friendship bracelet */
static const uint32_t PASTEL[6] = {0xf7b7d2u, 0xc9b6f2u, 0xa9d8f5u, 0xb8ebd0u, 0xfbe7a1u, 0xffc9a8u};
static const char *const BRACELET[12] = {"S", "Y", "L", "M", "I", "R", "A", "\x03", "1", "3", "\x04", "\x03"};

static void draw_bracelet(void) {
  gfx_ring(CX * 16, CY * 16, RING_R * 16, 26, 0xc9b6f2u, 230); /* the cord */
  for (int s = 0; s < 12; s++) {
    int bx = bead_x[s], by = bead_y[s], px = bx >> 4, py = by >> 4;
    if (!gfx_touches(px - 30, py - 30, px + 31, py + 31)) {
      continue;
    }
    uint32_t c = PASTEL[(s + cur.twinkle / 6) % 6];
    gfx_glow(px, py, 22, c, 60);
    gfx_disc(bx, by, (BEAD_R + 1) * 16, c, 256);
    gfx_disc(bx - 50, by - 50, 40, 0xffffffu, 140); /* a glossy highlight */
    int w = gfx_text_width(&F_MED, BRACELET[s]);
    gfx_text(&F_MED, px - w / 2, py + 6, BRACELET[s], s >= 7 && s != 8 && s != 9 ? 0xd94f86u : INK, 256);
  }
}

static uint32_t hash32(uint32_t v) {
  v ^= v >> 16, v *= 0x45d9f3bu, v ^= v >> 16, v *= 0x45d9f3bu, v ^= v >> 16;
  return v;
}

static void center_egg(void) {
  gfx_glow(CX, CY, 46, 0xf7b7d2u, 60);
  for (int i = 0; i < 9; i++) { /* sparkles that come and go */
    uint32_t cyc = (cur.twinkle + i * 11u) / 16u, h = hash32(i * 977u + cyc * 131u);
    int ph = (cur.twinkle + i * 11) % 16, a = ph < 8 ? ph * 32 : (16 - ph) * 32;
    int ang = (int)(h & 1023), r = 14 + (int)(h >> 10) % 32;
    int x = CX + ((r * isin(ang)) >> 14), y = CY - ((r * icos(ang)) >> 14);
    gfx_glow(x, y, 6, PASTEL[i % 6], a);
    gfx_disc(x * 16 + 8, y * 16 + 8, 10 + ph % 3 * 4, 0xffffffu, a);
  }
  text_hc(&F_MED, CX, CY + 1, "for Sylmira", 0xffd1e3u);
  text_hc(&F_SMALL, CX, CY + 18, "with love", IVORY);
}

static void scene(void) {
  gfx_background();
  if (cur.egg && gfx_touches(CX - 90, CY - 90, CX + 91, CY + 91)) {
    draw_bracelet();
    if (gfx_touches(CX - 52, CY - 52, CX + 53, CY + 53)) {
      center_egg();
    }
  } else if (gfx_touches(CX - 90, CY - 90, CX + 91, CY + 91)) {
    gfx_ring(CX * 16, CY * 16, RING_R * 16, 20, DUSK, 256);
    if (cur.arc > 0) {
      gfx_arc(CX * 16, CY * 16, RING_R * 16, 56, 0, cur.arc, MARIGOLD, 170);
    }
    draw_shape();
    if (cur.center != C_IDLE && gfx_touches(CX - 52, CY - 52, CX + 53, CY + 53)) {
      gfx_glow(CX, CY, 50, 0x111338u, 150); /* a quiet pool for the words */
    }
    for (int s = 0; s < 12; s++) {
      draw_bead(s);
      draw_letter(s);
    }
    if (cur.comet >= 0) {
      int x = CX * 16 + ((RING_R * 16 * isin(cur.comet)) >> 14), y = CY * 16 - ((RING_R * 16 * icos(cur.comet)) >> 14);
      gfx_glow(x >> 4, y >> 4, 12, MARIGOLD, 210);
      gfx_disc(x, y, 56, WARM, 256);
    }
    if (gfx_touches(CX - 52, CY - 52, CX + 53, CY + 53)) {
      switch (cur.center) {
        case C_IDLE:
          center_idle();
          break;
        case C_NOTE:
          center_note();
          break;
        case C_CHORD:
          center_chord();
          break;
        case C_LEARN:
          center_learn();
          break;
        default:
          center_tip();
          break;
      }
    }
  }
  draw_gauges();
  draw_band();
}

/* ---- what changed since the last picture: mark only that for repainting */

static void dirty_bead(int s, int letter) {
  int px = bead_x[s] >> 4, py = bead_y[s] >> 4;
  gfx_dirty(px - 22, py - 22, px + 23, py + 23);
  if (letter) {
    int ang = bead_ang(s), lx = CX + ((LETTER_R * isin(ang)) >> 14), ly = CY - ((LETTER_R * icos(ang)) >> 14);
    gfx_dirty(lx - 12, ly - 10, lx + 12, ly + 10);
  }
}
static void dirty_ring_point(int ang, int pad) {
  int x = CX + ((RING_R * isin(ang)) >> 14), y = CY - ((RING_R * icos(ang)) >> 14);
  gfx_dirty(x - pad, y - pad, x + pad + 1, y + pad + 1);
}
/* the part of the ring between two angles (0..1023, a0 <= a1): the box around its ends and any quarter-points between */
static void dirty_arc(int a0, int a1) {
  int x0 = 999, y0 = 999, x1 = -999, y1 = -999;
  for (int k = 0; k < 6; k++) {
    int ang = k == 0 ? a0 : k == 1 ? a1 : (k - 2) * 256;
    if (k >= 2 && (ang < a0 || ang > a1)) {
      continue;
    }
    int x = CX + ((RING_R * isin(ang)) >> 14), y = CY - ((RING_R * icos(ang)) >> 14);
    x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
  }
  gfx_dirty(x0 - 6, y0 - 6, x1 + 7, y1 + 7);
}
static void dirty_center(void) { gfx_dirty(CX - 52, CY - 52, CX + 53, CY + 53); }
static void dirty_wheel(void) { gfx_dirty(CX - 92, CY - 92, CX + 93, CY + 93); }

static void repaint_changes(void) {
  view_t *a = &cur, *b = &drawn;
  if (a->thaat != b->thaat || a->sa != b->sa || a->egg != b->egg || a->learn != b->learn || a->lesson != b->lesson ||
      a->litem != b->litem || a->mask != b->mask || a->vadi != b->vadi || a->samv != b->samv) {
    gfx_dirty_all();
    *b = *a;
    return;
  }
  for (int k = 0; k < 4; k++) {
    if (a->knob[k] != b->knob[k] || (a->active_knob == k) != (b->active_knob == k) || (k == 3 && a->tempo != b->tempo)) {
      gfx_dirty(k * 60, 0, k * 60 + 60, 42);
    }
  }
  if (a->target != b->target) {
    if (b->target >= 0) {
      dirty_bead(b->target, 0);
    }
    if (a->target >= 0) {
      dirty_bead(a->target, 0);
    }
  }
  if (a->lstate != b->lstate || a->phase != b->phase || (a->lesson == L_FIND && a->n1 != b->n1) ||
      (a->lesson == L_TAAL && a->tempo != b->tempo)) {
    gfx_dirty(150, 206, SCREEN_W, SCREEN_H); /* the status words */
  }
  if (a->lstate != b->lstate || a->n1 != b->n1 || a->n2 != b->n2 || a->hint != b->hint || a->count != b->count ||
      a->tbeat != b->tbeat || a->verdict != b->verdict) {
    if (a->lesson == L_TAAL && a->lstate == S_PLAYING && b->lstate == S_PLAYING) {
      gfx_dirty(CX - 48, CY - 48, CX + 49, CY + 49); /* the beat ring and the bol */
    } else {
      dirty_center();
    }
  }
  if (a->mode != b->mode || a->drone != b->drone || a->jhala != b->jhala) {
    gfx_dirty(150, 206, SCREEN_W, SCREEN_H);
  }
  for (int s = 0; s < 4; s++) {
    if (a->dstr[s] != b->dstr[s]) {
      gfx_dirty(206 + s * 5, 206, 227 + s * 5, SCREEN_H);
    }
  }
  if (a->chord_pcs != b->chord_pcs) { /* the box around every corner of the old shape and the new one */
    uint16_t m = a->chord_pcs | b->chord_pcs;
    int x0 = 999, y0 = 999, x1 = -999, y1 = -999;
    for (int s = 0; s < 12; s++) {
      if (m >> s & 1) {
        int x = bead_x[s] >> 4, y = bead_y[s] >> 4;
        x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
      }
    }
    gfx_dirty(x0 - 4, y0 - 4, x1 + 5, y1 + 5);
  }
  if (a->arc != b->arc) { /* only the stretch of ring that grew or shrank */
    int o = b->arc > 0 ? b->arc : 0, n = a->arc > 0 ? a->arc : 0;
    dirty_arc(o < n ? o : n, o < n ? n : o);
  }
  if (a->comet != b->comet) { /* the spark: one box over where it was and where it is */
    int pts[2] = {b->comet, a->comet}, x0 = 999, y0 = 999, x1 = -999, y1 = -999;
    for (int k = 0; k < 2; k++) {
      if (pts[k] >= 0) {
        int x = CX + ((RING_R * isin(pts[k])) >> 14), y = CY - ((RING_R * icos(pts[k])) >> 14);
        x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
      }
    }
    gfx_dirty(x0 - 13, y0 - 13, x1 + 14, y1 + 14);
  }
  for (int s = 0; s < 12; s++) {
    if (a->play[s] != b->play[s] || a->symp[s] != b->symp[s] || a->ripple[s] != b->ripple[s]) {
      dirty_bead(s, !a->play[s] != !b->play[s]);
    }
  }
  if (a->egg && a->twinkle != b->twinkle) {
    if (a->twinkle / 6 != b->twinkle / 6) {
      dirty_wheel(); /* the bead colours drift */
    } else {
      dirty_center();
    }
  }
  if (a->center == C_IDLE && b->center == C_IDLE && a->breath != b->breath) {
    gfx_dirty(CX - 46, CY - 39, CX + 47, CY + 40); /* only the breath's light and words */
  }
  if (a->center != b->center || a->tip != b->tip || a->note != b->note ||
      a->root != b->root || a->kind != b->kind || a->bass != b->bass || a->interval != b->interval ||
      a->octave != b->octave || a->reverb != b->reverb) {
    dirty_center();
  }
  *b = *a;
}

/* ================================================================ notes */

static int stack[KEY_COUNT + 16], depth; /* the held notes, oldest first (Sarod mode) */
static uint8_t chord_age[4], chord_voice[4];

static void play_on(int note, int vel) {
  if (saved.mode == MODE_SAROD) {
    int legato = depth > 0 && saved.knob[3] > 0;
    if (depth < (int)(sizeof stack / sizeof stack[0])) {
      stack[depth++] = note;
    }
    lead_target = note * 256;
    lead_held = 1;
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
    chord_voice[pick] = (uint8_t)note;
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
  if (saved.mode == MODE_SAROD) {
    if (depth > 0) {
      lead_target = stack[depth - 1] * 256; /* slide back to the note still held */
    } else {
      lead_held = 0;
    }
  }
}

/* name what is held: fills root, kind, bass, interval */
static void name_chord(const int *notes, int n, view_t *v) {
  uint16_t pcs = 0;
  int low = 127, high = 0;
  for (int i = 0; i < n; i++) {
    pcs |= (uint16_t)(1u << (notes[i] % 12));
    low = notes[i] < low ? notes[i] : low;
    high = notes[i] > high ? notes[i] : high;
  }
  v->bass = (uint8_t)(low % 12);
  for (int t = 0; t < 12; t++) {
    int root = (low + t) % 12;
    if (!(pcs >> root & 1)) {
      continue;
    }
    uint16_t rel = (uint16_t)(((pcs >> root) | (pcs << (12 - root))) & 0xFFF);
    for (int c = 0; c < CHORDS; c++) {
      if (rel == CHORD[c].mask && (n > 2 || c == 13)) {
        v->root = (uint8_t)root, v->kind = (uint8_t)c;
        return;
      }
    }
  }
  if (n == 2 || (pcs & (pcs - 1)) == 0) {
    v->kind = 255, v->root = (uint8_t)(low % 12), v->bass = (uint8_t)(high % 12);
    v->interval = (uint8_t)((high - low) % 12);
    return;
  }
  v->kind = 254, v->root = (uint8_t)(low % 12);
}

/* ================================================================ setup / loop */

static void make_string(string_t *s, int16_t *buf, uint32_t len, int32_t a, int32_t fb, int32_t ap, int32_t jaw) {
  *s = (string_t){buf, len - 1, 0, 100u << 16, 0, a, fb, ap, 0, 0, jaw, SLEEP, 0};
}

void setup(void) {
  settings_load(&saved, sizeof saved);
  if (saved.octave < -2 || saved.octave > 2 || saved.thaat >= THAATS || saved.sa > 11 || saved.mode > 1 ||
      saved.drone_lvl > 8 || saved.reverb > 2) {
    saved.octave = 0, saved.thaat = 1, saved.sa = 0, saved.mode = MODE_SAROD, saved.drone_lvl = 4, saved.reverb = 0;
  }
  for (int k = 0; k < 4; k++) {
    saved.knob[k] = saved.knob[k] > 127 ? 64 : saved.knob[k];
  }
  /* the big buffers live in the pool, which the FM-1 may not clear at power-on: start them silent */
  static int16_t *const ZERO[] = {lead_buf, chord_buf[0], drone_buf[0], taraf_buf[0], chik_buf[0], rv_comb, rv_ap, dayan_buf};
  static const uint32_t ZLEN[] = {2048, 4 * 2048, 4 * 1024, 11 * 256, 2 * 256, sizeof rv_comb / 2, sizeof rv_ap / 2, 512};
  for (int z = 0; z < 8; z++) {
    for (uint32_t k = 0; k < ZLEN[z]; k++) {
      ZERO[z][k] = 0;
    }
  }
  make_string(&lead, lead_buf, 2048, 12000, 32500, AP_STIFF, 0);
  for (int v = 0; v < 4; v++) {
    make_string(&chord[v], chord_buf[v], 2048, 12000, 32500, AP_STIFF, 0);
    make_string(&drone[v], drone_buf[v], 1024, 14500, 32730, 0, 2600);
  }
  for (int v = 0; v < 11; v++) {
    make_string(&taraf[v], taraf_buf[v], 256, 11000, TARAF_FB, 0, 0);
  }
  make_string(&chik[0], chik_buf[0], 256, 15000, 32300, AP_STIFF, 0);
  make_string(&chik[1], chik_buf[1], 256, 15000, 32200, AP_STIFF, 0);
  make_string(&dayan, dayan_buf, 512, 14500, 32250, 0, 0);
  scale_mask = THAAT[saved.thaat].mask;
  int16_t *p = rv_comb;
  for (int c = 0; c < 8; c++) {
    comb_at[c] = p, p += COMB_LEN[c];
  }
  p = rv_ap;
  for (int a = 0; a < 4; a++) {
    ap_at[a] = p, p += AP_LEN[a];
  }
  for (int s = 0; s < 12; s++) {
    int ang = bead_ang(s);
    bead_x[s] = CX * 16 + ((RING_R * 16 * isin(ang)) >> 14);
    bead_y[s] = CY * 16 - ((RING_R * 16 * icos(ang)) >> 14);
  }
  lead_target = (60 + saved.sa) * 256;
  retune = 1;
  ready = 1; /* now the sound may start */
  led_backlight(1);
  gfx_splash();
  print("BE HERE NOW: love, serve, remember");
}

void loop(void) {
  { /* how long since the keys were last read (the screen's sending happens between passes) */
    static uint32_t last_us, gap_at;
    uint32_t t = micros();
    if (last_us && t - last_us > gap_max_us) {
      gap_max_us = t - last_us;
    }
    last_us = t;
    if (millis() - gap_at >= 1000) {
      gap_at = millis(), gap_show = gap_max_us, gap_max_us = 0;
    }
  }
  static uint8_t held[KEY_COUNT];
  static uint32_t changed_at, tip_until, note_until, last_frame;
  static int dirty, ui, tip = -1, knob_on = -1;
  static uint32_t seen_lead, seen_chord[4];
  static uint8_t ripple[12];
  static view_t keep; /* the last note or chord shown, held while it rings */
  int base = KEY_FIRST_NOTE + 12 * saved.octave;
  uint8_t st, d1, d2;
  uint32_t now = millis();

  /* the keys, and MIDI notes from a computer or a keyboard */
  int key_hit = 0;
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_pressed(k)) {
      key_hit = 1;
      if (scale_note) { /* your turn: let go of the scale's note first, so yours is plucked, not slid to */
        play_off(scale_note), scale_note = 0;
      }
      held[k] = (uint8_t)(base + k);
      play_on(held[k], 100);
      midi_send(0x90, held[k], 100);
      if (learn) {
        learn_key(held[k], now);
      }
    }
    if (key_released(k)) {
      play_off(held[k]);
      midi_send(0x80, held[k], 0);
    }
  }
  static uint8_t cc_n, cc_num[32], cc_val[32]; /* control changes, applied with the other controls below */
  static uint16_t coach_blink, coach_lit;           /* the Coach's key lights, pitch classes from C */
  static uint32_t coach_until;
  while (midi_read(&st, &d1, &d2)) {
    if ((st & 0xF0) == 0x90 && d2) {
      play_on(d1, d2);
    } else if ((st & 0xF0) == 0x80 || (st & 0xF0) == 0x90) {
      play_off(d1);
    } else if ((st & 0xF0) == 0xB0 && cc_n < 32) {
      cc_num[cc_n] = d1, cc_val[cc_n] = d2, cc_n++;
    }
  }

  /* the start screen stays 3 s, or until a key is played */
  if (!ui) {
    if (now < 3000 && !keys()) {
      return;
    }
    ui = 1;
    gfx_dirty_all();
    drawn.thaat = 255;
  }

  /* the controls */
  int changed = 0, t = -1;
  int oct = saved.octave + button_pressed(BTN_OCT_UP) - button_pressed(BTN_OCT_DOWN);
  if (oct != saved.octave && oct >= -2 && oct <= 2) {
    saved.octave = (int8_t)oct, changed = 1, t = TIP_OCTAVE;
  }
  /* the Coach setting things up */
  int seq_toggle = button_pressed(BTN_SEQ);
  for (int i = 0; i < cc_n; i++) {
    int v = cc_val[i];
    switch (cc_num[i]) {
      case 20:
        if (v < 12 && v != saved.sa) {
          saved.sa = (uint8_t)v, retune = 1, changed = 1, t = TIP_SA;
        }
        break;
      case 21:
        if (v < THAATS && v != saved.thaat) {
          saved.thaat = (uint8_t)v, retune = 1, changed = 1, t = TIP_THAAT;
        }
        break;
      case 22:
        if ((v >= 64) != drone_on) {
          drone_on = v >= 64, t = TIP_DRONE;
        }
        break;
      case 23:
        if ((v >= 64 ? MODE_CHORD : MODE_SAROD) != saved.mode && !learn) {
          saved.mode = v >= 64 ? MODE_CHORD : MODE_SAROD, depth = 0, lead_held = 0, changed = 1, t = TIP_MODE;
        }
        break;
      case 24:
        if ((v >= 64) != learn) {
          seq_toggle = 1;
        }
        break;
      case 25:
        coach_blink = (uint16_t)((coach_blink & 0xF80) | (v & 0x7F));
        break;
      case 26:
        coach_blink = (uint16_t)((coach_blink & 0x07F) | (v & 0x1F) << 7);
        break;
      case 27:
        coach_lit = (uint16_t)((coach_lit & 0xF80) | (v & 0x7F));
        break;
      case 28:
        coach_lit = (uint16_t)((coach_lit & 0x07F) | (v & 0x1F) << 7);
        break;
      case 29:
        coach_until = v >= 64 ? now + 3000 : 0;
        break;
    }
  }
  cc_n = 0;
  if (seq_toggle) { /* Learn mode on or off */
    learn_stop();
    if (!learn) {
      learn = 1, learn_drone_was = drone_on, learn_mode_was = saved.mode;
      drone_on = 1, saved.mode = MODE_SAROD, depth = 0, lead_held = 0;
    } else {
      learn = 0, drone_on = learn_drone_was, saved.mode = learn_mode_was;
    }
    t = TIP_LEARN;
  }
  int32_t e = encoder(ENC_PRESETS);
  if (e && learn) {
    learn_stop();
    lesson = (uint8_t)((lesson + LESSONS + (e > 0 ? 1 : -1)) % LESSONS);
    t = TIP_LESSON;
  } else if (e) {
    saved.thaat = (uint8_t)((saved.thaat + THAATS + (e > 0 ? 1 : -1)) % THAATS);
    midi_send(0xB0, 21, saved.thaat); /* tell the Coach */
    retune = 1, changed = 1, t = TIP_THAAT;
  }
  e = encoder(ENC_ALGORITHM);
  if (e) {
    saved.sa = (uint8_t)((saved.sa + 12 + (e > 0 ? 1 : -1)) % 12);
    midi_send(0xB0, 20, saved.sa);
    retune = 1, changed = 1, t = TIP_SA;
  }
  e = encoder(ENC_SELECT);
  if (e && learn) {
    static const uint8_t COUNT[LESSONS] = {RAGAS, LEVELS, ALANKARS, TAALS};
    learn_stop();
    litem[lesson] = (uint8_t)((litem[lesson] + COUNT[lesson] + (e > 0 ? 1 : -1)) % COUNT[lesson]);
    t = TIP_ITEM;
  } else if (e) {
    int l = saved.drone_lvl + (e > 0 ? 1 : -1);
    saved.drone_lvl = (uint8_t)(l < 0 ? 0 : l > 8 ? 8
                                                  : l);
    changed = 1, t = TIP_DRONE_LVL;
  }
  if (button_pressed(BTN_SEL) && !learn) {
    saved.mode ^= 1;
    midi_send(0xB0, 23, saved.mode == MODE_CHORD ? 127 : 0);
    depth = 0, lead_held = 0;
    changed = 1, t = TIP_MODE;
  }
  if (button_pressed(BTN_PLAY)) {
    if (learn) {
      learn_play(now);
    } else {
      drone_on ^= 1;
      midi_send(0xB0, 22, drone_on ? 127 : 0);
      t = TIP_DRONE;
    }
  }
  if (button_pressed(BTN_ARP)) {
    jhala ^= 1;
    t = TIP_JHALA;
  }
  static uint32_t egg_until, egg_next, egg_from;
  static int egg_i;
  if (button_pressed(BTN_HOME)) { /* for Sylmira */
    egg_from = now, egg_until = now + 12000, egg_next = now + 300, egg_i = 0;
  }
  if (egg_until && keys() && now - egg_from > 1500) {
    egg_until = 0; /* playing brings the mala back */
  }
  if (egg_until && now >= egg_until) {
    egg_until = 0;
  }
  if (egg_until && egg_i < 13 && now >= egg_next) {
    /* a little shimmer of my own: up through Sa, Ga, Pa, Ni, Re, and home to a bright chord */
    static const int8_t ARP[13] = {0, 7, 12, 16, 19, 23, 26, 28, 31, 24, 28, 31, 36};
    int v = egg_i % 4, note = 60 + saved.sa + ARP[egg_i];
    chord_note[v] = (uint32_t)note, chord_vel[v] = egg_i >= 9 ? 70 : 85, chord_plucks[v]++;
    egg_next = now + (egg_i < 8 ? 150 : egg_i == 8 ? 260 : 40);
    egg_i++;
  }
  { /* the sound engine's load, each second */
    static uint32_t load_at;
    if (now - load_at >= 1000) {
      load_at = now;
      load_avg = load_blocks ? load_us * 441u / load_blocks / 100u : 0;
      load_max = load_peak / 100u;
      load_us = load_blocks = load_peak = 0;
    }
  }
  if (button_pressed(BTN_GLO)) {
    t = TIP_LOAD;
  }
  if (button_pressed(BTN_SAVE)) {
    settings_save(&saved, sizeof saved);
    dirty = 0, t = TIP_SAVED;
  }
  if (button_pressed(BTN_FX)) {
    saved.reverb = (uint8_t)((saved.reverb + 1) % 3);
    changed = 1, t = TIP_REVERB;
  }
  for (int k = 0; k < 4; k++) {
    int32_t d = encoder(ENC_KNOB1 + k);
    if (d && k == 3 && ((learn && (lesson == L_ALANKAR || lesson == L_TAAL)) || (scale_on && !learn))) { /* KNOB4 is the tempo here */
      int v = tempo_knob + d * 2;
      tempo_knob = (uint8_t)(v < 0 ? 0 : v > 127 ? 127
                                                 : v);
      pulse_spb = 44100u * 60u / learn_tempo();
      t = TIP_TEMPO, knob_on = 3;
    } else if (d) {
      int v = saved.knob[k] + d * 2;
      saved.knob[k] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127
                                                    : v);
      changed = 1, t = TIP_TONE + k, knob_on = k;
    }
  }
  { /* REC: the scale on its own, from the lowest Sa on the keys up to the next Sa and back down */
    static int idx, sig;
    static uint32_t next_at;
    int s = saved.thaat | saved.sa << 4 | (saved.octave + 2) << 8 | saved.mode << 11;
    if (button_pressed(BTN_REC) && !learn) {
      scale_on ^= 1, scale_turn = 0, idx = 0, next_at = now, scale_keys = 0, t = TIP_SCALE;
    } else if (scale_on && (learn || key_hit)) {
      scale_on = 0, scale_turn = key_hit && !learn, scale_keys = 0, t = learn ? t : TIP_SCALE;
    } else if (scale_on && s != sig) { /* a new scale, Sa, octave or mode: start again from Sa */
      idx = 0, next_at = now + 250, scale_keys = 0;
    }
    sig = s;
    if (scale_note && (!scale_on || now >= next_at)) {
      play_off(scale_note), scale_note = 0, scale_now = -1;
    }
    if (scale_on && now >= next_at) {
      uint8_t seq[26];
      int n = 0, lo = base + ((saved.sa - base % 12) % 12 + 12) % 12;
      uint16_t m = THAAT[saved.thaat].mask;
      for (int d = 0; d < 12; d++) {
        if (m >> d & 1) {
          seq[n++] = (uint8_t)(lo + d);
        }
      }
      int up = n + 1;
      seq[n++] = (uint8_t)(lo + 12);
      for (int i = up - 2; i >= 0; i--) {
        seq[n++] = seq[i];
      }
      uint32_t beat = 60000u / learn_tempo();
      if (idx < n) {
        scale_note = seq[idx], scale_now = seq[idx] - base;
        play_on(scale_note, 92);
        scale_keys |= 1u << (scale_now & 31);
        next_at = now + (idx == up - 1 || idx == n - 1 ? 2 * beat : beat); /* the top and the bottom Sa linger */
        idx++;
      } else { /* a breath, then again */
        idx = 0, scale_keys = 0, next_at = now + beat;
      }
    }
  }
  if (t >= 0) {
    tip = t, tip_until = now + 2600;
    if (t > TIP_MEEND && t != TIP_TEMPO) {
      knob_on = -1;
    }
  }
  if (learn) {
    learn_update(now);
  }
  if (learn_mask() != scale_mask) { /* the sympathetic strings follow the raga, or the thaat */
    scale_mask = learn_mask(), retune = 1;
  }
  if (tip >= 0 && now >= tip_until) {
    tip = -1, knob_on = -1;
  }
  if (changed) {
    dirty = 1, changed_at = now;
  }
  led_button(BTN_OCT_DOWN, saved.octave < 0);
  led_button(BTN_OCT_UP, saved.octave > 0);
  led_button(BTN_PLAY, drone_on);
  led_button(BTN_SEL, saved.mode == MODE_CHORD);
  led_button(BTN_ARP, jhala);
  led_button(BTN_FX, saved.reverb != 2);
  led_button(BTN_SEQ, learn);
  led_button(BTN_REC, scale_on);

  /* what is held */
  base = KEY_FIRST_NOTE + 12 * saved.octave;
  int notes[KEY_COUNT], n = 0;
  for (int k = 0; k < KEY_COUNT; k++) {
    if (key_held(k)) {
      notes[n++] = held[k];
    }
    int pc = ((base + k) % 12 - saved.sa + 24) % 12;
    if (scale_on) { /* REC: the keys light up as the scale climbs; the one sounding blinks */
      led_key(k, k == scale_now ? (now / 125 & 1) : (int)(scale_keys >> k & 1));
    } else if (now < coach_until && !learn) { /* the Coach's lesson: its scale steady, the next note blinking */
      int c = (base + k) % 12;
      led_key(k, (coach_blink >> c & 1) ? (now / 250 & 1) : (coach_lit >> c & 1));
    } else {
      led_key(k, scale_mask >> pc & 1);
    }
  }

  /* the picture, at most 30 times a second */
  if (now - last_frame >= 33) {
    last_frame = now;
    view_t v = cur;
    v.thaat = saved.thaat, v.sa = saved.sa, v.mode = saved.mode, v.drone = drone_on, v.reverb = saved.reverb;
    v.jhala = jhala, v.octave = saved.octave, v.active_knob = (int8_t)knob_on;
    for (int k = 0; k < 4; k++) {
      v.knob[k] = saved.knob[k];
    }
    /* the light of what sounds: the lead (or the chord strings), and the sympathetic strings */
    uint16_t play[12] = {0}, symp[12] = {0};
    int lead_pc = -1;
    if (saved.mode == MODE_SAROD) {
      int32_t p = lead_now;
      lead_pc = (((p + 128) >> 8) % 12 - saved.sa + 12) % 12;
      if (p != lead_target && cur.comet >= 0) {
        lead_pc = ((lead_target >> 8) % 12 - saved.sa + 12) % 12; /* sliding: the glow waits where it will land */
      }
      play[lead_pc] = lvl_lead;
      if (lead_struck != seen_lead) {
        seen_lead = lead_struck, ripple[lead_pc] = 1;
      }
    } else {
      for (int c = 0; c < 4; c++) {
        int pc = (chord_voice[c] % 12 - saved.sa + 12) % 12;
        play[pc] = lvl_chord[c] > play[pc] ? lvl_chord[c] : play[pc];
        if (chord_struck[c] != seen_chord[c]) {
          seen_chord[c] = chord_struck[c], ripple[pc] = 1;
        }
      }
    }
    for (int s = 0; s < 11; s++) {
      int pc = taraf_pc[s];
      uint16_t l = lvl_taraf[s];
      symp[pc] = l > symp[pc] ? l : symp[pc];
    }
    uint32_t smax = 1;
    for (int s = 0; s < 12; s++) {
      smax = symp[s] > smax ? symp[s] : smax;
    }
    int sabs = (int)isqrt_u(smax * 6u) / 9; /* how strongly the loudest one rings */
    sabs = sabs > 16 ? 16 : sabs;
    /* the glows change slowly and the screen is slow to send: 8 times a second, in steps of 4, and a glow only
     * moves when it has really changed (a fresh pluck shows at once) */
    static uint32_t slow_at;
    int slow = now - slow_at >= 125;
    if (slow) {
      slow_at = now;
    }
    for (int s = 0; s < 12; s++) {
      /* only the strings that really answer: measured against the loudest, the faint ones stay dark */
      int rel = symp[s] * 5u > smax * 3u ? (int)((symp[s] * 5u - smax * 3u) * 16u / (2u * smax)) : 0;
      int lp = (int)isqrt_u(play[s] * 2u) / 9, ls = rel * sabs / 16;
      lp = lp > 16 ? 16 : lp, ls = ls > 16 ? 16 : ls;
      lp = lp > 8 ? 16 : lp > 2 ? 8 : 0, ls = ls < 4 ? 0 : ls & ~3; /* full, half, off */
      if (lp > v.play[s] || ripple[s] == 1 || (slow && lp != v.play[s])) {
        v.play[s] = (uint8_t)lp;
      }
      if (slow && (ls == 0 || ls > v.symp[s] + 3 || ls + 3 < v.symp[s])) {
        v.symp[s] = (uint8_t)ls;
      }
      v.ripple[s] = (uint8_t)((ripple[s] + 1) / 2); /* 1, 2: a ring that widens over two steps */
      if (ripple[s] && ++ripple[s] > 4) {
        ripple[s] = 0;
      }
    }
    for (int s = 0; s < 4 && slow; s++) {
      int l = (int)isqrt_u(lvl_drone[s]) / 14;
      v.dstr[s] = (uint8_t)(l > 6 ? 8 : l > 2 ? 4 : 0);
    }

    /* the centre: a tip, the note, the chord, or the breath */
    int sounding = saved.mode == MODE_SAROD ? lvl_lead > 900 : (lvl_chord[0] | lvl_chord[1] | lvl_chord[2] | lvl_chord[3]) > 900;
    v.comet = -1, v.arc = -1, v.chord_pcs = 0;
    if (n || sounding) {
      note_until = now + 1500;
    }
    if (saved.mode == MODE_SAROD && (n || now < note_until)) {
      int32_t p = lead_now;
      v.center = C_NOTE;
      v.note = (int16_t)(depth ? stack[depth - 1] : (p + 128) >> 8);
      int rel = (int)((p - saved.sa * 256) % 3072 + 3072) % 3072; /* Q8 semitones above Sa */
      v.arc = (int16_t)(rel / 3 ? rel / 3 : -1);
      if (p != lead_target) {
        static uint32_t spark_at;
        if (now - spark_at >= 50 || cur.comet < 0) { /* a slide in flight, 20 frames a second */
          spark_at = now;
          v.comet = (int16_t)(rel / 3);
        } else {
          v.comet = cur.comet, v.arc = cur.arc;
        }
      }
      keep = v;
    } else if (saved.mode == MODE_CHORD && n) {
      if (n == 1) {
        v.center = C_NOTE, v.note = (int16_t)notes[0];
        int rel = ((notes[0] - saved.sa) % 12 + 12) % 12;
        v.arc = (int16_t)(rel ? bead_ang(rel) : -1);
      } else {
        v.center = C_CHORD;
        name_chord(notes, n, &v);
        for (int i = 0; i < n; i++) {
          v.chord_pcs |= (uint16_t)(1u << ((notes[i] % 12 - saved.sa + 12) % 12));
        }
      }
      keep = v;
    } else if (saved.mode == MODE_CHORD && now < note_until && keep.center >= C_NOTE) {
      v.center = keep.center, v.note = keep.note, v.root = keep.root, v.kind = keep.kind, v.bass = keep.bass;
      v.interval = keep.interval, v.chord_pcs = keep.chord_pcs, v.arc = keep.arc;
    } else {
      v.center = C_IDLE;
      v.breath = (uint8_t)((now % 8000u) * 32u / 8000u * 2u);
    }
    if (tip >= 0) {
      v.center = C_TIP, v.tip = (uint8_t)tip;
    }
    /* Learn mode */
    v.learn = learn, v.lesson = lesson, v.litem = litem[lesson], v.lstate = lstate, v.phase = phase;
    v.mask = learn_mask(), v.tempo = tempo_knob, v.verdict = 0, v.hint = hint_pc, v.count = 0, v.n1 = v.n2 = 0;
    v.vadi = v.samv = -1, v.target = -1, v.tbeat = 0, v.tbol = 0;
    if (learn) {
      if (lesson == L_RAGA) {
        v.vadi = RAGA[litem[L_RAGA]].vadi, v.samv = RAGA[litem[L_RAGA]].samvadi;
        v.n1 = (uint8_t)(lstate == S_DONE ? echo_n : echo_i), v.n2 = (uint8_t)(lstate == S_DONE ? echo_slips : echo_n);
      } else if (lesson == L_FIND) {
        v.n1 = (uint8_t)find_streak, v.n2 = (uint8_t)(find_played < 0 ? 255 : find_played);
      } else if (lesson == L_ALANKAR) {
        uint32_t b = alk_seen;
        v.count = (uint8_t)(b < 5 ? b : 0);
        if (lstate == S_LISTEN && b >= 5) {
          v.n1 = (uint8_t)(degree_semis(alk[b - 5]) % 12);
        } else if (lstate == S_YOURS) {
          int j = (int)b - 5 - alk_n;
          v.n1 = (uint8_t)(j >= 0 && j < alk_n ? degree_semis(alk[j]) % 12 : 0), v.n2 = (uint8_t)(j + 1);
        } else if (lstate == S_DONE) {
          v.n1 = (uint8_t)(alk_n ? alk_hits * 100 / alk_n : 0), v.n2 = (uint8_t)alk_hits;
        }
        v.target = (int8_t)learn_target();
      } else if (lstate == S_PLAYING) {
        uint32_t b = pulse_beats;
        v.tbeat = (uint8_t)(b ? (b - 1) % TAAL[litem[L_TAAL]].len : 0), v.tbol = pulse_bol_now;
      }
      /* the centre: the lesson, except while a raga is demonstrated, or a found note is shown */
      if (lesson == L_RAGA && lstate == S_DEMO) {
        v.center = C_NOTE, v.note = (int16_t)((lead_now + 128) >> 8);
      } else if (lesson == L_FIND && lstate == S_RIGHT) {
        v.center = C_NOTE, v.note = (int16_t)(60 + saved.sa + find_note), v.verdict = 1;
      } else {
        v.center = C_LEARN;
      }
      if (tip >= 0) {
        v.center = C_TIP;
      }
    }
    v.egg = egg_until != 0;
    v.twinkle = (uint8_t)(v.egg ? (now - egg_from) / 66 : 0);
    cur = v;
    repaint_changes();
  }
  /* short screen steps: the keys are read between them, so a pass must never keep a note waiting */
  gfx_render(scene, 2400);
#if defined(__wasm__)
  { /* the emulator reports what the screen costs: rows the FM-1 would send each second */
    extern uint32_t gfx_rows_sent, gfx_px_drawn;
    static uint32_t stat_at;
    if (now - stat_at >= 1000) {
      char b[48], n[12];
      stat_at = now;
      format_int(n, (int32_t)gfx_rows_sent);
      char *e = cat(cat(b, "rows/s "), n);
      format_int(n, (int32_t)gfx_px_drawn);
      cat(cat(e, " px/s "), n);
      print(b);
      gfx_rows_sent = gfx_px_drawn = 0;
    }
  }
#endif

  /* saving stalls the sound for ~50 ms, so it happens on its own only in real silence: nothing held, no drone,
   * every string faded. Otherwise SAVE does it */
  int silent = !n && !drone_on && lvl_lead < 40 && lvl_chord[0] < 40 && lvl_chord[1] < 40 && lvl_chord[2] < 40 && lvl_chord[3] < 40;
  if (dirty && silent && now - changed_at > 3000) {
    settings_save(&saved, sizeof saved);
    dirty = 0;
  }
}
