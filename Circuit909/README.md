# 909 Circuits

Circuit models of the Roland TR-909's instruments, one module per instrument, in the same
spirit as [808 Circuits](../Circuit808/): a model of the stock voice first, then the circuit's
inner parts on the panel with ranges past stock. No samples are used or shipped.

Modules (all 16 HP since 2.3.0): **909 Kick, 909 Snare, 909 Tom, 909 Rim, 909 Clap, 909 Cymbals**.

![909 Kick](panel-preview.png)

Every module has a **STOCK 909** row (the 909's own knobs, plus ACCENT, LEVEL and AGE),
the circuit controls, jacks for RAND, NEAR, TRIG, ACCENT, V/OCT (not on the Clap; the Cymbals
also have OPEN) and OUT, and a HIT button. An **orange dot** beside each knob marks the stock 909 setting; right-click →
*Reset to stock 909* puts every knob back (LEVEL and switches are left alone).

- **ACCENT** stands in for the 909's TOTAL ACCENT knob. The ACCENT jack takes a gate
  (accented) or 0-5 V (how much); it is read over 0.5 ms after the trigger so an accent that
  arrives a sample late still counts. One accent law for every voice: about +5 dB at stock.
- **AGE**: every module has a serial number (saved with the patch, re-rollable in the
  right-click menu) that sets where its parts sit inside the 909's component tolerances; AGE
  scales how far off they are, adds a little hit-to-hit drift, and past about 70 % wears the
  capacitors (shorter decays, a little hiss). AGE 0 = every unit exactly stock.
- LEVEL and AGE are never randomized.

## The voices

| Module | Circuit | Controls beyond the stock 909 |
|---|---|---|
| 909 Kick | ramp VCO reset on every hit, waveshaped towards a sine; pitch sweeps from high down to the base note (TUNE = how long); VCA with DECAY; ATTACK adds a click (square pulse + low-passed noise burst) | BASE, SWEEP, BEND (slower last drop), PULSE, NOISE, NOISE LP, SNAP, SHAPE (sine ↔ triangle ↔ ramp), DRIVE |
| 909 Snare | two swept VCOs (about 185 and 330 Hz) through one VCA, plus white noise through a high-pass and TONE low-pass into its own VCA (SNAPPY) | RATIO, BALANCE, DECAY, SWEEP, BEND, SHAPE, SNAP DEC, COLOUR, DRIVE |
| 909 Tom | the 909's three tom circuits (LOW / MID / HIGH, about 100 / 145 / 210 Hz): swept VCO, waveshaper, VCA, skin noise | SWEEP, BEND, SHAPE, NOISE, DOUBLE, TONE, MUTE, DRIVE |
| 909 Rim | three high bridged-T resonators (about 500 Hz, 1.1 kHz, 1.8 kHz) through a fast transistor VCA and a high-pass | TUNE, RATIO, RING, GATE, SNAP, STRIKE, DRIVE |
| 909 Clap | band-passed noise into a VCA from a sawtooth that fires bursts (the last rings on), plus a reverb envelope | BAND, RES, TONE, BURSTS, SPREAD, SNAP, TAIL, MIX, DRIVE |
| 909 Cymbals | the 909's 6-bit sample path: clocked DAC (TUNE = clock), BITS, reconstruction filter, analogue VCA, output high-pass | TYPE (HAT / CRASH / RIDE), METAL, NOISE, BITS, RECON, CH DEC, CUT, DRIVE |

**909 Cymbals.** On the real 909 the hats, crash and ride are 6-bit recordings in ROM. Those
recordings are Roland's, so they are not here; the module models the path around them. Its
source is a built-in synthesised metal sound (six square oscillators plus noise, shaped per
type, restarted on every hit so each hit is the same "recording") or a WAV you load
(right-click → *Load sample WAV*; the path is saved with the patch). A loaded WAV is converted
once to the model's 32 kHz clock and normalised, like a ROM; PCM 8/16/24/32-bit and 32/64-bit
float files load, anything else is refused and the menu says so. The source reaches the
6-bit converter at full level and the analogue VCA after it does all the fading, as on the
909, so tails fade smoothly. TRIG plays the closed hat (CH DEC) and chokes the open hat; OPEN
plays the open hat. DECAY is a multiple of the type's stock length (open hat 0.7 s, crash
2.2 s, ride 1.6 s), so 1.0x is stock for every type. For crash and ride both inputs play the
cymbal. At stock TUNE a loaded WAV plays at its own pitch.

**Sources and honesty.** The structures follow the TR-909 service notes' block diagrams and
the 909's widely published behaviour. Stock pitches and times are fitted, not read from
component values, and have not yet been compared with recordings of a real 909.

**Built in from the start** (lessons from the 808 Circuits DSP review): the same level at
44.1, 48 and 96 kHz (noise scaled to the sample rate), band-limited VCOs and DAC steps, the
rim's STRIKE and TUNE holding the level, clap BAND and RES holding the level, knob maths at
control rate, smoothed DRIVE and LEVEL, a DC blocker after DRIVE, NaN-safe V/OCT, and voices
that sleep when silent.

## 2.1.0 (Fable's DSP review)

Cymbals: ENV returns to zero and the module sleeps after a crash or ride; the fade moved after
the converter; DECAY reads true for every type; WAV files resampled to 32 kHz, normalised,
handed over without a copy on the audio thread, and odd headers refused instead of crashing.
All voices: the output stage is straight up to 7.5 V at DRIVE 0 (accents keep their peaks),
DRIVE adds little loudness; retriggering a ringing Kick, Snare or Tom fades the old tail over
about 1 ms instead of clicking; the ramp end of SHAPE starts from zero; AGE hiss holds its level
at high sample rates. Rim level holds over V/OCT and its SNAP range is 100-2500 Hz; Tom skin
noise holds its level at high sample rates; Snare noise filters are two-pole; Clap MIX is
equal-power. Both hat triggers inside the accent window: the closed hat wins.
Still to check against the service notes or a recording before moving any stock dot: kick
sweep law and start pitch, the snare's noise paths and oscillator pitches, three oscillators
per tom, rim ring and pitches, the cymbal clock rate and reconstruction filter.

## 2.3.0: compact panels and randomise

- The six single modules are 16 HP (was 28): the stock 909 knobs big in a shaded band, the
  circuit knobs small below, no explanation text. Jacks are only RAND, NEAR, TRIG, ACC,
  V/OCT and OUT (plus OPEN on the Cymbals); the other CV inputs and ENV are gone.
- **RAND** (key R, or a trigger at the RAND jack) sets every knob to a random value, like
  Ctrl+R. **NEAR** (key N, or the NEAR jack) does the same, but values near the stock 909
  setting are twice as likely as values at the ends of the knob. Neither touches LEVEL,
  AGE or the switches.

## 909 Circuit Bench (2.3.0)

All eleven voices in one 36 HP module: BD, SD, LT, MT, HT, RS, CP, CH, OH, CR, RD. It
works like the 808 Circuit Bench 2.4.0.

- No screen: 16 knobs follow the selected voice, each with its name and value, grouped by
  circuit stage (for the kick: TRIG, PITCH ENV, CLICK, VCO + SHAPER, VCA, OUT). Knobs a
  voice does not use are hidden. An orange dot marks stock 909; a blue dot marks the kit
  as loaded.
- Double-click a knob: back to the loaded kit value, double-click again: stock 909.
- Tap a voice button to select it; SHIFT + voice hits it. STOCK puts the voice back to the
  loaded kit (SHIFT + STOCK: stock 909). COPY, then tap another voice of the same kind,
  pastes.
- KITS steps through four kit memories saved in the patch (SHIFT + KITS: stock kit).
  Right-click has 11 factory kits (Stock 909, Chicago Jack, Detroit, Hard Techno, Gabber
  Kick, Deep House, Garage Swing, Crunchy 4-bit, Tuned Toms, Worn Unit, Circuit Bent) and
  .909kit save/load.
- **RAND** rolls the selected voice near its loaded values; SHIFT + RAND (or RAND while
  SHIFT is lit) rolls wild. The IN jack beside it does the same from a trigger. LEVEL, AGE,
  PAN, the tom range and the cymbal type are never rolled.
- Every voice has its own TRIG and OUT. Voices whose OUT is not patched go to the stereo
  mix (PAN per voice), 6 dB below the single outputs. ACC: mono = all voices, poly = one
  channel per voice. POLY: one trigger channel per voice (1 = BD ... 11 = RD).
- CH chokes OH, as on the 909.
- Right-click: own WAVs for the hats (CH and OH together), crash and ride, saved by path
  in the patch; serial number for AGE.

The sound is the same engines as the single modules. Offline checks in `test/bench.cpp`:
levels and accent per voice, tom pitches, CH/OH lengths, the choke, random settings, every
parameter on a knob slot exactly once, RAND never moving LEVEL/AGE/PAN/RANGE/TYPE, and every
factory kit playing at a sane level.

## Build

`make test` runs the offline checks (levels, accent, sample rates, 40 random and extreme
settings per voice, AGE, kick pitch, tom sizes, cymbal choke, BITS, user WAV pitch) and writes
demo WAVs to `test/out/`. `python3 res/make_voice_panels.py` draws the panels from
`res/voices.py` and writes `src/VoiceSpecs.hpp`; `python3 tools/preview_voices.py outdir`
renders previews. The repository's GitHub Action builds the plugin.

MIT licence.
