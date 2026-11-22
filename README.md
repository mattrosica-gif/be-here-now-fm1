# TARAF

A sarod-inspired firmware for the M-VAVE FM-1 that teaches you what you play. Built on [foma](https://github.com/konsumer/foma) (GPL-3.0).

```
KNOB1..4    Tone, Taraf, Sustain, Meend: the four gauges along the top of the screen
PRESETS     the thaat (parent scale)          ALGORITHM   Sa, the home note
SELECT      drone level                       MASTER      volume
OCT- OCT+   octave                            SEL         Sarod (one string, slides) / Chord (names and shapes)
PLAY        tanpura drone on / off            ARP         jhala: the high drone strings strum along
FX          reverb: hall, room, dry           HOME        for Sylmira
keys        play; in Sarod mode, press a key while holding another to slide (meend)
```

The screen shows the 12 notes as a mala of beads with Sa at the top, in Devanagari sargam with the Western names
outside. What you play glows marigold, the sympathetic strings that answer it glow rose, an arc from Sa shows the
interval, a slide carries a spark around the ring, and a chord draws its shape. When you stop, the centre breathes.

The sound: Karplus-Strong strings with a stiffness filter for the steel, a skin body, a coconut-shell plectrum, 11
sympathetic strings tuned to the thaat, a tanpura with a buzzing jawari bridge, chikari strings for jhala, and a hall.
Integers only: the FM-1 has no FPU. Fonts: Mukta (Ek Type, SIL Open Font License), DejaVu Sans; assets are generated
by `tools/gen_assets.py`.


Push to GitHub and set Settings → Pages → Source: GitHub Actions; every push builds the emulator and the FM-1 package. A tag (v0.1) makes a release with the .fwsc. Pick your own `identity` in firmware.json before sharing (FM-1_86 is a placeholder).

Original foma README: https://github.com/konsumer/foma#readme
