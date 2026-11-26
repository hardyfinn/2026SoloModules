# Upright

A sampled upright piano for VCV Rack, with few controls: just a convincing
piano. The samples are **VS Upright No. 1** by Versilian Studios (CC0, from the
VSCO 2 Community Edition): a C and a G in every octave, three dynamics (pp, mf,
f) and two takes of most notes, so repeated notes don't sound identical.

(An earlier synthesised piano model lived in this folder on this branch; it was
dropped because a piano is far better served by samples.)

## Panel (12 HP)

| Control | What it does |
|---|---|
| TONE | Dark to bright (tilt around 1.2 kHz; centre = the recording as is) |
| RELEASE | How fast the dampers stop a note when the key is let go (0.06 to 3 s) |
| PEDAL | Sustain pedal latch (red light when down) |
| VOLUME | Output level (not changed by Ctrl+R) |
| READY light | Blinks while the samples load (a second or two, once per Rack session), steady when ready |

| Jack | |
|---|---|
| V/OCT, GATE | Poly, up to 16 notes. 0 V = middle C |
| VEL | Poly velocity 0 to 10 V, read when the key goes down. Unpatched = 6.5 V (a mezzo forte) |
| SUS | Sustain pedal gate (high = down), added to the button |
| L, R | Stereo out, about ±3 V for a loud single note |

How it plays:
- Velocity picks the dynamic layer (pp below 4.3 V, mf to 7.8 V, f above) and
  sets the level inside the layer, so loudness rises smoothly over about 40 dB.
- Each key uses the nearest sampled note, shifted by at most 3.5 semitones.
  Takes alternate per note.
- The top keys (F6 up) have no dampers, as on a real piano, so they ring out.
- Striking a key that is still ringing fades the old note quickly (slowly with
  the pedal down).

Memory: the samples are decoded once into about 150 MB of RAM, shared by every
Upright in the patch. CPU is tiny (16 pedalled notes about 1 % of one core).

## Building and testing

`make` / `make dist` with the Rack SDK as usual. `make test` decodes the samples,
runs offline checks (all 88 keys, smooth velocity, pedal and dampers, CPU) and
writes `test/out/upright-phrase.wav`. The panel is drawn by `res/make_panel.py`.

## Licences

Code: MIT (`LICENSE.txt`). Samples: CC0 (`res/samples/SAMPLES-LICENSE.txt`).
Ogg Vorbis decoder: stb_vorbis by Sean Barrett, public domain (`src/dep/`).
