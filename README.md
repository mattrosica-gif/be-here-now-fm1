# BE HERE NOW

After Ram Dass: love, serve, remember. A sarod-inspired firmware for the M-VAVE FM-1 that teaches you what you play. Built on [foma](https://github.com/konsumer/foma) (GPL-3.0).

```

  keys        SAROD mode: one string. Press a key while holding another = MEEND (a slide), no new pluck.
              CHORD mode: four strings, one per key, and the screen names the chord.
  SEL         switch SAROD / CHORD
  PLAY        the tanpura drone on / off (Pa, Sa, Sa, low Sa, round and round)
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

Push to GitHub and set Settings → Pages → Source: GitHub Actions; every push builds the emulator and the FM-1 package. A tag (v0.1) makes a release with the .fwsc. Pick your own `identity` in firmware.json before sharing (FM-1_86 is a placeholder).

Original foma README: https://github.com/konsumer/foma#readme
