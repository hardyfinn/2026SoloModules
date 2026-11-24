# Essphor: build instructions

Design by Fable, 2026-11-24. For the model that builds it. Read `RESEARCH.md` for why things are the way they are. Where the two differ, this document wins.

The **reference engines** are `tools/proto/mosaic_proto.py` (grains) and `tools/proto/res_proto.py` (resonator). They define the sound. Where prose here is ambiguous, the Python decides. `check_proto.py`, `check_res.py` and the feedback check print the measurements the references give (`check_proto.txt`, `check_res.txt`, `check_feedback.txt`, `check_age.txt`); the C++ must match them as section 9 says. `make_sketches.py` and `make_sketches2.py` render music through the references.

The references run at 44.1 kHz and in blocks of 32 samples. The C++ runs per sample at any Rack sample rate. Every time constant below is in seconds or Hz, not samples.

Hardy's decisions that shaped this spec:
- The granular device of a certain four-track "sculpting sampler", plus its 48-band tuned resonator, as separate modules. Not the tape, colour or space stages, and not four tracks.
- Out of the box the grain module behaves like the hardware (4 second buffer, 64 grains). Additions sit beside that: V/OCT, a grain trigger output, longer buffers, loading a WAV, saving the buffer.
- A small companion module with the hardware's four modulators.
- An AGE knob. Default 0, because the hardware is clean.
- Name: Essphor.

## 1. What to build

- Plugin folder `EssphorFable` in hardyfinn/2026SoloModules, on this session's branch (not main). Use `TapeizerFable` (already in this tree) as the template for the Makefile, plugin.json, panel script, logo, offline test harness, PDF tooling and README. The repository workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `EssphorFable`, name `Essphor Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. Three modules:

| Slug | Name | Tags | Description |
|---|---|---|---|
| `Essphor` | Essphor | Granular, Effect, Delay, Sampler, Clock | "Live rhythmic granular processor: clock-locked grains, scale-snapped pitch, stretch, position and delay scanning, lock, feedback, AGE" |
| `EssphorBank` | Essphor Bank | Filter, Effect, Equalizer | "Morphing 48-band tuned filter bank and resonator with scales, ring time and AGE" |
| `EssphorMod` | Essphor Mod | LFO, Random, Envelope follower, Envelope generator, Clock | "Four modulators: wave, random steps, envelope follower and ADSR, each with a second spread output" |

- No hardware brand or product names on the panels or in plugin.json. The manual and README may say once, in prose, which instrument the modules are modelled on, and that they are not affiliated with it.
- Panels: text labels, Haer logo. Essphor about 32 HP, Essphor Bank about 16 HP, Essphor Mod about 26 HP. Adjust to fit.
- Deliverables: C++ modules, panel SVGs from a script, factory presets (12 for Essphor, 8 for Essphor Bank, 4 for Essphor Mod), offline tests (`make test`), MANUAL.md, one PDF manual for the plugin with patch recipes, README, demo WAVs rendered by the C++ engine.
- The three modules are independent and connect with cables. Do not use Rack's expander mechanism.

Voltage conventions for all three: audio internal = volts / 5. CV inputs add to their knob, with ±5 V covering the knob's whole range, and the sum is clamped to the range. No attenuverters. Gates and triggers at 1 V threshold (Rack's Schmitt trigger). R inputs are normalled to L.

---

# Part A. Essphor (grains)

## 2. Signal flow

```
IN L/R ─┬──────────────────────────────────────────────► dry ─┐
        └─► AGE record path ─►(+)─► circular buffer ─► grains ─┴─► WET mix ─► soft limit ─► OUT L/R
                               ▲                         │
                               └──── FEEDBACK ◄──────────┘
```

- One circular stereo buffer of B seconds (4 by default; menu: 4, 8, 16). A write head goes round it at one sample per sample unless LOCK is on.
- Up to 64 grains. Each reads the buffer at its own start, speed and direction, under its own window and stereo gains. The wet signal is their sum.
- Output: `out = softlimit(mix(dry, wet))` then × 5 V. `softlimit(x)`: x for |x| ≤ 1.6, then a smooth knee to a ceiling of 2.4 (use `1.6 + 0.8*tanh((|x|-1.6)/0.8)` with the sign of x).

## 3. Controls

Top row, large knobs. Bottom row, medium knobs. "CV" means a jack for it.

| Control | Range | Default | Notes |
|---|---|---|---|
| PITCH | −36 to +36 semitones | 0 | CV. Plus V/OCT (12 semitones a volt). The sum is quantised by SCALE. |
| RATE | SYNC: 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 (snapped; 1/4 is one beat). FREE: 0.25 to 250 Hz, exponential | 1/16 | CV. RATE MODE switch SYNC / FREE. |
| SIZE | 1/64 of a beat to 8 beats, exponential, not snapped | 1/4 beat | CV. Always in beats of the current tempo. Clamped to 2 ms at the bottom and B/2 at the top. |
| CONTOUR | −1 to +1 | 0 | CV. |
| SCAN | by mode, below | STRETCH 50% | CV. SCAN MODE switch, four positions. |
| SPRAY | 0 to 1 | 0 | CV. SPRAY MODE switch, four positions. |
| PATTERN | 0 to 1 | 0 | CV. |
| WET | 0 to 1 | 0.5 | CV. WET MODE switch MIX / SEND. |
| FEEDBACK | 0 to 1 | 0 | CV. FEEDBACK MODE switch GRAIN / SOS. |
| RAND PITCH | 0 to 1 | 0 | CV. RAND PITCH MODE switch DETUNE / RANDOM / RANDOM MONO. |
| RAND RATE | 0 to 1 | 0 | CV. |
| RAND SIZE | 0 to 1 | 0 | CV. |
| RAND AMP | 0 to 1 | 0 | CV. |
| RAND REV | 0 to 1 | 0 | CV. |
| AGE | 0 to 1 | 0 | CV. Section 6. |
| SCALE | 20 positions, snap knob | CHROMATIC | A small text display shows the scale name. |
| TEMPO | 30 to 300 BPM | 120 | Small knob. Used only while CLOCK is unpatched. |
| LOCK | latching button with light | off | LOCK gate input: high = locked. Button and gate are OR-ed. |

SCAN by mode:

| Mode | Knob meaning | Range | Default |
|---|---|---|---|
| STRETCH | playhead speed as a fraction of the write head's speed | −2 to +2, linear, 0 at centre | 0.5 |
| POSITION | fixed place in the buffer | 0 to 1 of B | 0.5 |
| DELAY SYNC | distance behind the write head, in the same nine divisions as RATE, plus 1/8 dotted and 1/4 dotted (eleven steps, snapped) | 1/64 to 4/1, capped at B − 50 ms | 1/4 |
| DELAY FREE | distance behind the write head in seconds | 0 to B − 50 ms, quadratic taper | 0.25 s |

The knob keeps its 0 to 1 position when the mode changes; only the meaning changes.

Scales (canonical list for both Essphor and Essphor Bank, pitch classes above the root): CHROMATIC 0-11; MAJOR 0 2 4 5 7 9 11; MINOR 0 2 3 5 7 8 10; DORIAN 0 2 3 5 7 9 10; LYDIAN 0 2 4 6 7 9 11; SUPER LOCRIAN 0 1 3 4 6 8 10; MIXOLYDIAN 0 2 4 5 7 9 10; HEX AEOLIAN 0 3 5 7 8 10; HEX DORIAN 0 2 3 5 7 10; BLUES 0 3 5 6 7 10; PENTATONIC 0 3 5 7 10; HIRAJOSHI 0 2 3 7 8; KUMOI 0 2 3 7 9; IWATO 0 1 5 6 10; WHOLE TONE 0 2 4 6 8 10; PELOG 0 1 3 7 8; TETRATONIC 0 4 7 11; FIFTHS 0 7; OCTAVES 0; FREE (no quantising).

In Essphor the scale applies to the pitch *shift*: a shift of 0 is always allowed, and other shifts snap to the nearest interval of the scale (`quantise()` in the reference; ties go down).

Jacks: IN L, IN R, OUT L, OUT R, CLOCK, RESET, LOCK, V/OCT, fifteen CV inputs (the fourteen row knobs and AGE), GRAIN (output).

Lights: LOCK, CLOCK (blinks on each beat), GRAIN (flashes per grain; brightness by grain level).

Context menu: buffer length 4 / 8 / 16 s; "Clock input: 1, 2, 4 or 24 pulses per beat" (default 1); "New pattern"; "Load WAV into buffer" (locks the buffer, resamples the file to the engine rate, truncates or zero-pads to B); "Save buffer with patch" (off by default; when on and LOCK is on, the buffer is stored beside the patch using Rack's patch storage directory, and reloaded locked); "Grains keep running without a clock" (default on).

## 4. Tempo, grid and grain timing

- Beat phase φ counts beats. With CLOCK patched, tempo is measured from the pulse interval (average of the last two intervals) and φ is corrected at each pulse to the nearest pulse position. If the correction is more than a tenth of a pulse, jump; otherwise slew over 20 ms. Without CLOCK, φ runs at TEMPO.
- RESET sets φ to 0, sets the pattern step to 0 and realigns the grain grid.
- SYNC: the grid is every `div` beats from φ = 0. FREE: an independent phase running at RATE in Hz; the grid is each whole number of that phase.
- Grain k fires at grid time k plus jitter. Jitter = `u * 0.5 * RAND RATE` of one grid interval, u uniform in ±1, drawn when the previous grain fires. Jitter never accumulates: the grid itself is untouched. AGE adds its own timing error (section 6).
- If the clock stops for more than two seconds and "keep running" is off, stop firing grains. Sounding grains finish.
- GRAIN output: a 1 ms, 10 V pulse at each grain start.

## 5. The grain, at the moment it fires

Everything is decided once, when the grain fires. Follow `process()` in `mosaic_proto.py` line by line; this is the same thing in words.

1. **Length.** `L = SIZE in seconds × 2^(−3 × u × RAND SIZE)`, u uniform 0 to 1. Minimum 2 ms.
2. **Pitch.** `semi = PITCH + V/OCT×12 + pattern[step mod 16] × 24 × PATTERN`. RAND PITCH in RANDOM or RANDOM MONO adds `u × 24 × RAND PITCH` (u in ±1) before quantising. Quantise to SCALE. In DETUNE mode add `u × RAND PITCH` semitones (±100 cents) after quantising. Clamp to ±36. Speed ratio `r = 2^(semi/12)`.
   The pattern is 16 values uniform in ±1 from a seed created with the module, saved in the patch, and rolled again by "New pattern". `step` advances by one per grain.
3. **Direction.** Reversed with probability RAND REV.
4. **Playhead.** STRETCH: a running position that advances by SCAN samples per sample, wrapping round the buffer. While locked it still advances, so a locked buffer loops at that speed. POSITION: `SCAN × B`. DELAY modes: write head minus the delay.
5. **Spray.** RANDOM modes: offset `u × SPRAY × 1 s`. WARP modes: offset `warp₀ × SPRAY × 3 s`, where warp₀ and warp₁ are two independent smooth random signals (white noise through two one-pole low-passes of 0.35 s each, as in the reference). Stereo position: RANDOM gives `u × min(1, 2 × SPRAY)`; WARP gives `clip(3 × warp₁) × min(1, 2 × SPRAY)`; the MONO modes give 0. RAND PITCH in RANDOM (stereo) mode adds another `u × min(1, 2 × RAND PITCH)`. Clamp to ±1.
6. **Keep clear of the write head** (only while unlocked). Let d be how far behind the write head the grain starts. A forward grain needs `GUARD + max(0, (r−1)L) ≤ d ≤ B − GUARD − max(0, (1−r)L)`. A reversed grain reads backwards from its start, away from the write head, so it needs `GUARD ≤ d ≤ B − GUARD − (1+r)L`. If no d satisfies this, halve L until one does. Clamp d into the range. GUARD is 1 ms.
7. **Level.** `gain = (1 − u × RAND AMP) × comp`, u uniform 0 to 1. `comp = max(1, rate × size)^(−(1 − 0.5 × decor))`, with rate in grains per second, size in seconds, and `decor = min(1, 8×SPRAY + 4×PATTERN + 4×RAND PITCH + 2×RAND REV + |semi|)`. Identical overlapping grains add in amplitude, scattered ones in power; this follows it.
8. **Stereo.** Equal power: `gL = gain × √2 × cos((pan+1)π/4)`, `gR = gain × √2 × sin((pan+1)π/4)`. The grain reads both buffer channels; L uses gL, R uses gR.
9. **Window.** `peak = clamp(0.5 + 0.5 × CONTOUR, e, 1 − e)` with `e = min(0.45, 1 ms / L)`. For grain phase p from 0 to 1: `u = p/peak` before the peak, `(1−p)/(1−peak)` after. `w = (1 − |CONTOUR|) × (0.5 − 0.5 cos(πu)) + |CONTOUR| × u`.
10. **Voice.** Use a free voice. If all 64 are busy, steal the one nearest its end.

While sounding, a grain reads the buffer with 4-point Hermite interpolation (the reference uses linear; Hermite is required). No anti-alias filter on fast reads: a grain three octaves up aliases, as it does on hardware of this kind.

### Recording, lock and feedback

- Unlocked: `buffer[w] = rec + fb`, where `rec` is the input after the AGE record path.
- LOCK on: over 10 ms, fade what is written from new audio to the audio already there, then stop the write head. LOCK off: start the write head and fade from old to new over 10 ms. This keeps the join click-free. While locked the guard in step 6 is off and FEEDBACK does nothing.
- FEEDBACK MODE GRAIN: `fb = tanh(hp30(wet) × 2 × FEEDBACK × lim)`. `hp30` is a one-pole high-pass at 30 Hz. `lim = min(1, 0.5 / env)`, where env is the peak of the wet signal with instant attack and a 0.2 s release. The reference feeds back the previous 32-sample block; per sample, use the previous sample. `check_feedback.txt` shows the result: at FEEDBACK 1 plain grains hold almost level, scattered grains swell to about −10 dB and stay there, and nothing passes −2 dB.
- FEEDBACK MODE SOS: `buffer[w] = softclip(rec + FEEDBACK × buffer[w])`, softclip linear to ±1 then tanh-shaped to ±1.5. At 1 the buffer layers forever.

### WET

MIX: `out = dry × cos(WET × π/2) + wet × sin(WET × π/2)`. SEND: `out = dry + wet × WET`.

## 6. AGE on Essphor

At 0 every item below is bypassed exactly. The picture is an early digital sampler that has not been serviced: narrow converters, short words, a wandering clock.

| Item | Law |
|---|---|
| Record bandwidth | Two cascaded one-pole low-passes at `18 kHz × (5/18)^AGE` |
| Input drive | `tanh(x × d) / d`, `d = 1 + 2 × AGE` |
| Hiss | White noise at `−84 + 34 × AGE` dB re full scale, added before the buffer, so it is granulated too |
| Word length | `bits = 16 − 7 × AGE`; round to that step, with uniform dither of `(1 − AGE)` of a step |
| Clock drift | Every grain's pitch gets `+0.35 × AGE × clip(3 × drift)` semitones; drift is white noise through two one-pole low-passes of 1.5 s |
| Loose timing | Every grain's start time gets a normal error of `4 ms × AGE` rms |

Order in the record path: bandwidth, drive, hiss, word length.

---

# Part B. Essphor Bank (resonator)

## 7. The bank

Stereo: one bank per channel, 48 two-pole band-passes each, in parallel. Use SIMD (four bands per vector). Follow `res_proto.py`.

### Controls

| Control | Range | Default | Notes |
|---|---|---|---|
| CUTOFF | 20 Hz to 20 kHz, exponential | 20 kHz | CV (1 V per octave around the knob). |
| RESONANCE | 0 to 1 | 0 | CV. |
| DECAY | 0 to 1 | 0 | CV. DECAY MODE switch SUSTAIN / CHOKE. |
| PITCH | −24 to +24 semitones, snapped to semitones | 0 | CV, plus V/OCT. CV and V/OCT are not snapped. |
| SLOPE | −1 (low-pass) through 0 (band-pass) to +1 (high-pass) | −1 | CV. |
| TONE | −1 to +1 | 0 | CV. |
| SCALE | the 20 scales of section 3, snap knob with text display | CHROMATIC | |
| WET | 0 to 1 | 1 | CV. WET MODE MIX / SEND, as in Essphor. |
| AGE | 0 to 1 | 0 | CV. |

Jacks: IN L, IN R, OUT L, OUT R, V/OCT, eight CV inputs.

### Band tuning

- Root is C1 = 32.703 Hz. The base grid has six bands an octave over eight octaves: semitone `12×o + 2×i + (o mod 2)` above C1 for octave o = 0..7 and i = 0..5. Even octaves hold the even pitch classes and odd octaves the odd ones, so CHROMATIC holds all twelve notes and still spans 33 Hz to 7.9 kHz.
- Any other scale: each base band moves to the nearest note of the scale (ties go up), and bands that land on the same note merge into one. So PENTATONIC has 41 bands, FIFTHS 17, OCTAVES 9 (`check_res.txt`).
- FREE: the plain grid `2×k` semitones, k = 0..47.
- PITCH, CV and V/OCT move the whole bank. A band above 0.45 × sample rate or below 16 Hz is silent.
- Polarity alternates band by band (+, −, +, ...) in order of frequency after merging. This is what lets the bank sum nearly flat.

### Each band

A state-variable band-pass in the trapezoidal (TPT) form, as in the reference: `G = tan(π f / sr)`, `R = 1/(2Q)`, output `2R × bp` (unity gain at the centre).

- `Q0 = 11`. With DECAY at 0, `Q = Q0` for every band.
- With DECAY above 0: ring time `T60 = 0.05 × 400^DECAY × (f / 261.6)^(−0.3)` seconds, and `Q = max(Q0, π f T60 / 6.91)`. So 0.05 s to 20 s at middle C, shorter above, longer below.
- Make-up per band: `(Q / Q0)^0.25`. Bank gain: +1.7 dB.

### The filter is the band gains

`curve()` in the reference. For a band at frequency f: `q = 0.707 × (1 − RESONANCE) + 12 × RESONANCE²`, `w = f / CUTOFF`, `den = sqrt((1 − w²)² + (w/q)²)`, `lp = 1/den`, `hp = w²/den`, `bp = (w/q)/den`. SLOPE at or below 0: `g = −SLOPE × lp + (1 + SLOPE) × bp`. Above 0: `g = SLOPE × hp + (1 − SLOPE) × bp`. Then TONE: `g × (f / 1000)^(0.5 × TONE)`. Clamp g to 0..16.

- DECAY MODE SUSTAIN: g multiplies each band's *input*. A band that is already ringing keeps ringing when the filter moves away from it.
- DECAY MODE CHOKE: g multiplies each band's *output*. Closing the filter silences the ringing.
- Smooth g and the coefficients over 5 ms. Recompute them every 16 samples.

End caps carry what lies outside the bank: a two-pole low-pass (Q 1) four semitones below the lowest band and a two-pole high-pass (Q 1) four semitones above the highest, each at gain 0.7 times the curve value there, polarity negative for the low cap and opposite to the top band for the high cap, exactly as in the reference.

Output: `softlimit` as in section 2, then WET.

Measured on the reference with the filter open, DECAY 0, CHROMATIC: −4.0 to +0.9 dB from 60 Hz to 6 kHz, −4.4 to +2.5 dB from 20 Hz to 18 kHz. It is a filter bank, not a wire; WET at 0 is the true bypass.

### AGE on Essphor Bank

Fixed errors come from a seed created with the module and saved in the patch. Left and right use different seeds, so AGE also widens the stereo image.

| Item | Law |
|---|---|
| Tuning | Each band off by a fixed normal error of `18 cents × AGE` rms |
| Ring time | Each band's Q times `2^(0.4 × AGE × n)`, n a fixed standard normal, floor 3 |
| Level | Each band's gain off by a fixed `2 dB × AGE` rms |
| Drift | Each band's tuning wanders by `±6 cents × AGE`: four slow random signals (two one-poles of 2 s), band k uses signal k mod 4. Not in the reference; add it. |
| Drive | `tanh(x × d)/d` into the bank, `d = 1 + 3 × AGE` |
| Hiss | White noise at `−90 + 36 × AGE` dB into the bank, so a long DECAY sings faintly on its own |

---

# Part C. Essphor Mod (modulators)

## 8. Four sections on one panel

WAVE, RANDOM, FOLLOW, ADSR, top to bottom, eight small knobs each. Each section has two outputs, A and B. B is the same modulator shifted by SPREAD. Shared jacks: CLOCK and RESET (same conventions and menu as Essphor).

RATE in WAVE and RANDOM has a four-position MODE switch: FREE (0.01 to 100 Hz, exponential), STRAIGHT, TRIPLET, DOTTED. The synced modes snap to 8/1, 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64 of a bar-based division (1/4 = one beat), times 2/3 for TRIPLET and 3/2 for DOTTED. Without CLOCK, synced modes use 120 BPM.

**WAVE.** Output `5 V × (OFFSET + AMOUNT × y)`, clamped to ±10 V.

| Knob | Range | Default | Law |
|---|---|---|---|
| RATE | above | 1 Hz | |
| AMOUNT | −1 to +1 | 1 | |
| PHASE | 0 to 360° | 0 | Added to the phase. RESET returns to this phase. |
| OFFSET | −1 to +1 | 0 | |
| SKEW | −1 to +1 | 0 | Phase warp: `s = 0.5 + 0.49 × SKEW`; `p' = 0.5 p/s` below s, `0.5 + 0.5 (p−s)/(1−s)` above. At the ends a sine becomes a ramp-like envelope. |
| FOLD | 0 to 1 | 0 | `y × (1 + 4 × FOLD)`, then reflected back into ±1 (triangle fold). |
| CURVE | −1 to +1 | 0 | `sign(y) × |y|^(2^(−2 × CURVE))`. Positive fattens the peaks, negative thins them. |
| SPREAD | 0 to 1 | 0 | B's phase is ahead by `SPREAD × 180°`. |

SHAPE switch: SIN, TRI, SQUARE. Order of operations: phase + PHASE, SKEW, shape, CURVE, FOLD, AMOUNT and OFFSET.

**RANDOM.** A looping sequence of random steps. Output as WAVE.

| Knob | Range | Default | Law |
|---|---|---|---|
| RATE | above | 1/8 | One step per cycle. |
| AMOUNT, PHASE, OFFSET | as WAVE | | PHASE shifts within the step. |
| LENGTH | 1 to 32 steps, snapped | 8 | |
| VARIATION | 0 to 1 | 0 | Each time a step comes round, it is re-rolled with this probability. 0 repeats forever, 1 never repeats. |
| SMOOTH | 0 to 1 | 0 | One-pole slew with time constant `SMOOTH × one step`. |
| SPREAD | 0 to 1 | 0 | B reads the sequence `SPREAD × half a step` earlier, and at 1 uses its own independent sequence. Keep it simple: B = crossfade from A to an independent second sequence by SPREAD. |

Step values are uniform in ±1. The sequence and its seed are saved in the patch.

**FOLLOW.** Envelope follower. Jacks IN L, IN R. Output `10 V × AMOUNT × env`, unipolar.

| Knob | Range | Default | Law |
|---|---|---|---|
| DELAY | 0 to 500 ms | 0 | Delays the envelope. |
| AMOUNT | −1 to +1 | 1 | Negative gives ducking. |
| RISE | 0 to 1 s, quadratic | 5 ms | One-pole time constant when rising. |
| FALL | 0 to 4 s, quadratic | 150 ms | One-pole time constant when falling. |
| INPUT | 0 to 4 × | 1 | Gain before rectifying. 5 V peak at gain 1 gives env = 1. Clamp env to 1. |
| FILTER | −1 to +1 | 0 | One knob: below 0 a 12 dB low-pass sweeping 20 kHz down to 80 Hz; above 0 a 12 dB high-pass sweeping 20 Hz up to 5 kHz. |
| HOLD | off, then a rate | off | Sample and hold of the envelope. Fully left is off. With CLOCK: 1/1 to 1/32, snapped. Without: 0.5 to 50 Hz. |
| SPREAD | 0 to 1 | 0 | A follows L + R. B crossfades from that to R alone while A crossfades to L alone. |

**ADSR.** GATE input and a TRIG button (acts as a gate while held). Output 0 to 10 V.

| Knob | Range | Default | Law |
|---|---|---|---|
| ATTACK | 1 ms to 10 s, exponential | 5 ms | |
| DECAY | 1 ms to 10 s | 200 ms | |
| SUSTAIN | 0 to 1 | 0.6 | |
| RELEASE | 1 ms to 20 s | 300 ms | |
| ATT CURVE, DEC CURVE, REL CURVE | −1 to +1 | 0 | 0 is linear. For stage progress p from 0 to 1 use `p^(2^(2c))` for the attack; for decay and release apply the same law to the falling part. Negative is logarithmic (fast start), positive exponential (slow start). |
| SPREAD | 0 to 1 | 0 | B is A delayed by `SPREAD × 250 ms`. |

No AGE on Essphor Mod.

---

## 9. Tests (`make test`, no Rack needed)

Build the three engines as plain C++ classes with no Rack dependency (`src/dsp/`), and a `test/render.cpp` that drives them. Run at 44.1 and 48 kHz. Each line below is a pass/fail check.

Grains, against `check_proto.txt` and `check_feedback.txt`:
1. Pitch: 440 Hz in, DELAY FREE 0.25 s, 16 grains a second of 0.25 s, at −12, −7, 0, +7, +12, +24 semitones: spectral peak within 2% of the target.
2. Delay: noise bursts in, pitch 0, DELAY FREE at 0.125, 0.25, 0.5 and 1 s: lag of best match within 1 ms.
3. Stretch: 200 to 2000 Hz sweep over 4 s, STRETCH 0.5: frequency heard at 2, 3, 4, 5 s within 3% of the sweep at half that time.
4. Level: low-passed noise at −20 dB, wet only, overlaps 1, 4, 16, 64, SPRAY 0 and 0.5: all eight results within 3 dB of each other.
5. Feedback: the fifteen cases of `check_feedback.txt`: level at 11 to 12 s within 6 dB of the reference where the reference is above −60 dB, below −60 dB where it is below; peak never above 0 dB.
6. Guard: +36 semitones, SIZE 1 beat, 20 seconds of a sine: no sample-to-sample jump in the wet signal larger than the sine and window allow (a click detector on the second difference).
7. Lock: lock during a sine, POSITION scan across the join: same click detector.
8. Grid: SYNC 1/16 with a 120 BPM clock for 60 s, RAND RATE 1: the GRAIN pulse count is 480 ± 1, and with RAND RATE 0 every pulse is within one sample of the grid.
9. AGE 0 is exact: output with AGE 0 equals output with the AGE code compiled out.
9b. AGE scale, against `check_age.txt`: with silence in, output level at AGE 0.25, 0.5, 0.75 and 1 within 3 dB of the reference; with noise in, the 8-12 kHz band falls as AGE rises.
10. Voices: 250 grains a second of 2 s: never more than 64 voices, no NaN, peak below the limiter ceiling.

Resonator, against `check_res.txt`:
11. Flatness figures for 20-60 Hz, 60-6000 Hz and 6-18 kHz within 1 dB of the reference (44.1 kHz).
12. Band counts and lowest and highest band for the seven listed scales: exact.
13. Filter curve at 125 Hz to 8 kHz for low-pass, band-pass, high-pass and resonant low-pass: within 1.5 dB.
14. Ring time and tuning at DECAY 0.25, 0.5, 0.75, 1: T60 within 10%, peak within 0.5%.
15. SUSTAIN and CHOKE: ring a pentatonic bank with a noise burst at DECAY 0.75, then sweep CUTOFF to 100 Hz at 0.5 s. From 0.6 to 1.0 s SUSTAIN must be within 3 dB of the unswept level and CHOKE at least 20 dB below it. (Not in the reference, which has fixed controls.)
16. AGE 0 is exact, and AGE 1 with two seeds gives left and right band frequencies that differ.

Modulators:
17. WAVE: frequency within 0.1% at 1 Hz and 1/8 at 120 BPM; output within ±5 V at AMOUNT 1; B leads A by 90° at SPREAD 0.5.
18. RANDOM: LENGTH 8, VARIATION 0 repeats exactly over 100 cycles; VARIATION 1 does not.
19. FOLLOW: 5 V sine burst gives 10 V within 5%; RISE and FALL time constants within 10%.
20. ADSR: stage times within 5% at curve 0; sustain level exact.

## 10. Presets

Essphor (12). Each is a `.vcvm` with every parameter set.

| # | Name | Idea |
|---|---|---|
| 01 | Grain Delay Eighths | DELAY SYNC 1/8 dotted, RATE 1/16, SIZE 3/8 beat, PITCH +12, FEEDBACK 0.35, WET 0.55 |
| 02 | Half Speed Cloud | STRETCH 0.5, RATE 1/32, SIZE 1 beat, SPRAY 0.06, WET 1 |
| 03 | Chord To Arpeggio | DELAY SYNC 1/4, RATE 1/16, SIZE 1/4 beat, CONTOUR −0.9, PATTERN 0.5, PENTATONIC, WET 0.8 |
| 04 | Frozen Scan | POSITION, RATE 1/32, SIZE 1/2 beat, SPRAY 0.01, WET 1 (press LOCK, patch an LFO to SCAN) |
| 05 | Backwards Glass | DELAY FREE 0.6 s, FREE 7 Hz, SIZE 0.45 s, RAND REV 0.6, RAND PITCH RANDOM 0.5, MAJOR, SPRAY 0.1 |
| 06 | Shimmer | DELAY FREE 0.5 s, FREE 12 Hz, PITCH +12, OCTAVES, FEEDBACK 0.7, SPRAY 0.05, WET 0.5 |
| 07 | Warped Tape | DELAY FREE 1.2 s, FREE 10 Hz, SIZE 0.6 s, SPRAY WARP 0.15, RAND PITCH DETUNE 0.08, WET 0.75 |
| 08 | Buzz Tone | STRETCH 0.25, FREE 110 Hz, SIZE 30 ms, CONTOUR −0.5, WET 1 (play RATE from a sequencer) |
| 09 | Scatter | DELAY SYNC 1/4, RATE 1/16, all four RAND knobs high, PITCH −12, SPRAY 0.2 |
| 10 | Stutter Sixteenths | DELAY SYNC 1/16, RATE 1/16, SIZE 1/8 beat, CONTOUR −1, FEEDBACK SOS 0.5, WET 1 |
| 11 | Octave Ghost | DELAY SYNC 1/2, RATE 1/8, PITCH −12, SIZE 1/2 beat, CONTOUR +0.8, WET 0.4 |
| 12 | Worn Sampler | Preset 02 with AGE 0.7 |

Essphor Bank (8): Open Bank (default); Low-pass Classic (CUTOFF 700 Hz, RESONANCE 0.6); Pentatonic Ring (PENTATONIC, PITCH +9, DECAY 0.7, WET 0.6); Fifths Drone (FIFTHS, PITCH +3, DECAY 0.85, WET 0.5); Band-pass Shimmer (SLOPE 0, CUTOFF 900 Hz, RESONANCE 0.3, DECAY 0.55); Choke Sweep (CHOKE, MINOR, DECAY 0.8, patch an LFO to CUTOFF); Bright Glass (HIRAJOSHI, TONE +0.6, DECAY 0.65, PITCH +12); Old Bank (Pentatonic Ring with AGE 0.7).

Essphor Mod (4): Slow Sine And Steps; Skewed Envelope Wave (SKEW +0.9, 1/4); Evolving Eight Steps (LENGTH 8, VARIATION 0.2, SMOOTH 0.3); Duck And Swell (FOLLOW AMOUNT −1, FALL 250 ms; ADSR slow).

## 11. Manual

MANUAL.md and one PDF for the plugin, in the style of the Tapeizer manual: what each module is, every control in plain words, the presets, and at least ten patch recipes. Include these: grains into the bank; the bank's V/OCT played from a sequencer; GRAIN output firing a drum; LOCK from a gate to catch one bar; Essphor Mod's RANDOM into SCAN in POSITION mode; FOLLOW ducking WET; two Essphors on one clock at different RATE; AGE at 0, 0.5 and 1 compared; loading a WAV and scanning it; feedback shimmer with the bank in the loop (Essphor OUT to Bank to a mixer and back to Essphor IN). State once that the sketches behind the design were checked by measurement, not by ear.

Demo WAVs from the C++ engine: the eighteen sketches of `tools/proto/make_sketches.py` and `make_sketches2.py`, same settings, plus one per preset. Sources are in the project files; copy the two source WAVs into `tools/proto/source/`.

## 12. Order of work

1. Plugin skeleton from the template; three empty modules that load.
2. Grain engine as a plain class; tests 1 to 10.
3. Resonator engine; tests 11 to 16.
4. Modulators; tests 17 to 20.
5. Rack wrappers, panels from the script, scale display, context menus, WAV load and buffer save.
6. Presets, demos, manual, PDF, README.
7. Push, check the Apple silicon build is green, confirm the preview release has the plugin.

If something in the reference and this document disagree and the Python is plainly a prototype shortcut (block size, linear interpolation, the fixed controls of the resonator), this document wins. Anything else, the Python wins. Record each such decision in `docs/BUILD-DECISIONS.md`.
