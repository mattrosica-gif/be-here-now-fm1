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

[coach.html](https://mattrosica-gif.github.io/be-here-now-fm1/coach.html) runs in Chrome next to the FM-1 (plugged in
by USB), or with a guitar or piano through the computer's microphone. It teaches how to think and improvise in both
traditions: Think and improvise (gravity and landing notes, fewer notes, developing ideas, question and answer,
rhythm and space, chords, the pentatonic, ornaments, raga habits), a Jam room with backing tracks and live feedback,
Why it works (frequency, overtones, ratios, major and minor, the chain of fifths, equal temperament, the string), a
two-language dictionary and the basics. It never lights the answer unless you ask for a hint. The FM-1 talks to it
over MIDI control changes (firmware 1.0):

```
  CC 20  Sa (0..11)              CC 25 / 26  notes that blink on the keys (a hint)
  CC 21  thaat (0..9)            CC 27 / 28  notes lit steadily (the scale or chord), pitch-class masks from C
  CC 22  drone (>= 64 on)        CC 29       >= 64 keeps the Coach's lights on (sent every 2 s)
  CC 23  mode (>= 64 Chord)      the FM-1 sends CC 20..23 back when you turn PRESETS or ALGORITHM, or press SEL or PLAY
  CC 24  Learn mode (>= 64 on)
```

Push to GitHub and set Settings → Pages → Source: GitHub Actions; every push builds the emulator and the FM-1 package. A tag (v0.1) makes a release with the .fwsc. Pick your own `identity` in firmware.json before sharing (FM-1_86 is a placeholder).

Original foma README: https://github.com/konsumer/foma#readme
