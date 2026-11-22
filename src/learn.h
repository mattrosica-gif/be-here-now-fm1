/* learn.h: BE HERE NOW's Learn mode (SEQ). Included by main.c, which defines what it uses.
 *
 *   PRESETS   the lesson: Raga studio, Find the note, Alankars, Taal
 *   SELECT    within it: a raga, a level, a pattern, a taal
 *   PLAY      start (and again to stop or replay)
 *   KNOB4     Tempo, in Alankars and Taal
 *
 * Raga data: aroha, avaroha, vadi, samvadi, pakad and time as given by NIOS (Yaman), Wikipedia and the
 * raga pages of tanarang.com, shankarmahadevanacademy.com and indianclassicalmusic.com; where sources
 * differ (Kafi's samvadi), the more common reading is used. */

/* ================================================================ the lessons' data */

#define END 127
#define SL(x) ((x) + 50) /* reach this note by meend (a slide) */

typedef struct {
  const char *name, *thaat, *time, *mood;
  uint16_t mask;
  int8_t vadi, samvadi;
  int8_t aroha[10], avaroha[13], pakad[16]; /* semitones from Sa (negative: the octave below), END-terminated */
} raga_t;

static const raga_t RAGA[] = {
  {"Yaman", "Kalyan thaat", "evening, 6 to 9", "serene, devoted", M(0, 2, 4, 6, 7, 9, 11), 4, 11,
   {-1, 2, 4, 6, 9, 11, 12, END}, {12, 11, 9, 7, 6, 4, 2, 0, END}, {-1, 2, SL(4), 2, 0, 7, 6, 4, 2, 0, END}},
  {"Bhupali", "Kalyan thaat", "early night", "calm, devoted", (uint16_t)(1u << 0 | 1u << 2 | 1u << 4 | 1u << 7 | 1u << 9), 4, 9,
   {0, 2, 4, 7, 9, 12, END}, {12, 9, 7, 4, 2, 0, END}, {4, 2, 0, -3, 0, 2, 4, 7, SL(4), 9, 7, 4, 2, 0, END}},
  {"Bhairav", "Bhairav thaat", "daybreak", "grave, devoted", M(0, 1, 4, 5, 7, 8, 11), 8, 1,
   {0, 1, 4, 5, 7, 8, 11, 12, END}, {12, 11, 8, 7, 5, 4, 1, 0, END}, {4, 5, 8, 8, 7, 4, 5, SL(1), 1, 0, END}},
  {"Bhairavi", "Bhairavi thaat", "morning", "tender, devoted", M(0, 1, 3, 5, 7, 8, 10), 5, 0,
   {0, 1, 3, 5, 7, 8, 10, 12, END}, {12, 10, 8, 7, 5, 3, 1, 0, END}, {-2, 0, 3, 5, 8, 7, 3, 5, SL(3), 1, 0, END}},
  {"Kafi", "Kafi thaat", "night", "sweet, playful", M(0, 2, 3, 5, 7, 9, 10), 7, 0,
   {0, 2, 3, 5, 7, 9, 10, 12, END}, {12, 10, 9, 7, 5, 3, 2, 0, END}, {7, 5, 3, 2, 2, 3, 5, 7, 5, 3, 2, 0, END}},
  {"Bageshri", "Kafi thaat", "late night", "yearning, tender", M(0, 2, 3, 5, 7, 9, 10), 5, 0,
   {0, 3, 5, 9, 10, 12, END}, {12, 10, 9, 5, 7, 9, 3, 5, 3, 2, 0, END}, {-3, -2, 0, 5, 5, 7, 9, 5, 3, 2, 0, END}},
  {"Malkauns", "Bhairavi thaat", "the small hours", "deep, meditative", (uint16_t)(1u << 0 | 1u << 3 | 1u << 5 | 1u << 8 | 1u << 10), 5, 0,
   {0, 3, 5, 8, 10, 12, END}, {12, 10, 8, 5, 3, 0, END}, {5, 3, 5, 8, 10, 8, 5, SL(3), 0, END}},
  {"Khamaj", "Khamaj thaat", "night, 9 to 12", "playful, warm", (uint16_t)(M(0, 2, 4, 5, 7, 9, 10) | 1u << 11), 4, 11,
   {0, 4, 5, 7, 9, 11, 12, END}, {12, 10, 9, 7, 5, 4, 2, 0, END}, {10, 9, 5, 7, 9, 5, 4, END}},
};
#define RAGAS 8

/* Find the note: what it may play */
static const char *const LEVEL_NAME[5] = {"Sa and Pa", "Sa, Ga, Pa", "Major scale", "This thaat", "All twelve"};
static const uint16_t LEVEL_MASK[5] = {1u << 0 | 1u << 7, 1u << 0 | 1u << 4 | 1u << 7, M(0, 2, 4, 5, 7, 9, 11), 0, 0xFFF};
#define LEVELS 5

/* Alankars: practice patterns on the thaat's seven notes (degree 7 = Sa above) */
static const char *const ALANKAR_NAME[6] = {"Straight", "Pairs", "Threes", "Fours", "Skips", "Turns"};
static const char *const ALANKAR_EG[6] = {"S R G M P D N S", "S S, R R, G G...", "S R G, R G M...", "S R G M, R G M P...",
                                          "S G, R M, G P...", "S R S, R G R..."};
#define ALANKARS 6

static int alankar_degrees(int p, int8_t *out) { /* up, then the mirror down; returns how many */
  int n = 0, g, k;
  static const uint8_t SIZE[6] = {1, 2, 3, 4, 2, 3};
  if (p == 0) {
    for (g = 0; g <= 7; g++) out[n++] = (int8_t)g;
    for (g = 7; g >= 0; g--) out[n++] = (int8_t)g;
  } else if (p == 1) {
    for (g = 0; g <= 7; g++) out[n++] = (int8_t)g, out[n++] = (int8_t)g;
    for (g = 7; g >= 0; g--) out[n++] = (int8_t)g, out[n++] = (int8_t)g;
  } else if (p == 2 || p == 3) {
    int s = SIZE[p];
    for (g = 0; g + s - 1 <= 7; g++)
      for (k = 0; k < s; k++) out[n++] = (int8_t)(g + k);
    for (g = 7; g - s + 1 >= 0; g--)
      for (k = 0; k < s; k++) out[n++] = (int8_t)(g - k);
  } else if (p == 4) {
    for (g = 0; g + 2 <= 7; g++) out[n++] = (int8_t)g, out[n++] = (int8_t)(g + 2);
    for (g = 7; g - 2 >= 0; g--) out[n++] = (int8_t)g, out[n++] = (int8_t)(g - 2);
  } else {
    for (g = 0; g + 1 <= 7; g++) out[n++] = (int8_t)g, out[n++] = (int8_t)(g + 1), out[n++] = (int8_t)g;
    for (g = 7; g - 1 >= 0; g--) out[n++] = (int8_t)g, out[n++] = (int8_t)(g - 1), out[n++] = (int8_t)g;
  }
  return n;
}

/* Taal: one bol a beat. The tabla: bayan (left, bass) and dayan (right, tuned to Sa) */
enum { B_DHA, B_DHIN, B_DHI, B_TIN, B_TI, B_TA, B_NA, B_GE, B_KA, B_TU, B_KAT, B_CLICK };
static const char *const BOL[] = {"dha", "dhin", "dhi", "tin", "ti", "ta", "na", "ge", "ka", "tu", "kat", ""};
static const uint8_t BOL_BAYAN[] = {1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0};
static const uint8_t BOL_DAYAN[] = {1, 2, 2, 2, 2, 1, 1, 0, 3, 2, 3, 1}; /* 1 ringing, 2 open and long, 3 muted */
typedef struct {
  const char *name, *shape;
  uint8_t len;
  uint8_t bol[16];
  char mark[16]; /* X sam, 0 khali, 1..4 tali, ' ' none */
} taal_t;
static const taal_t TAAL[] = {
  {"Teentaal", "16 beats: 4 4 4 4", 16,
   {B_DHA, B_DHIN, B_DHIN, B_DHA, B_DHA, B_DHIN, B_DHIN, B_DHA, B_DHA, B_TIN, B_TIN, B_TA, B_TA, B_DHIN, B_DHIN, B_DHA},
   "X   2   0   3   "},
  {"Jhaptaal", "10 beats: 2 3 2 3", 10, {B_DHI, B_NA, B_DHI, B_DHI, B_NA, B_TI, B_NA, B_DHI, B_DHI, B_NA}, "X 2  0 3  "},
  {"Rupak", "7 beats: 3 2 2", 7, {B_TIN, B_TIN, B_NA, B_DHI, B_NA, B_DHI, B_NA}, "0  1 2 "},
  {"Ektaal", "12 beats: 2 x 6", 12,
   {B_DHIN, B_DHIN, B_DHA, B_TI, B_TU, B_NA, B_KAT, B_TA, B_DHA, B_TI, B_DHI, B_NA}, "X 0 2 0 3 4 "},
  {"Dadra", "6 beats: 3 3", 6, {B_DHA, B_DHI, B_NA, B_DHA, B_TI, B_NA}, "X  0  "},
  {"Keherwa", "8 beats: 4 4", 8, {B_DHA, B_GE, B_NA, B_TI, B_NA, B_KA, B_DHI, B_NA}, "X   0   "},
};
#define TAALS 6
static const uint8_t COUNT_IN[4] = {B_DHA, B_NA, B_NA, B_NA};

/* ================================================================ the lessons' state */

enum { L_RAGA,
       L_FIND,
       L_ALANKAR,
       L_TAAL,
       LESSONS };
static const char *const LESSON_NAME[LESSONS] = {"Raga studio", "Find the note", "Alankars", "Taal"};
enum { S_READY,
       S_DEMO,   /* raga: playing aroha, avaroha, pakad */
       S_ECHO,   /* raga: your turn */
       S_DONE,   /* raga / alankar: the result */
       S_WAIT,   /* find: listening for your answer */
       S_RIGHT,  /* find: right */
       S_WRONG,  /* find: not that one */
       S_COUNT,  /* alankar: count-in */
       S_LISTEN, /* alankar: the pattern played for you */
       S_YOURS,  /* alankar: your turn, in time */
       S_PLAYING /* taal */ };
enum { PH_NONE,
       PH_AROHA,
       PH_AVAROHA,
       PH_PAKAD };

static uint8_t learn, lesson, litem[LESSONS] = {0, 0, 0, 0}, lstate, tempo_knob = 50;
static uint8_t learn_drone_was, learn_mode_was;
static uint8_t phase, hint_pc = 255; /* raga demo phase; echo: the note it wanted after a slip */
static uint32_t hint_until;

/* Raga studio */
typedef struct {
  int16_t note; /* MIDI, or -1 for a rest */
  uint8_t slide, phase;
  uint16_t ms;
} step_t;
static step_t demo[48];
static int demo_n, demo_i;
static uint32_t demo_at;
static int8_t echo_pc[16];
static int echo_n, echo_i, echo_slips;

/* Find the note */
static int find_note = -1, find_streak, find_best, find_played = -1;
static uint32_t find_next_at;

/* Alankars */
static int8_t alk[48];
static int alk_n, alk_hits;
static uint8_t alk_hit[48];
static uint32_t alk_seen, alk_beat_at;

static uint32_t learn_tempo(void) { return 40u + tempo_knob * 120u / 127u; } /* beats a minute */
static uint16_t learn_mask(void) {
  if (learn && lesson == L_RAGA) {
    return RAGA[litem[L_RAGA]].mask;
  }
  return THAAT[saved.thaat].mask;
}
static int degree_semis(int d) { /* the thaat's d-th note above Sa (7 = Sa above) */
  uint16_t m = THAAT[saved.thaat].mask;
  int k = 0;
  for (int s = 0; s < 12; s++) {
    if (m >> s & 1) {
      if (k == d) {
        return s;
      }
      k++;
    }
  }
  return 12;
}

/* the lead string plays a note for the lesson (a pluck, or a slide) */
static void lesson_note(int midi, int slide) {
  lead_target = midi * 256;
  if (!slide) {
    lead_vel = 95;
    lead_plucks++;
  }
}

static void pulse_start(const uint8_t *bols, int len) {
  pulse_on = 0;
  pulse_bols = bols, pulse_len = (uint8_t)len;
  pulse_spb = 44100u * 60u / learn_tempo();
  pulse_beats = 0;
  pulse_restart = 1;
  pulse_on = 1;
}

static void learn_stop(void) {
  pulse_on = 0;
  meend_force = 0;
  lstate = S_READY;
  phase = PH_NONE;
}

static void raga_start_demo(uint32_t now) {
  const raga_t *r = &RAGA[litem[L_RAGA]];
  int sa = 60 + saved.sa, n = 0;
  const int8_t *parts[3] = {r->aroha, r->avaroha, r->pakad};
  for (int p = 0; p < 3; p++) {
    for (int i = 0; parts[p][i] != END && n < 46; i++) {
      int v = parts[p][i], slide = v >= 38;
      demo[n++] = (step_t){(int16_t)(sa + (slide ? v - 50 : v)), (uint8_t)slide, (uint8_t)(PH_AROHA + p), (uint16_t)(p == 2 ? 520 : 400)};
    }
    demo[n++] = (step_t){-1, 0, (uint8_t)(PH_AROHA + p), (uint16_t)(p == 2 ? 900 : 700)};
  }
  demo_n = n, demo_i = 0, demo_at = now;
  echo_n = 0;
  for (int i = 0; r->pakad[i] != END && echo_n < 16; i++) {
    int v = r->pakad[i] >= 38 ? r->pakad[i] - 50 : r->pakad[i];
    echo_pc[echo_n++] = (int8_t)((v % 12 + 12) % 12);
  }
  meend_force = 40;
  lstate = S_DEMO;
}

static void find_new(uint32_t now) {
  uint16_t m = LEVEL_MASK[litem[L_FIND]];
  if (!m) {
    m = THAAT[saved.thaat].mask;
  }
  int count = 0, pick, prev = find_note >= 0 ? find_note : -1;
  for (int s = 0; s < 12; s++) count += m >> s & 1;
  for (int tries = 0; tries < 8; tries++) {
    pick = (int)(random32() % (uint32_t)count);
    for (int s = 0; s < 12; s++) {
      if (m >> s & 1 && pick-- == 0) {
        find_note = s;
        break;
      }
    }
    if (find_note != prev || count == 1) {
      break;
    }
  }
  lesson_note(60 + saved.sa + find_note, 0);
  lstate = S_WAIT, find_played = -1;
  (void)now;
}

static void alankar_start(void) {
  alk_n = alankar_degrees(litem[L_ALANKAR], alk);
  alk_hits = 0;
  for (int i = 0; i < alk_n; i++) alk_hit[i] = 0;
  alk_seen = 0;
  pulse_start(COUNT_IN, 4);
  lstate = S_COUNT;
}

/* PLAY in Learn mode */
static void learn_play(uint32_t now) {
  switch (lesson) {
    case L_RAGA:
      if (lstate == S_DEMO) {
        learn_stop();
      } else {
        raga_start_demo(now);
      }
      break;
    case L_FIND:
      if (lstate == S_WAIT || lstate == S_WRONG) {
        lesson_note(60 + saved.sa + find_note, 0); /* hear it again */
      } else {
        find_streak = lstate == S_READY ? 0 : find_streak;
        find_new(now);
      }
      break;
    case L_ALANKAR:
      if (lstate == S_COUNT || lstate == S_LISTEN || lstate == S_YOURS) {
        learn_stop();
      } else {
        alankar_start();
      }
      break;
    case L_TAAL:
      if (lstate == S_PLAYING) {
        learn_stop();
      } else {
        pulse_start(TAAL[litem[L_TAAL]].bol, TAAL[litem[L_TAAL]].len);
        lstate = S_PLAYING;
      }
      break;
  }
}

/* a key played in Learn mode */
static void learn_key(int note, uint32_t now) {
  int pc = ((note - saved.sa) % 12 + 12) % 12;
  if (lesson == L_RAGA && lstate == S_ECHO) {
    if (pc == echo_pc[echo_i]) {
      echo_i++;
      hint_pc = 255;
      if (echo_i >= echo_n) {
        lstate = S_DONE;
      }
    } else {
      echo_slips++;
      hint_pc = (uint8_t)echo_pc[echo_i], hint_until = now + 1800;
    }
  } else if (lesson == L_FIND && (lstate == S_WAIT || lstate == S_WRONG)) {
    find_played = pc;
    if (pc == find_note) {
      find_streak++;
      find_best = find_streak > find_best ? find_streak : find_best;
      lstate = S_RIGHT, find_next_at = now + 1500;
    } else {
      find_streak = 0;
      lstate = S_WRONG, find_next_at = now + 1100; /* then it plays the note again */
    }
  } else if (lesson == L_ALANKAR && lstate == S_YOURS) {
    /* the beat it belongs to: this one, or the next if it comes in the second half of the beat */
    uint32_t half = 30000u / learn_tempo();
    int j = (int)alk_seen - 5 - alk_n; /* the beat now sounding, counted in your turn */
    if (now - alk_beat_at > half) {
      j++;
    }
    if (j >= 0 && j < alk_n && !alk_hit[j]) {
      int want = (degree_semis(alk[j]) % 12);
      if (pc == want) {
        alk_hit[j] = 1, alk_hits++;
      }
    }
  }
}

/* every pass of loop() in Learn mode */
static void learn_update(uint32_t now) {
  if (lesson == L_RAGA && lstate == S_DEMO) {
    while (demo_i < demo_n && now >= demo_at) {
      step_t *s = &demo[demo_i++];
      phase = s->phase;
      if (s->note >= 0) {
        lesson_note(s->note, s->slide);
      }
      demo_at += s->ms;
    }
    if (demo_i >= demo_n && now >= demo_at) {
      meend_force = 0;
      lstate = S_ECHO, echo_i = 0, echo_slips = 0, phase = PH_NONE, hint_pc = 255;
    }
  }
  if (hint_pc != 255 && now >= hint_until) {
    hint_pc = 255;
  }
  if (lesson == L_FIND && (lstate == S_RIGHT || lstate == S_WRONG) && now >= find_next_at) {
    if (lstate == S_RIGHT) {
      find_new(now);
    } else {
      lesson_note(60 + saved.sa + find_note, 0);
      lstate = S_WAIT, find_next_at = 0xFFFFFFFFu;
    }
  }
  if (lesson == L_ALANKAR && (lstate == S_COUNT || lstate == S_LISTEN || lstate == S_YOURS)) {
    uint32_t b = pulse_beats;
    if (b != alk_seen) { /* a new beat */
      alk_seen = b, alk_beat_at = now;
      uint32_t n = (uint32_t)alk_n;
      /* beats 1..4 count in; 5..4+n the pattern played for you; then n beats for you to play it */
      if (b >= 5 && b <= 4 + n) {
        lstate = S_LISTEN;
        lesson_note(60 + saved.sa + degree_semis(alk[b - 5]), 0);
      } else if (b >= 5 + n && b <= 4 + 2 * n) {
        lstate = S_YOURS;
      } else if (b > 4 + 2 * n) {
        pulse_on = 0;
        lstate = S_DONE;
      }
    }
  }
}

/* the bead to aim for now (-1: none) */
static int learn_target(void) {
  if (lesson == L_ALANKAR && lstate == S_YOURS) {
    int j = (int)alk_seen - 5 - alk_n;
    return j >= 0 && j < alk_n ? degree_semis(alk[j]) % 12 : -1;
  }
  return -1;
}

/* the words at the bottom right, and the item at the bottom left */
static const char *learn_item_name(char *buf) {
  switch (lesson) {
    case L_RAGA:
      return RAGA[litem[L_RAGA]].name;
    case L_FIND:
      return LEVEL_NAME[litem[L_FIND]];
    case L_ALANKAR:
      return ALANKAR_NAME[litem[L_ALANKAR]];
    default:
      return TAAL[litem[L_TAAL]].name;
  }
  (void)buf;
}
static const char *learn_status(char *buf) {
  char n[12];
  switch (lstate) {
    case S_READY:
      return "PLAY: start";
    case S_DEMO:
      return phase == PH_AROHA ? "aroha, up" : phase == PH_AVAROHA ? "avaroha, down" : "pakad";
    case S_ECHO:
      return "your turn";
    case S_WAIT:
    case S_RIGHT:
    case S_WRONG:
      format_int(n, find_streak);
      cat(cat(buf, "streak "), n);
      return buf;
    case S_COUNT:
      return "get ready";
    case S_LISTEN:
      return "listen";
    case S_YOURS:
      return "your turn";
    case S_PLAYING:
      format_int(n, (int32_t)learn_tempo());
      cat(cat(buf, n), " bpm");
      return buf;
    default:
      return "PLAY: again";
  }
}
