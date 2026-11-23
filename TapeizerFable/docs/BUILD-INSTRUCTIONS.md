# Tapeizer: build instructions

Design by Fable, 2026-11-23. For the model that builds it. Read `RESEARCH.md` (the proposal) for why things are the way they are. Where the two differ, this document wins.

The **reference engine** is `tools/proto/tape_ref.py`. It is the definition of the sound. Where prose here is ambiguous, the Python decides. `tools/proto/check_ref.py` prints the measurements the reference gives (`check_ref_48000.txt`, `check_ref_96000.txt`), and the C++ must match them as section 9 says. `tools/proto/make_reference.py` renders music through the reference.

Hardy's decisions that shaped this spec:
- One wide module with two tape decks, a poly TRACKS path, a HISS knob that turns off, name Tapeizer.
- Machines asked for by name: the Fostex 260 (here "4TR C") and a Tascam eight-track (both kinds are here: the cassette one as "8TR CASS", the quarter-inch one as "8TR 7.5").
- The first sketches were too extreme. There must be one control for the strength of the whole lo-fi character. That is **STRENGTH**, and the default is gentle.
- 12 presets.

## 1. What to build

- Plugin folder `TapeizerFable` in hardyfinn/2026SoloModules, on this session's branch (not main). Use `SawstackFable` on branch `claude/project-thread-qvvmoz` as the template for the Makefile, plugin.json, panel script, logo, offline test harness, PDF tooling and README. The repository workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Check the workflow on that branch and carry it over if this branch lacks it. Apple silicon only.
- plugin.json: slug `TapeizerFable`, name `Tapeizer Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `Tapeizer`, name `Tapeizer`, tags Effect, Dynamics, Distortion, Noise, Polyphonic. Description: "Cheap 80s and 90s tape recording in one module: desk, noise reduction, a multitrack deck and a master deck, with STRENGTH and AGE".
- No brand names on the panel or in plugin.json: not Tascam, Fostex, Dolby or dbx. The noise reduction types are labelled B, C and 2:1. The manual and README may say once, in prose, which hardware each setting is modelled on, and that the module is not affiliated with any of them.
- Panel about 50 HP (adjust to fit), text labels, Haer logo.
- Deliverables: C++ module, panel SVG from a script, 12 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo WAVs rendered by the C++ engine.

## 2. Signal flow

```
IN L/R ─────────┐
                ├─► DESK ─► MULTITRACK deck, BOUNCE passes ─► mix ─► MIX OUT L/R
TRACKS (poly) ──┘      (NR encode ► tape ► NR decode, per pass)   │
                                   └─► TRACKS OUT (poly)          │
MASTER IN L/R (normalled to the mix) ◄────────────────────────────┘
        └─► BUS compressor ─► MASTER deck ─► MIX (dry/wet) ─► OUTPUT ─► OUT L/R
```

- Levels: 5 V peak = +3 dB over 0 VU. Internal signal = volts × 0.2828; outputs = internal / 0.2828.
- IN R is normalled to IN L.
- Tracks on the multitrack tape, in head order: TRACKS channels 0 to n-1 (n at most 8), then IN L, then IN R. IN L/R are only on the tape when at least one of them is patched. All tracks share one transport (same wow), leak into their neighbours in that order, and the first and last are edge tracks.
- The mix: IN L and IN R tracks go to left and right. TRACKS channels are added equally to both sides, unless TRACKS OUT is patched, in which case they are left out of the mix (the user mixes them outside and can return through MASTER IN).
- TRACKS OUT carries each TRACKS channel after the deck, n channels.
- Each section has a bypass button with a light: DESK, MULTITRACK, BUS (off by default), MASTER. A bypassed section passes its input straight through with no latency change artefacts beyond a click-free 10 ms crossfade.
- MIX: dry is IN L/R (plus nothing else), delayed by the module's current nominal latency so the blend does not comb more than the wow makes it.

## 3. Controls

| Section | Control | Range | Default | Notes |
|---|---|---|---|---|
| Global | STRENGTH | 0 to 1 | 0.4 | Section 5. Large knob. CV. |
| Global | AGE | 0 to 1 | 0.2 | Section 6. Large knob. CV. |
| Global | MIX | 0 to 1 | 1 | |
| Global | OUTPUT | -12 to +12 dB | 0 | |
| DESK | TRIM | 0 to 24 dB | 0 | Drive into the op-amp clip, compensated |
| DESK | LOW, HIGH | ±15 dB | 0 | Shelves |
| DESK | LOW FREQ | 80 to 1200 Hz | 100 | Log scale |
| DESK | HIGH FREQ | 700 Hz to 10 kHz | 10 kHz | Log scale |
| NR | TYPE | OFF, B, C, 2:1 | 2:1 | For the multitrack deck |
| NR | ENC, DEC | on/off | on, on | Separate on purpose |
| NR | CAL | ±6 dB | 0 | Level error between tape and decoder |
| MULTITRACK | MACHINE | 4TR, 4TR HI, 4TR C, 8TR CASS, 8TR 7.5, 8TR 15 | 4TR | Snap knob |
| MULTITRACK | REC | ±12 dB | 0 | Large knob. CV. Output is compensated |
| MULTITRACK | TAPE | I, II, IV | II | |
| MULTITRACK | BIAS | -1 to +1 | 0 | |
| MULTITRACK | BOUNCE | 1 to 4 | 1 | Snap knob. Whole deck run that many times in series |
| TAPE PATH | WOW, FLUTTER, DROPOUT, HISS | 0 to 1 | 0.5 | 0.5 = the machine's own amount, 0 = none, 1 = four times. Both decks. WOW has CV |
| TAPE PATH | AZIMUTH | -1 to +1 | 0 | ±20 arc minutes, both decks. CV |
| TAPE PATH | WEAR | 0 to 1 | 0 | Condition of the tape, both decks |
| BUS | THRESH | -24 to 0 dB re 0 VU | -8 | |
| BUS | RATIO | 1.5 to 20 | 4 | |
| MASTER | MACHINE | DECK, BOOMBOX, 1/4 INCH, DAT | DECK | Snap knob |
| MASTER | REC | ±12 dB | 0 | CV |
| MASTER | TAPE | I, II, IV | II | Ignored by DAT |
| MASTER | NR B | on/off | on | B-type encode and decode. Ignored by DAT |

CV inputs (±5 V sweeps the whole knob, added to the knob, clamped): STRENGTH, AGE, REC, MASTER REC, WOW, AZIMUTH. Mono.

Audio jacks: IN L, IN R, TRACKS (poly in), MASTER IN L, MASTER IN R, TRACKS OUT (poly), MIX OUT L, MIX OUT R, OUT L, OUT R.

Meters: a five-light VU for the multitrack deck (the loudest track, level on tape after NR encode) and one for the master deck, at -10, -5, 0, +3 VU in green/yellow and a red peak light when the instantaneous level exceeds the tape's MOL. VU ballistics: rectified average, 300 ms, a sine of peak 1.0 reads 0. A gain reduction light on BUS. A bypass light per section.

Right-click menu: **Hum** 50 Hz (default) or 60 Hz. **New unit** (rolls the seed, show it as short hex). **Oversampling** is fixed at 2 and not offered.

Smooth knobs with a 20 ms one-pole. Switch and machine changes crossfade (section 7.4).

Panel order left to right: inputs, DESK, NR, MULTITRACK (with its VU), TAPE PATH, BUS, MASTER (with its VU), Global (STRENGTH and AGE large and prominent), outputs on a darker plate. Lay it out with a script, render a preview PNG, look at it, and fix collisions before finishing.

## 4. The deck

Everything is in `deck()` in the reference. One pass, per track, at engine rate fs unless noted:

1. Multiply by the REC gain.
2. NR encode (section 8).
3. Record EQ: a linear-phase FIR (`fir_design`), gain `min(1 / nominal_loss, cap)` with a soft 21 kHz roll-off, raised to the power STRENGTH.
4. Upsample by 2 (Rack's `dsp::Upsampler<2, 8>`).
5. At 2 fs: the tape curve with a play operator for memory, then modulation noise, then the transport delay line read with cubic Lagrange interpolation. One delay time for all tracks plus a per-track skew from azimuth and weave. See `_core`.
6. Downsample by 2 (`dsp::Decimator<2, 8>`).
7. Playback loss: a second FIR from the wavelength loss formula `loss()` (thickness, spacing, gap, azimuth) for the machine as it now is, raised to the power STRENGTH.
8. Drop-outs: two one-poles whose corner falls as the extra spacing grows (`_dropouts`), then slow level instability, then the unit's sensitivity error.
9. Head bump (peaking biquad) and a 2-pole high-pass.
10. Crosstalk from neighbouring tracks, bass-heavy.
11. Hiss (white noise through a 1.5 kHz to 7 kHz shelf and a 17 kHz low-pass) and hum (fundamental plus two harmonics).
12. CAL gain and the unit's calibration error.
13. NR decode.
14. Divide by the REC gain.

The machine table, tape table, every constant and every AGE and STRENGTH scaling are in the reference. Copy them exactly. Points that need care:

- **FIR design**: 513 frequency points, inverse real DFT of length 1024, Hann window, 63 taps up to 50 kHz and 127 above. Precompute the cosine table. Redesign only when an input to the curve has changed (machine, tape, bias, age, wear, azimuth, strength), check at most every 256 samples, and crossfade old to new over 256 samples by running both.
- **Noise sources**: the reference normalises filtered noise to unit RMS by measuring it. In C++ compute each filter's noise gain once (sum of squares of its impulse response) and divide by it. Sources: slow wow noise (1.2 Hz low-pass), capstan phase wander (0.5 Hz), flutter (8 to 45 Hz band), scrape (2.5 to 6 kHz band), weave (0.7 Hz), modulation noise (2.5 kHz), contact noise (30 Hz), level instability (4 Hz). All are 2-pole Butterworth.
- **Leaky integrators** for the wow and flutter noise have a 2 s time constant.
- **Base delay**: `0.5 ms + skew allowance + STRENGTH × 4 × sig`, where `sig` is the expected size of the delay swing as the reference computes it. Smooth the base with a 0.5 s one-pole so turning WOW bends the pitch gently instead of clicking. Clamp the total to 45 ms.
- **Latency** of one quiet pass is (taps - 1) + 0.5 ms + the oversampler's delay. Report the module's nominal latency for the MIX dry path as the sum over active passes.
- **BOUNCE**: each pass has its own state and its own noise streams. The unit errors are the same for every pass (same machine).
- **DAT** on the master deck replaces the deck with `dat()`: 8-pole 20 kHz low-pass, clip at +15 dB over 0 VU, 16-bit TPDF.
- A fast tanh is allowed if its error is under 0.001 over ±4.
- Denormals: flush or add a tiny offset in the one-poles.

## 5. STRENGTH

One number S from 0 to 1 that scales every departure from a perfect machine, in both decks at once. At 0 a deck is transparent apart from its latency. At 1 it is the real machine. The reference applies it as follows, and the C++ must do the same:

| What | How S acts |
|---|---|
| Record EQ and playback loss | Both curves raised to the power S (so decibels scale by S) |
| Saturation | Saturation level Ms divided by max(S, 0.02) |
| Under-bias memory | Blend h × S |
| Wow, flutter, scrape, weave, azimuth skew | Delay swing × S |
| Modulation noise, level instability, crosstalk, hiss, hum | Amplitude × S |
| Drop-outs | Depth and contact noise × S |
| Head bump | Gain in dB × S |
| Low corner | max(8 Hz, corner × S) |
| Unit errors (sensitivity, calibration) | Decibels × S |

STRENGTH does not touch the DESK, the BUS compressor, the CAL knob, the NR switches or DAT. Those are the user's own choices.

## 6. AGE and the unit

`make_unit(seed)` draws the fixed errors of this particular machine with splitmix64: per track a sensitivity error and a calibration error, then the sign of the azimuth error and a bias drift. The module seed is random at creation, saved in the patch, changed only by "New unit". The multitrack deck uses `seed`, the master deck `seed + 100`. The C++ must give the same numbers as the Python, and a test checks it.

AGE scales, as in the reference: azimuth error (2 to 18 arc minutes), head dirt (extra spacing), gap wear, bias drift, wow (× up to 6), pinch roller wow, weave, scrape, modulation noise, level instability, crosstalk (+8 dB), hiss (+4 dB), hum (-78 to -56 dB), a magnetised head (even harmonics), under-bias, drop-out rate, and the per-track sensitivity and calibration errors that make the noise reduction mistrack.

## 7. Other sections

### 7.1 DESK
`desk()`: TRIM gain, 2 times oversampled clip `clip(1.5 tanh(x / 1.5) + 0.03 x², -1.6, 1.45)`, divide by the TRIM gain, then RBJ low and high shelves at the swept frequencies. Applied to every track.

### 7.2 BUS
`bus_comp()`: stereo-linked peak detector, 3 ms attack, 120 ms release, hard knee, make-up of half the reduction at threshold.

### 7.3 VU and peak
Section 3.

### 7.4 Switching
Changing a machine, tape type, NR type or a bypass must not click: crossfade over 10 ms, or mute-ramp 5 ms down and 5 ms up while state resets.

## 8. Noise reduction

In the reference: `_x2` (2:1 compander with 400 Hz to 1.6 kHz pre-emphasis outside it, band-limited treble-weighted detector that always reads the tape-side signal, 5 ms up and 30 ms down), `_slide` (a sliding-band stage: side chain of gain 2.16 through a one-pole high-pass whose corner rises with level; the decoder is solved without a delay). B is one stage at 1.8 kHz. C is an anti-saturation shelf plus two stages at 450 Hz with thresholds 20 dB apart. These are simplified models, and the manual must say so.

With ENC on and DEC off, or the reverse, the signal is deliberately wrong. That is a feature.

## 9. Tests (`make test`, no Rack needed)

Follow the shape of `SawstackFable/test/render.cpp`. Every check prints PASS or FAIL. "Quiet" means all random sources off, AGE 0, STRENGTH 1 unless stated, as `quiet=True` in the reference. Expected numbers are in `check_ref_48000.txt`.

1. Unit numbers for seeds 1 and 12345 match the Python to 1e-12.
2. Quiet response of every machine at -20, 0 and +6 dB, at 40 Hz, 100 Hz, 1, 5, 10 and 14 kHz: within 0.4 dB of the reference (0.8 dB at 14 kHz, where the oversampling filters differ).
3. STRENGTH 0: every machine within 0.1 dB of flat from 40 Hz to 14 kHz at +6 dB. STRENGTH 0.5 on 4TR matches the reference within 0.4 dB.
4. Third harmonic at 315 Hz: 2.3% to 2.7% at MOL and 0.2% to 0.35% at MOL - 10 dB, tapes I, II and IV.
5. BIAS: -1 gives -3.5 dB (±0.5) gain at -30 dB, 315 Hz, and about +1.4 dB at 10 kHz; +1 gives about -4.4 dB at 10 kHz.
6. AZIMUTH 0, 0.5 and 1 on 4TR at 5, 10 and 14 kHz match the reference within 0.5 dB (1.5 dB at the -25 dB point).
7. Noise reduction: encode then decode returns the input, error below -60 dB, for B, C and 2:1. Encode gains at 200 Hz, 1 kHz and 5 kHz at -60, -40, -20 and 0 dB match the reference within 0.3 dB.
8. Latency of a quiet pass equals the documented figure, and the MIX dry path is aligned to it within one sample.
9. Statistics with the random sources on (10 s renders, 4TR, AGE 0.25, STRENGTH 1): hiss level with no input within 1 dB of `hiss + tape + 4 × age`; weighted speed deviation between half and twice the machine's `wow` figure × (1 + 5 × age^1.5); drop-out count within a factor of two of the rate.
10. Sample rates: checks 2 and 4 also pass at 44.1 kHz and 96 kHz.
11. Crosstalk: a 1 kHz tone on one track appears on its neighbour at the machine's `xt` figure within 1.5 dB at AGE 0, and not at all on a track two away.
12. Speed: stereo, BOUNCE 1, both decks on, at 48 kHz, faster than real time by at least 10. Eight TRACKS at BOUNCE 4 faster than real time. Report the figures.
13. No NaN or infinity with every control and CV at its extremes and ±12 V on every input. Bypassing and switching machines during signal produces no sample jump larger than the signal itself.

## 10. Presets (12)

Factory presets as .vcvm files, numbered. Unlisted controls stay at their defaults.

| # | Name | Settings |
|---|---|---|
| 1 | Four-Track To Cassette | The defaults |
| 2 | Two Decks 1985 | STRENGTH 0.5. Multitrack 4TR, tape I, NR OFF, REC +3. Master DECK, tape I, NR B off, REC +3 |
| 3 | Clean Four-Track | STRENGTH 0.25. 4TR HI, 2:1. Master DAT |
| 4 | In The Red | STRENGTH 0.5. TRIM 8, REC +9, master REC +6 |
| 5 | 260 Session | STRENGTH 0.45. 4TR C, NR C, REC +4, LOW +2 at 100 Hz, HIGH +2 at 8 kHz. Master DAT |
| 6 | Eight On A Cassette | STRENGTH 0.5. 8TR CASS, 2:1, REC +2 |
| 7 | Studio Eight | STRENGTH 0.55. 8TR 7.5, 2:1, REC +6. Master 1/4 INCH, NR B off, REC +5 |
| 8 | Budget Reel | STRENGTH 0.5. 8TR 15, NR C, REC +6. Master 1/4 INCH, NR B off, REC +4 |
| 9 | Third Bounce | STRENGTH 0.45. 4TR HI, BOUNCE 3, REC +3 |
| 10 | Boombox Dub | STRENGTH 0.5. MULTITRACK bypassed. Master BOOMBOX, tape I, NR B off, REC +5 |
| 11 | Bright Trick | STRENGTH 0.4. NR B, ENC on, DEC off. Master NR B off |
| 12 | Neglected | STRENGTH 0.7, AGE 1, WEAR 0.5 |

## 11. Manual

MANUAL.md and a PDF made with the Sawstack tooling. Plain language, full sentences. Contents: what it is and how it pairs with Vinylizer (Tapeizer first, Vinylizer after), the signal path with a diagram, STRENGTH and AGE first and in terms of what you hear, each section and control, the machines and what each is modelled on, the tape types, noise reduction and its deliberate misuses, the TRACKS path, the presets, latency, what is simplified (the B and C models, the tape curve, the unverified constants listed in the proposal), and at least eight patch recipes, including: a drum machine through the defaults; stems on TRACKS mixed in a Rack mixer and returned to MASTER IN; the two-deck method; bouncing; the bright trick; LFO on AZIMUTH for phasing; envelope on REC for level-dependent treble; Tapeizer into Vinylizer for the full path to the record.

## 12. Order of work

1. Port the engine to `src/dsp/` with no Rack dependency, and the tests. Get `make test` green.
2. Module, panel, presets.
3. Push, confirm the Mac arm64 build is green and the plugin is on the preview release.
4. Manual, PDF, README, demo WAVs from the C++ engine into `/mnt/project-files/tape-vcv/demos/`.
5. Record any deviation from this spec in the README.
