# BE HERE NOW

A sarod-inspired firmware for the M-VAVE FM-1 that teaches you what you play. Built on [foma](https://github.com/konsumer/foma) (GPL-3.0).

```

  keys        SAROD mode: one string. Press a key while holding another = MEEND (a slide), no new pluck.
              CHORD mode: four strings, one per key, and the screen names the chord.
  SEL         switch SAROD / CHORD
  PLAY        the tanpura drone on / off (Pa, Sa, Sa, low Sa, round and round)
  REC         the scale on its own: up from Sa and back down, the keys lighting up as it climbs;
              KNOB4 is the tempo; play a key and it stops for your turn
  PRESETS     the thaat (the parent scale): the sympathetic strings retune to it
  ALGORITHM   Sa, the home note (C .. B)
  OCT- OCT+   the octave
  KNOB1       TONE     bright / dark pluck
  KNOB2       TARAF    how loud the sympathetic strings ring
  KNOB3       SUSTAIN  how long a string rings
  KNOB4       MEEND    how slow the slides are (0: no slides, every key plucks)
  key lights  the notes of the thaat

The sound: Karplus-Strong strings (a burst of noise in a delay line, filtered a little on every pass).
The main string feeds 11 more strings tuned to the thaat: they only ring when you play a note (or a
harmonic) they share, as a sarod's taraf strings do.

```

## The Coach

[coach.html](https://mattrosica-gif.github.io/be-here-now-fm1/coach.html) runs in Chrome next to the FM-1 (plugged
in by USB): a guided course in Western and Indian music side by side, nine units from the tonic to a first raga
and taal. It hears what you play, names every note in both languages, lights the scale on a mala, a piano and a
guitar neck, and sets up the FM-1 for each step over MIDI control changes (needs firmware 0.8):

```
  CC 20  Sa (0..11)              CC 25 / 26  notes that blink on the keys (what the lesson wants next)
  CC 21  thaat (0..9)            CC 27 / 28  notes lit steadily (the scale), pitch-class masks from C
  CC 22  drone (>= 64 on)        CC 29       >= 64 keeps the Coach's lights on (sent every 2 s)
  CC 23  mode (>= 64 Chord)
  CC 24  Learn mode (>= 64 on)
```

Push to GitHub and set Settings → Pages → Source: GitHub Actions; every push builds the emulator and the FM-1 package. A tag (v0.1) makes a release with the .fwsc. Pick your own `identity` in firmware.json before sharing (FM-1_86 is a placeholder).

Original foma README: https://github.com/konsumer/foma#readme
