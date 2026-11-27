# 808 Circuits

Circuit models of the Roland TR-808's instruments, one module per instrument. Each one starts
from a faithful model of the original voice, then puts the circuit's inner parts on the panel
and takes them past their stock ranges. Hats and cymbal are the separate [808 Metal](https://github.com/hardyfinn/808Metal) plugin.

Planned family: Kick (built), Snare, Tom/Conga, Rim, Clave, Clap, Maracas, Cowbell.

![808 Kick](panel-preview.png)

## 808 Kick (28 HP)

The TR-808 bass drum as Werner, Abel and Smith describe it ("A Physically-Informed,
Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum Circuit", DAFx-14):
a 1 ms trigger pulse (4 V, up to 14 V when accented) goes through a pulse shaper with a diode
clamp into a **bridged-T resonator** at 49.5 Hz. There is no VCA: the ring time comes from a
feedback buffer around the bridged-T. For about 6 ms after the hit a transistor (Q43) shorts
part of the bridged-T, so the first cycle runs more than an octave high (the "attack jump").
On big negative swings the same transistor leaks, so loud early cycles run sharp and settle
back (the "pitch sigh", roughly 58 to 49.5 Hz). Then a one-pole tone filter and the output stage.

A **red dot** beside each knob marks the stock 808 setting. Right-click → *Reset to stock 808*
puts every knob back (LEVEL is left alone).

| Knob | What it is in the circuit | Stock | Range |
|---|---|---|---|
| TUNE | bridged-T centre (a trimmer on the 808) | 49.5 Hz | 20-220 Hz, + V/OCT |
| DECAY | feedback buffer (the 808's DECAY pot) | 0.43 s ring | 80 ms - 1.6 s stock range, then to 8 s, then SUSTAIN (self-oscillation) |
| TONE | output low-pass | 1.2 kHz | 150 Hz - 10 kHz |
| ACCENT | global accent level (VR3) | 60 % | accented hit 4-14 V |
| LEVEL | output level | | (not randomized) |
| AGE | part tolerances + drift + wear | 0 (new) | see below (not randomized) |
| SWEEP | size of the attack jump | 1.1 oct | 0-2.5 oct |
| BEND | how long the jump lasts | 6 ms | 1-60 ms (long = zaps) |
| STRIKE | trigger pulse width | 1 ms | 0.1-10 ms (wide = softer) |
| CLICK | shaped pulse straight through the bridged-T | 1x | 0-2x |
| RETRIG | second pulse into the bridged-T | small, right after the jump | up = louder and later: a flam |
| SIGH | Q43 leak: loud cycles sharp | 808 amount | off to a deep boom drop |
| TENSION | louder = higher, at all levels (like a skin) | off | |
| SHELF | feedback buffer's high shelf | flat decay | up: raised pitches die faster, down: they ring |
| DRIVE | output op-amp into its rails | clean | |

**AGE.** The 808 was built from ±20 % capacitors and ±5 % resistors, so no two units sounded the
same. Every 808 Kick has a serial number (saved with the patch, shown and re-rollable in the
right-click menu) that fixes where its parts sit inside those tolerances; AGE scales how far
they are off (pitch about ±12 %, decay ±25 %, tone ±20 %, sigh, jump and level). Each hit also
drifts a little. Past about 70 % the capacitors are worn: shorter ring and a little hiss (halved in 2.0.1).
At 0 every unit is exactly the stock 808.

**Jacks.** TRIG, ACCENT (gate = accented; 0-5 V = how much, read at each hit), CV for
V/OCT, DECAY, TONE, SWEEP, SIGH, TENSION and DRIVE (±5 V = full knob range), ENV out
(ring amplitude 0-10 V) and OUT (about 4.5 V peak unaccented, 8.5 V accented).

**Notes.** Retriggering adds to the ring that's already going, as on the 808. The paper prints
no component values, so the stock numbers are fitted to its reported behaviour; `make test`
checks them (49.5 Hz, the sigh inside 48-58 Hz, attack jump, accent, bounded sustain, AGE).

## Build

`make test` runs the offline checks and writes demo WAVs to `test/out/`.
`python3 res/make_kick_panel.py` draws the panel; `python3 tools/preview.py out.png` renders a preview.
The repository's GitHub Action builds the plugin.

MIT licence.
