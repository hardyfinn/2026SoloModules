# Sawstack: build instructions

Design by Fable, 2026-11-23. For the model that builds it. Read `RESEARCH.md` and `DSP-REVIEW.md` in this folder for why things are the way they are. Where this document and those two differ, this document wins: it includes every change from the review.

The **reference engine** is `tools/proto/sawstack_ref.py`. It is the definition of the sound. Where prose here is ambiguous, the Python decides. `tools/proto/check_ref.py` shows the checks the reference passes, and the C++ must pass equivalents.

## 1. What to build

- Plugin folder `SawstackFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-qvvmoz` (not main). Use `CicadaFable` on branch `claude/project-thread-0bfex4` as the template for the Makefile, plugin.json, panel script, offline test harness, PDF tooling and README. The repository's workflow already builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `SawstackFable`, name `Sawstack Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `Sawstack`, name `Sawstack`, tags Synth voice, Oscillator, Polyphonic, Envelope generator. Description: "Five-oscillator swarm voice: one SPREAD knob from unison to a stack of octaves, sine-to-saw TIMBRE, saw-multiplying RICHNESS, stereo VCA, two rise/fall envelopes, and an AGE knob for analog looseness".
- No Verbos name or likeness anywhere on the panel, in the plugin.json or in the manual title. The manual and README may say once, in prose, that the module is inspired by the Verbos Electronics Sawtooth Stack and is not affiliated with it.
- Panel 22 HP, text labels, the Haer logo as on Cicada.
- Deliverables: C++ module, panel SVG from a script, 12 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo WAVs rendered by the C++ engine.

## 2. Signal flow

Per voice (one per poly channel, up to 16):

```
V/OCT + FREQUENCY + FREQ CV ──► five oscillators (shared pitch, SPREAD fans them)
                                  each: triangle core ─► sine shaper ┐
                                                         └► saw × RICHNESS ┴► TIMBRE crossfade ─► saturation ─► band-limit ─► reset rounding
five outputs ─► pan (stereo) or equal sum (mono) ─► VCA (AMPLITUDE) ─► output stage (DRIVE) ─► decimate ─► DC block ─► OUTPUT
GATE 1 ─► envelope 1 ─► ENV 1, END 1; normalled to RICHNESS CV
GATE 2 ─► envelope 2 ─► ENV 2, END 2; normalled to AMPLITUDE CV
```

Voices are summed into the L and R outputs. The oscillators and the output stage run at 2 times the engine sample rate. Envelopes run at the engine rate.

## 3. Panel

Six columns at x = 11.5, 29.25, 47.0, 64.75, 82.5, 100.25 mm. Adjust y positions as needed so labels do not collide, but keep the grouping. Render a preview PNG and look at it before finishing.

| Row | y (mm) | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 |
|---|---|---|---|---|---|---|---|
| Large knobs | 24 | FREQUENCY | SPREAD | TIMBRE | RICHNESS | AMPLITUDE | OUTPUT |
| Trimpots (attenuverters) | 40 | FREQ CV | SPREAD CV | TIMBRE CV | RICHNESS CV | AMP CV | |
| Input jacks | 53 | FREQ | SPREAD | TIMBRE | RICHNESS | AMP | |
| Medium knobs | 72 | RISE 1 | FALL 1 | RISE 2 | FALL 2 | AGE | DRIVE |
| Trimpots | 87 | | FALL 1 CV | | FALL 2 CV | | |
| Input jacks | 100 | V/OCT | GATE 1 | FALL 1 | GATE 2 | FALL 2 | |
| Output jacks | 114 | ENV 1 | END 1 | ENV 2 | END 2 | L | R |

A small light beside RISE 1 and RISE 2 shows each envelope's level (voice 0, or the maximum across voices). Section captions: "STACK" over SPREAD, "ENVELOPE 1", "ENVELOPE 2". Outputs on a darker plate as on Cicada. There is no VEL jack: velocity is patched into TIMBRE.

## 4. Parameters

| Param | Range | Default | Meaning |
|---|---|---|---|
| FREQUENCY | -4 to +4 octaves | 0 | Centre pitch around C4 (261.6256 Hz). Display in Hz. |
| SPREAD | 0 to 1 | 0.25 | Section 5.2 |
| TIMBRE | 0 to 1 | 1 | Sine to saw |
| RICHNESS | 0 to 1 | 0 | Saw multiple, 1 to 8 |
| AMPLITUDE | 0 to 1 | 0 | VCA offset |
| OUTPUT | 0 to 1 | 1 | Final level, 1 = 10 V peak to peak |
| FREQ CV att | -1 to 1 | 0 | 1 V/oct at full |
| SPREAD, TIMBRE CV att | -1 to 1 | 0 | 10 V sweeps the whole knob at full |
| RICHNESS CV att | -1 to 1 | 0 | As above. Input normalled to envelope 1 (10 V). |
| AMP CV att | -1 to 1 | 1 | Input normalled to envelope 2 (10 V). |
| RISE 1, FALL 1, RISE 2, FALL 2 | 0 to 1 | 0.0, 0.55, 0.0, 0.6 | Time = 0.002 × 5000^knob seconds (2 ms to 10 s). Display in ms or s. |
| FALL 1, FALL 2 CV att | -1 to 1 | 0 | Fall time × 2^(att × volts), result clamped to 0.5 ms to 20 s |
| AGE | 0 to 1 | 0.6 | Section 6 |
| DRIVE | 0 to 1 | 0.35 | Section 5.5 |

Combined controls: `spread = clamp(knob + att × cv / 10, 0, 1)`, same for timbre, rich and amp. `pitch = FREQUENCY + V/OCT + att × FREQ CV` in octaves.

Smooth knob values with a 5 ms one-pole. Do not smooth CV inputs.

Right-click menu:
- **STACK voicing**: Octaves (default), Fifths, Harmonics. Section 5.2.
- **New unit**: rolls a new unit seed (section 6.1). Show the current seed as a short hex label.
- **FREQUENCY knob snaps to semitones**: off by default.

Polyphony: channels = the largest channel count among V/OCT, GATE 1 and GATE 2 (at least 1). Every CV input is polyphonic, and a mono cable feeds all voices. GATE 2 is normalled to GATE 1. ENV and END outputs carry one channel per voice. L and R are the sum of all voices, not polyphonic.

Mono rule: if only one of L and R is patched, that jack carries the mono mix (section 5.4).

## 5. The voice

All of this is in `voice()` and `output_stage()` in the reference. Let `fs2` = 2 × engine sample rate.

### 5.1 Pitch of each oscillator i (0 to 4)

```
octave_i = pitch × vtrack_i + ratio_i × sa × strack_i + tune_i + drift_i
f = 261.6256 × 2^octave_i
f = f × 2^(-droop_i × f / 4000 / 1200)       high-frequency droop, cents flat at 4 kHz
f = f × (1 + jitter_i)
f = clamp(f, 0.01, 0.225 × fs2)
dp = f / fs2
```

`vtrack`, `strack`, `tune`, `droop` come from the unit (6.1). `drift` and `jitter` are in 6.2.

### 5.2 SPREAD

`sa = spread³` (so the bottom third of the knob is fine detune, about 0 to 45 cents for the inner pair). `ratio_i` by voicing, in octaves at full SPREAD:

| Voicing | Osc 0 | Osc 1 | Osc 2 | Osc 3 | Osc 4 |
|---|---|---|---|---|---|
| Octaves | -2 | -1 | 0 | +1 | +2 |
| Fifths (just) | -1 | log2(3/2) - 1 | 0 | log2(3/2) | +1 |
| Harmonics | log2(1/3) | log2(2/3) | 0 | log2(4/3) | log2(5/3) |

In Harmonics the centre oscillator is the third harmonic, so the five land on harmonics 1 to 5 of a fundamental a twelfth below the FREQUENCY setting. Say so in the manual.

### 5.3 Waveform

Phase p in [0, 1). The oscillators free-run: never reset phase on a gate. Starting phases come from the unit.

- **Sine**: `tri = 4|p - 0.5| - 1`, `sine = -sin(π/2 × tri × shp_i) / sin(π/2 × min(shp_i, 1))`. With `shp_i` not equal to 1 this is the imperfect shaper. No extra folding code is needed.
- **Saw with RICHNESS**: `k = 1 + 7 × rich²`, limited to `max(1, 0.45 / dp)` so the multiplied saw stays below the band edge. `saw = wrap(k × (2p - 1))`, where wrap maps into [-1, 1).
- **Turnaround glitch**: `w = min(0.1, dp × 40e-6 × fs2)`, `g = (p - 0.5) / w`; if |g| < 4, `gl = age × 0.06 × exp(-g²)`, and `saw -= gl`, `sine -= 0.5 × gl`.
- **Crossfade and saturation**: `y = (1 - timbre) × sine + timbre × 0.8 × saw + dc_i`, then `y = lvl_i × (tanh(od × (y + ob)) - tanh(od × ob)) / tanh(od)` with `od = 0.35 + 1.4 × drive`, `ob = 0.12 × age + 0.1 × drive`.
- **Band-limiting**: the saturation is applied to the raw waveform, and the steps are band-limited afterwards with a two-sample polyBLEP whose height is the step measured after saturation. Steps occur (a) wherever `k(2p - 1)` crosses an odd integer inside a cycle, from saw = +1 to saw = -1 with the sine evaluated at the crossing phase, and (b) at the reset. There can be more than one step per sample, so loop. Use a one-sample output delay so the "before" half of each correction can be added. Follow the reference exactly here, including the wraps that follow a reset within the same sample.
- **Reset rounding**: a one-pole low-pass on each oscillator's output, corner `20000 - age × lp_hz_i` Hz.

The sine and saw are 90 degrees apart at the fundamental on purpose (the saw's reset is at the triangle's lower turnaround). Do not "fix" the phase.

### 5.4 Coupling through the rail

Each oscillator leaves a spike on a shared rail when it resets. Spikes decay with a 160 microsecond time constant. The other oscillators' spikes lower an oscillator's reset threshold:

```
thr_i = 1 - couple × (rail - spike_i)        couple = age × 0.001; rail = sum of all five spikes, from the end of the previous sample
if p + dp >= thr_i: reset now. Fraction of the sample since the reset a = min(1, (p + dp - thr_i) / dp); new phase = a × dp
after all five: spike_j *= decay; if j reset: spike_j += decay^a_j
```

The step for band-limiting at an early reset goes from the shaped value at phase `thr_i` to the shaped value at phase 0. This gives real locking: a pair locks within about 1.7 cents at full AGE, and oscillators also lock at octaves and fifths. This replaces the phase-difference coupling in the older prototype, which could not lock at intervals.

### 5.5 Mix, VCA and output stage (at fs2)

- Stereo: pan positions -0.45, +0.9, 0, -0.9, +0.45 for oscillators 0 to 4, equal power (`gl = cos((pan + 1)π/4)`, `gr = sin(...)`).
- Mono: every oscillator at gain √0.5 into one channel.
- VCA: `a = amp + quiet × 0.0008`, where `quiet = max(0, age - 0.6) / 0.4`. Linear.
- Output stage per channel: `g = 0.30 × (1 + 2.5 × drive)`, `b = 0.10 × age + 0.12 × drive`, `x = (tanh(mix × a × g + b) - tanh(b)) / (0.75 + 1.35 × drive)`.
- Hiss: add white noise of RMS `quiet × 2.2e-4` (measured at the engine rate) after the saturation.
- Decimate by 2 (Rack's `dsp::Decimator<2, 8>` is fine), DC-block at 8 Hz, multiply by OUTPUT × 5 V.

At AGE 0.6 and below with the VCA closed the output must be exactly silent.

A fast tanh approximation is allowed if its error is under 0.001 over ±4.

## 6. AGE

### 6.1 The unit

Each voice has a fixed "unit": nine numbers per oscillator drawn from a seed with splitmix64, in the order given in `make_unit()` in the reference: tune (±5 cents), vtrack (±0.4%), strack (±5%), shp (±10%), dc (±0.02), level (±1.2 dB), lp_hz (6000 to 11000), droop (5 to 20 cents at 4 kHz), starting phase. Those are the values at AGE 1. AGE scales all but the starting phase, as in `unit_arrays()`.

The module seed is random when the module is created, saved in the patch JSON, and changed only by "New unit". Voice c uses `seed + c × 0x9E3779B97F4A7C15`. The C++ splitmix must give the same numbers as the Python for the same seed, and a test must check it.

### 6.2 Drift and jitter

Updated every 32 samples at fs2 with the exact discretisation `x = a × x + √(1 - a²) × gaussian`, `a = exp(-dt / τ)`, so the result does not depend on the sample rate:

```
drift_i (octaves) = age × (1.7 × d1_i + 5.0 × d2_i + 5.0 × d3_i + 3.0 × common) / 1200
τ = 0.3 s, 2.5 s, 25 s per oscillator; 20 s for the one shared wander
```

Jitter: per sample, white gaussian noise through a 400 Hz one-pole, scaled so its RMS after the filter is `age × 0.001156` (2 cents) whatever the sample rate.

### 6.3 Envelopes

State machine in `_env()` in the reference:

- Gate goes high at 1.0 V or more, low below 0.5 V. A rising edge starts the rise from the current level, whatever the stage.
- The rise always completes. The level then holds while the gate is high, then falls.
- `c = 0.6 × age`. Rise rate `(1 + c × (1.3 × (1 - v) - 0.5)) × nr / T`, fall rate `(1 + c × (2.2 × v - 1)) × nf / T`, with `nr = ln((1 + 0.8c) / (1 - 0.5c)) / (1.3c)` and `nf = ln((1 + 1.2c) / (1 - c)) / (2.2c)` (both 1 when c is 0). These keep the knob time true while the shape bends.
- At each rising edge draw a scatter `1 + age × 0.06 × uniform(-1, 1)` that multiplies both times for that note.
- END: a 1 ms, 10 V pulse when the fall reaches zero. Patching END to the same envelope's GATE must loop.
- ENV outputs are 0 to 10 V.

## 7. Tests (`make test`, no Rack needed)

Follow the shape of `CicadaFable/test/render.cpp`. Every check prints PASS or FAIL. Required checks:

1. Unit numbers for seeds 1, 2 and 12345 match the Python to 1e-12.
2. Clean engine (AGE 0, DRIVE 0), saw at C6: worst non-harmonic component below 20 kHz at least 55 dB under the fundamental (the reference gives 64.6 dB).
3. Tuning: at AGE 0, centre oscillator within 0.1 cent of the commanded pitch from C1 to C7. SPREAD full gives exact octaves in Octaves voicing.
4. Locking, drift and jitter off, AGE 1: two oscillators 1.0 cent apart end within 0.1 cent of each other. At an octave plus 1.0 cent, the same. At 4 cents apart they stay apart (more than 3 cents).
5. Mono: each oscillator alone has the same level in the mono mix within 0.1 dB.
6. Envelopes: rise 0.5 s and fall 2 s measure within 1% at AGE 0, and the mean over 40 notes within 1% at AGE 0.6 and AGE 1. END patched to GATE loops. Retrigger during the fall rises from the current level.
7. Closed VCA: exactly silent at AGE 0.6. At AGE 1 the floor is between -72 and -65 dBFS.
8. Agreement with the reference: third-octave band levels (same measure as Cicada's `ref_bands.py`) within 1 dB from 50 Hz to 16 kHz for five renders made by a new `tools/proto/ref_bands.py`: unison saw, octave stack sines, richness at 0.7, timbre at 0.5 in a light swarm, and the bass line. Use long renders (10 s) because the noise sources are not sample-identical.
9. Sample rates: checks 2 and 3 also pass at 44.1 kHz and 96 kHz.
10. Poly: 16 voices at 48 kHz run faster than real time by a factor of at least 5 on the build machine. Report the figure.
11. No NaN or infinity with every control and CV at its extremes, including ±12 V on every input.

## 8. Presets (12)

Factory presets as .vcvm files, numbered. Each must make sound from a gate on GATE 1 and a pitch on V/OCT, except the drones, which sound on their own.

1. Init Swarm (the defaults)
2. Tight Unison (SPREAD 0, AGE 0.6)
3. Supersaw Pad (SPREAD 0.3, slow rise and fall)
4. Octave Organ (TIMBRE 0, SPREAD 1)
5. Sync Sweep Bass (envelope 1 into RICHNESS)
6. Fifths Stack (voicing Fifths, SPREAD 1)
7. Harmonic Bells (voicing Harmonics, TIMBRE 0, short falls)
8. Tired Unit (AGE 1, DRIVE 0.75)
9. Factory Fresh (AGE 0, DRIVE 0)
10. Driven Drone (AMPLITUDE up, no gate needed)
11. Sine Swarm (TIMBRE 0, SPREAD 0.25)
12. Looper (notes say: patch END 1 to GATE 1 and ENV 1 to SPREAD)

## 9. Manual

MANUAL.md and a PDF made with the Cicada tooling. Plain language, full sentences. Contents: what it is, a panel tour, each control, AGE and DRIVE explained in terms of what you hear, polyphony, the three voicings, the presets, and at least eight patch recipes, including: velocity into TIMBRE, a looping envelope into SPREAD, a poly pad from a MIDI keyboard, the sync-sweep bass, a self-playing drone with slow LFOs on SPREAD and TIMBRE, an additive organ in Harmonics voicing, and two units layered with different seeds. State what is a guess about the hardware (the SPREAD range and the RICHNESS circuit).

## 10. Order of work

1. Port the engine to `src/dsp/Voice.hpp` with no Rack dependency, and the tests. Get `make test` green.
2. Module, panel, presets.
3. Push, confirm the Mac arm64 CI build is green and the plugin is on the preview release.
4. Manual, PDF, README, demo WAVs from the C++ engine into `/mnt/project-files/sawstack-vcv/demos/`.
5. Record any deviation from this spec in the README.
