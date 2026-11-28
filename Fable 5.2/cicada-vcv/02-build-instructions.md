# Cicada: build instructions

Design by Fable, 2026-11-22, for an Opus build. One VCV Rack 2 module, **Cicada**: a physical-style model of singing insects (cicadas, crickets, katydids, grasshoppers) and of insect flight, playable as nature or stretched into percussion and a formant voice. No samples.

The reference for every number is the Python engine `tools/proto/cicada_ref.py`, with the factory presets in `tools/proto/presets.py` and the demo patches in `tools/proto/render_ref.py`. The reference is written as a per-sample loop on purpose: port it line for line. Where this document and the reference disagree, the reference wins, except where this document says "C++ differs". Read the reference first. `sketch_engine.py` and `make_sketches.py` are the earlier offline sketch (one decaying sine per impulse); they are background only.

Background, research and measurements: `/mnt/project-files/cicada-vcv/01-research-and-proposal.md` (copy it to `docs/RESEARCH.md`).

## 0. What makes it sound real (do not simplify these away)

1. **Impulses ring a resonator; there is no oscillator.** Every sound is a train of mechanical impulses (rib buckles, file teeth, wingbeats) fed with sub-sample timing into a complex one-pole resonator. A cricket's pure tone exists only because its teeth arrive exactly one carrier cycle apart and add up in phase. Scatter the teeth and the same resonator gives katydid noise. Do not replace this with a sine oscillator and an envelope.
2. **The carrier slides down through every cricket pulse** (8% of lock, centred on the pulse). Measured on every real cricket clip at 4 to 7%.
3. **Cicada ribs lower the tymbal's pitch as they buckle, and the out-click is higher and quieter.** The two tymbals alternate, are detuned by 1.2% either way and sit slightly apart in the stereo field.
4. **The body filter moves.** OPEN retunes a band-pass after the resonators, and several songs move it on their own. This is the cicada's "wah".
5. **LOOSE runs from machine to animal** with the measured amounts: at the default 0.4 pulse timing wobbles by 3.2%, gaps by 12%, pulse level by 8%. At 0 nothing is random.
6. **Sub-sample event timing everywhere.** Pulses, ribs and teeth are scheduled with fractional sample positions and injected as `amp x coef^late`. Without it the high carriers alias and the locked teeth stop adding up.

## 1. Files and conventions

Folder `CicadaFable/` in hardyfinn/2026SoloModules, same shape as `Heathrow/` and `ClapfallFable/` (branch `claude/project-thread-p0eipc` has Clapfall for reference). Copy Heathrow's Makefile, .gitignore, LICENSE.txt, `res/haer_logo.py`, `tools/pdf.js`, `tools/preview.py` and adapt. CI already builds any folder with a plugin.json for Mac arm64, which is the only build needed.

```
CicadaFable/
  plugin.json            slug CicadaFable, name "Cicada Fable", brand Haer, version 2.0.0, GPL-3.0-or-later
  Makefile               as Heathrow, plus `make test`
  src/plugin.cpp/.hpp
  src/Cicada.cpp         Module + ModuleWidget only
  src/dsp/Voice.hpp      one insect: song, scheduler, mechanism, resonators, body (no Rack includes)
  src/dsp/Distance.hpp   shared air and reflections stage (no Rack includes)
  res/Cicada.svg, res/make_panel.py
  presets/Cicada/*.vcvm  the 20 factory presets
  test/render.cpp        offline checks and demo WAVs, no Rack needed
  docs/MANUAL.md, docs/Cicada-Manual.pdf, docs/BUILD-INSTRUCTIONS.md, docs/RESEARCH.md
  tools/proto/           the Python reference (already here)
```

Module slug `Cicada`, name "Cicada", tags Synth voice, Physical modeling, Percussion, Polyphonic, Noise. Description along the lines of: "Singing insects by physical model: cicadas, crickets, katydids, grasshoppers and wingbeats, from a single field cricket to clocked percussion and a formant voice". The DSP headers must compile without Rack so `test/render.cpp` can use them (vcvrack.com is blocked in the cloud container, so the Rack build is only checked by CI).

## 2. Panel (20 hp, 101.6 mm)

Text labels, light panel in the Heathrow style, Hær logo on the bottom rail, control labels 3.2 mm, jack labels 2.9 mm. Generate with `res/make_panel.py` and check with the preview tool before committing. Layout is guidance; keep the grouping and make sure nothing overlaps. Widen to 22 hp if labels do not fit.

- **Top:** title CICADA. To its right a small dark display (about 40 x 9 mm, NanoVG) showing the SONG name in capitals, and a pulse light that blinks 20 ms on every pulse (brightness by pulse level).
- **Row 1, heading MECHANISM:** MECH (three-position switch, top to bottom TYMBAL / FILE / WING), PITCH (large), PURE, STROKE.
- **Row 2, heading BODY AND AIR:** BODY, OPEN, TEMP, LOOSE, DISTANCE.
- **Row 3, heading SONG:** RATE (large), CHIRP, GAP, SONG (snapped, 8 positions), LENGTH, and a two-position switch **V/OCT** (PITCH / RATE).
- **Jack row A:** PITCH, PURE, STROKE, OPEN, RATE, CHIRP, TEMP (CV inputs), V/OCT.
- **Jack row B:** GATE, TRIG, CLOCK, then on a dark plate the outputs PULSE, CHIRP, ENV, L, R.

Tooltips on every control must say what it does in a plain sentence and show real units (Hz, pulses per second, pulses per chirp, seconds, degrees C, rib count or stroke percent by mechanism). Use custom ParamQuantity display strings: CHIRP at the top reads "Trill", LENGTH at the top reads "Hold".

## 3. Controls

Knob value k in 0..1. CV inputs add to the knob at 0.2 per volt (5 V = full sweep) and the sum is clamped to 0..1. All mappings are the `map_*` functions at the top of the reference.

| Control | Default | Mapping |
|---|---|---|
| MECH | FILE | TYMBAL, FILE or WING. Section 5. |
| PITCH | 0.752 | Carrier f = 100 x 160^k Hz (100 Hz to 16 kHz). In WING it is the body resonance. Multiplied by 2^V of V/OCT when the V/OCT switch is at PITCH. Clamp to 0.45 x sample rate. |
| PURE | 0.885 | Resonator Q = 2 x 15^k (2 to 30). Tooth lock = clamp((k - 0.45) / 0.30, 0, 1). Tymbal rib pitch drop = max(0, 0.12 x (1 - k/0.75)). |
| STROKE | 0.59 | What one muscle twitch does. TYMBAL: ribs = 1 + round(11k). FILE: pulse length = (0.05 + 0.9k) of the pulse period. WING: level of the upstroke impulse, 0 to 1. |
| BODY | 0 | Depth of the body band-pass: out = (1 - 0.9k) x dry + 0.9k x band-pass, then x (1 + 0.9k) make-up. |
| OPEN | 0.5 | Body tuning = carrier x 2^(2 x (k - 0.5)), half to double. Centre is tuned to the carrier and loudest. Songs add their own offset to this. |
| TEMP | 0.5 | Temperature 10 to 35 C. Speed factor tf = (T - 5) / 17.5, so 1.0 at the centre. Multiplies RATE and the PULSATE tremolo, divides GAP, and multiplies the carrier by tf^0.38. |
| LOOSE | 0.4 | Section 4, "LOOSE". |
| DISTANCE | 0.15 | Section 6. |
| RATE | 0.468 | Pulses per second = 1000^k (1 to 1000). With the V/OCT switch at RATE it becomes a transposer: rate = 261.6256 x 2^(V + (k - 0.5) x 8). Clamp to 2000. |
| CHIRP | 0.441 | Pulses per chirp = round(32^(k/0.95)) for k below 0.96 (1 to 32). At 0.96 and above: continuous trill. |
| GAP | 0.409 | Silence between chirps = 0.02 x 200^k seconds (20 ms to 4 s), divided by tf. |
| SONG | STEADY | The phrase gesture, section 4. Snapped: STEADY, SWELL, PULSATE, RISE, SLUR, TICK-BUZZ, SINK, TALK. |
| LENGTH | 1.0 (Hold) | Phrase length = 0.5 x 60^(k/0.95) seconds (0.5 to 30 s). At 0.96 and above: Hold, the phrase never ends unless a gate ends it (gestures then use 8 s as their time base). |
| V/OCT switch | PITCH | PITCH: V/OCT transposes the carrier. RATE: V/OCT plays the pulse rate (formant voice, 0 V = C4) and the carrier comes from the PITCH knob and CV only. |

The defaults are the Field Cricket preset.

**Jacks.** Schmitt triggers on GATE, TRIG and CLOCK (low 0.1 V, high 1 V).

- **GATE**: when patched, the song follows the gate. Rising edge starts a phrase from the top, the phrase holds at its end point while the gate is high whatever LENGTH says, the falling edge plays the release. Silent between gates. Unpatched: phrases cycle by themselves (body, release, rest).
- **TRIG**: when patched, the module is a triggered voice. Each trigger plays one chirp (CHIRP pulses, one pulse when CHIRP is at Trill) at full level, the song gesture and GAP are ignored, and nothing sounds between triggers.
- **CLOCK**: when patched, each clock edge starts a chirp in place of the GAP timer. The song gesture still applies. Ignored when CHIRP is at Trill.
- **Outputs.** L and R: section 6. **PULSE**: 10 V trigger of 1 ms at every pulse onset. **CHIRP**: 10 V gate while a chirp is sounding (in trill, while the phrase is active). **ENV**: follower of the voice's own level (rectify, 2 ms attack, 60 ms release, scaled so a steady full-level tone reads about 8 V, clamped 0 to 10 V). The reference returns pulse and chirp; ENV is C++ only.

**Polyphony.** Channels = the largest channel count among V/OCT, GATE, TRIG and CLOCK, at least 1, at most 16. Each channel is a separate individual (`individual()` in the reference): voice 0 is exact; others get fixed offsets from a per-channel seed of pitch within +-3.5% (as 2^(+-0.035)), rate +-4%, gap +-15%, a start delay of 0.2 to 4 s (free-running only; ignored when GATE or TRIG is patched), and a pan position spread evenly over -0.8 to +0.8 in the order `(v x 7) mod voices`. CV inputs are poly-aware (channel c, or channel 0 if mono). PULSE, CHIRP and ENV are polyphonic. L and R are the mix of all voices, multiplied by 1/sqrt(voices).

Context menu **Individuals: 1, 2, 3, 4, 6, 8** (default 1): used when no poly cable sets the channel count, so a chorus needs no cables. Saved in the patch.

## 4. Song and scheduler (`Voice.hpp`)

Port the control-rate block (every 16 samples) and the chirp scheduler of `voice()` exactly. Summary of what it does:

**Phrase state machine.** Three states: BODY (time tS runs 0 to LENGTH, x = tS / LENGTH clamped to 1), RELEASE (y = tS / release length), REST (silent; length = REST_FRAC[song] x LENGTH x (1 + 0.3 x LOOSE x gaussian), floor 0.2). REST_FRAC = 0.35, 0.50, 0.50, 0.90, 0.28, 0.15, 0.35, 0.40 in song order. With LENGTH at Hold the body never ends on its own.

Each gesture produces, every control tick, a level multiplier, pitch multiplier, rate multiplier, OPEN offset, gap multiplier and chirp-length multiplier:

| Song | Body | Release | Model |
|---|---|---|---|
| STEADY | nothing | 30 ms fade | crickets, katydids |
| SWELL | level 0.03 to 1 over the first 45% | 0.3 x LENGTH: level to 0, pitch to 0.93, rate to 0.75 | large cicadas |
| PULSATE | as SWELL, plus a 6.3 Hz x tf tremolo whose depth grows to 0.45 between x = 0.1 and 0.55; OPEN offset 0.12 x sin in step with it | as SWELL | dog-day cicada |
| RISE | level 0.25 + 0.75 x^1.5 | 30 ms cut | grasshopper |
| SLUR | level 0.1 to 1 in the first 0.4 s | clamp(0.2 x LENGTH, 0.15, 1.2) s: pitch to 0.74, level to 0.15 | 17-year "pharaoh" cicada |
| TICK-BUZZ | first 45%: single pulses at RATE x (0.06 rising to 0.15), level 0.5 to 1. Then a continuous buzz at RATE, pitch 0.96 rising to 1.04, level 0.5 to 1 | 0.25 x LENGTH: pitch 1.04 to 0.88, level to 0.1 | cassini-type cicada, meadow katydid |
| SINK | chirps speed up (gap x 1/(1 + 1.6x)), shorten (pulses x (1 - 0.35x)), sink in pitch (1 - 0.10 x^1.5), level 0.5, 1, 0.8, 0.35 at x = 0, 0.15, 0.6, 1. Inside each chirp: level 0.6, 1, 0.35 at 0, 15%, 100%; pitch falls 5%; OPEN offset falls to -0.28 over 1.6 chirp lengths | 0.15 x LENGTH fade | evening cicada "kana-kana" |
| TALK | the tymbal runs continuously and the body makes syllables. Syllable length S = chirp period (CHIRP / RATE + GAP); the first is 3.3 S. In each syllable the OPEN offset goes from -0.43 up to 0 in 0.25 S and back down over the last 0.35 S, level 0.25 to 1 and back with it. The body ends at the first syllable boundary after LENGTH | one last syllable of 5 S: opens, pitch falls 4%, level fades over its last 30% | min-min cicada |

**Chirp scheduler.** A pulse timer counts down in samples and fires with its fractional lateness. Pulse period = sample rate / (rate x rate multiplier), jittered by 8% x LOOSE per pulse. After the last pulse of a chirp the gap (x gap multiplier, jittered 30% x LOOSE) is added. The first two pulses of any chirp of three or more are at 0.45 and 0.75 level (measured on field crickets). TRIG and CLOCK behave as in section 3.

**LOOSE (k).** Pulse period sigma 0.08k. Gap sigma 0.30k. Pulse level sigma 0.20k. Chance that a chirp of three or more loses its last pulse: 0.4 x k^1.5. In a STEADY trill, not WING: pauses of 0.15 to 0.4 s arriving at 0.15k per second. Carrier drift: a random target redrawn every 0.4 to 1.2 s, slewed at 3 per second, depth 0.6% x k (3% x k in WING). TYMBAL rib timing sigma 0.12k and rib level sigma 0.24k. Unlocked file teeth have their own scatter from PURE and ignore LOOSE.

Random numbers: one small PRNG per voice (xorshift32 in the reference; the gaussian is four uniforms summed). They need to match in statistics, not bit for bit.

## 5. Mechanism, resonators and body (`Voice.hpp`)

**Resonators.** Two per voice, A and B, each `z = z x coef + impulses`, output = imag(z), with `coef = exp(-pi x bw / sr) x exp(i x 2 pi x f / sr)` and bw = carrier / Q. An impulse of level a that happened `late` samples before the current sample is added as `a x coef^late`. Recompute coef whenever the frequency changes (at every impulse in TYMBAL and FILE, and every control tick). Use `std::complex<float>` or two floats; `coef^late` is `exp(late x log(coef))`, or compute magnitude and angle directly.

**TYMBAL.** Pulses alternate sides: side 0 rings A, side 1 rings B. A pulse of n ribs: rib k arrives one carrier period after the last (jittered by LOOSE), at level `amp x (1 - 0.5k/n)`, and sets that side's frequency to `carrier x (1 - drop x k/(n-1))` times 0.988 for A or 1.012 for B. The out-click follows at `max(0.48 x pulse period, (n + 0.5) carrier periods)` after the pulse onset, at 0.35 level, and sets the frequency to carrier x 1.08 (x the side's detune). Pan: A at the voice position minus 0.15, B plus 0.15, equal power.

**FILE.** Uses A only, at the voice position. For the length of the pulse, teeth arrive at `tooth rate = f x (0.22 + 0.78 x lock)`, where `f = carrier x (1 - 0.08 x lock x (xx - 0.5))` and xx is the position in the pulse 0..1. Each tooth sets A to f and adds level `amp x max(0.02, sin(pi x xx)^0.7) x (1 + jit x gaussian)`. Tooth interval jitter is the same `jit = 0.45 x (1 - lock)`, floor 0.2 of the interval.

**WING.** Each pulse is a wingbeat: an impulse into A (at PITCH) on the beat and an impulse of STROKE level into B (at 1.9 x PITCH) 0.45 of a period later. RATE is the wingbeat pitch.

**Loudness normalisation** (`norm` in the reference) keeps every setting near the same level; port the three formulas exactly.

**Body.** One TPT state-variable band-pass per resonator output, Q 3.5, unity gain at its centre, tuned by OPEN plus the song's offset, mixed by BODY as in section 3. Bypass when BODY is 0.

Smoothing: knob-derived values are read every 16 samples as in the reference. That is fine for everything except the body filter coefficient `g`, which the C++ should interpolate across the 16 samples to avoid zipper noise when OPEN moves fast (C++ differs).

## 6. Distance and output (`Distance.hpp`)

Shared by all voices, after they are summed to stereo.

1. One-pole low-pass on L and R at `18000 x (1 - d)^2 + 2500` Hz.
2. Twenty-four fixed reflections read from a 125 ms stereo delay line of the low-passed signal. Each has gain `sign x d x 0.5 x exp(-delay / 50 ms)`. Their sum goes through a one-pole low-pass at 5 kHz and is added. Bypass both steps at d = 0.
3. Output = `10 V x tanh(5 x mix / 10)`. A cricket peaks near 5 V.

Reflection table (delay ms, sign, destination channel, source channel; 0 = L, 1 = R):

```
76.51 + 1 1 | 30.12 + 0 0 | 105.33 + 0 0 | 96.46 + 0 0 | 39.15 - 0 1 | 55.63 - 0 1
68.21 + 1 1 | 76.17 + 0 1 |  28.98 + 0 1 |  9.10 + 0 0 | 63.73 + 0 1 | 76.99 + 0 1
61.64 - 0 1 | 26.32 - 1 1 |  27.27 + 0 0 | 100.29 - 1 0 | 35.04 + 1 0 | 102.27 + 1 1
90.05 - 0 0 | 62.90 + 1 1 |  45.91 + 1 0 | 48.97 - 1 0 | 21.42 - 1 0 | 117.53 - 0 1
```

If only L is patched it carries (L + R) x 0.707.

## 7. Module details

- `onReset` clears voices and the delay line. `onSampleRateChange` recomputes coefficients and delay lengths.
- Patch JSON: Individuals.
- Flush denormals in the resonators (they decay to zero between chirps) and the body filter.
- CPU target: under 1% of a core for one voice at 48 kHz, under 6% for 16.
- The SONG display shows the gesture name; while a phrase is resting, dim it.

## 8. Factory presets (`presets/Cicada/`, also the manual's starting points)

Knob values, straight from `tools/proto/presets.py` (which holds them in real units). Order of columns: MECH, PITCH, PURE, STROKE, BODY, OPEN, RATE, CHIRP, GAP, SONG, LENGTH, TEMP, LOOSE, DISTANCE.

| Preset | MECH | PITCH | PURE | STROKE | BODY | OPEN | RATE | CHIRP | GAP | SONG | LENGTH | TEMP | LOOSE | DIST |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Field Cricket | FILE | 0.752 | 0.885 | 0.589 | 0.000 | 0.500 | 0.468 | 0.441 | 0.409 | STEADY | 1.000 | 0.500 | 0.500 | 0.150 |
| Tree Cricket | FILE | 0.678 | 0.885 | 0.500 | 0.000 | 0.500 | 0.603 | 1.000 | 0.435 | STEADY | 1.000 | 0.500 | 0.250 | 0.150 |
| Ground Cricket | FILE | 0.621 | 0.850 | 0.556 | 0.000 | 0.500 | 0.461 | 0.835 | 0.642 | STEADY | 1.000 | 0.500 | 0.450 | 0.150 |
| Katydid | FILE | 0.722 | 0.338 | 0.411 | 0.500 | 0.720 | 0.244 | 0.301 | 0.648 | STEADY | 1.000 | 0.500 | 0.600 | 0.150 |
| Meadow Katydid | FILE | 0.935 | 0.338 | 0.556 | 0.400 | 0.720 | 0.597 | 1.000 | 0.435 | TICK-BUZZ | 0.577 | 0.500 | 0.400 | 0.080 |
| Grasshopper | FILE | 0.813 | 0.227 | 0.656 | 0.400 | 0.800 | 0.383 | 1.000 | 0.435 | RISE | 0.333 | 0.500 | 0.500 | 0.150 |
| Dog-Day Cicada | TYMBAL | 0.823 | 0.512 | 0.182 | 0.650 | 0.440 | 0.781 | 1.000 | 0.435 | PULSATE | 0.717 | 0.500 | 0.350 | 0.150 |
| Pharaoh Cicada | TYMBAL | 0.511 | 0.719 | 0.545 | 0.000 | 0.500 | 0.707 | 1.000 | 0.435 | SLUR | 0.383 | 0.500 | 0.150 | 0.150 |
| Tick-Buzz Cicada | TYMBAL | 0.793 | 0.406 | 0.182 | 0.000 | 0.500 | 0.760 | 1.000 | 0.435 | TICK-BUZZ | 0.494 | 0.500 | 0.400 | 0.150 |
| Evening Cicada | TYMBAL | 0.732 | 0.594 | 0.273 | 0.750 | 0.530 | 0.735 | 0.703 | 0.353 | SINK | 0.515 | 0.500 | 0.300 | 0.300 |
| Min-Min Cicada | TYMBAL | 0.741 | 0.338 | 0.182 | 0.900 | 0.640 | 0.748 | 0.932 | 0.477 | TALK | 0.556 | 0.500 | 0.450 | 0.200 |
| Rasp Cicada | TYMBAL | 0.767 | 0.299 | 0.091 | 0.000 | 0.500 | 0.774 | 0.760 | 0.367 | STEADY | 1.000 | 0.500 | 0.900 | 0.150 |
| Greengrocer | TYMBAL | 0.741 | 0.662 | 0.273 | 0.500 | 0.500 | 0.793 | 1.000 | 0.435 | SWELL | 0.773 | 0.500 | 0.250 | 0.150 |
| Mosquito | WING | 0.461 | 0.150 | 0.500 | 0.000 | 0.500 | 0.905 | 1.000 | 0.435 | STEADY | 1.000 | 0.500 | 0.500 | 0.050 |
| Bumblebee | WING | 0.325 | 0.018 | 0.500 | 0.000 | 0.500 | 0.745 | 1.000 | 0.435 | STEADY | 1.000 | 0.500 | 0.900 | 0.050 |
| Cricket Hats | FILE | 0.843 | 0.719 | 0.500 | 0.000 | 0.500 | 0.551 | 0.190 | 0.435 | STEADY | 1.000 | 0.500 | 0.100 | 0.100 |
| Tymbal Bass | TYMBAL | 0.383 | 0.463 | 0.182 | 0.000 | 0.500 | 0.500 | 1.000 | 0.435 | STEADY | 1.000 | 0.500 | 0.020 | 0.000 |
| Leaf Shaker | FILE | 0.887 | 0.256 | 0.722 | 0.000 | 0.500 | 0.492 | 0.380 | 0.304 | STEADY | 1.000 | 0.500 | 0.300 | 0.200 |
| Giant | TYMBAL | 0.283 | 0.555 | 0.364 | 0.700 | 0.550 | 0.527 | 0.602 | 0.540 | SINK | 0.577 | 0.500 | 0.400 | 0.250 |
| Sine Night | FILE | 0.490 | 1.000 | 0.500 | 0.000 | 0.500 | 0.360 | 0.301 | 0.565 | STEADY | 1.000 | 0.500 | 0.200 | 0.350 |

Tymbal Bass is saved with the V/OCT switch at RATE (so RATE is 0.5, no transposition). Cricket Hats expects TRIG, Leaf Shaker expects CLOCK; say so in the manual.

## 9. Tests (`make test`, no Rack)

`test/render.cpp` must check, and fail loudly on:

1. **Field Cricket** at LOOSE 0: carrier measured by zero crossings inside pulses is 4541 Hz within 1.5%; pulse period 39.4 ms within 1%; five pulses per chirp; the frequency in the last fifth of a pulse is 150 to 400 Hz below the first fifth; envelope peak between 35% and 65% of the pulse.
2. **Lock makes the tone.** FILE at PURE 0.885 has at least 80% of its power within +-8% of the carrier; at PURE 0.3 less than 50%.
3. **Tymbal sides.** Dog-Day Cicada with BODY 0 and DISTANCE 0: L and R differ (correlation below 0.97), pulse count per second within 3% of 220 at LOOSE 0.
4. **LOOSE statistics.** At LOOSE 0 the pulse intervals of a trill have a coefficient of variation below 0.001 and two renders are identical. At LOOSE 1 it is 0.08 within 20%.
5. **TEMP.** Tree Cricket pulse rate at TEMP 0.2, 0.5, 0.9 is 64.6 x tf within 2%, and the carrier follows tf^0.38 within 2%.
6. **Songs.** For each of the eight songs with LENGTH 4 s, free-running for 30 s: the phrase, release and rest lengths match the reference within 5% at LOOSE 0, and the output is silent in the rest. SLUR's carrier ends at 0.74 of its start within 3%.
7. **Jacks.** TRIG: one chirp of exactly CHIRP pulses per trigger and silence otherwise. CLOCK: chirp onsets within one sample of the clock edges. GATE: silence before the first gate, a release after each falling edge. V/OCT at RATE: pulse rate is 261.63 x 2^V within 0.1% at V = -2, 0, 1.
8. **Agreement with the reference.** For Field Cricket, Katydid, Dog-Day Cicada, Pharaoh Cicada and Mosquito, third-octave band levels of a 10 s render at LOOSE 0.4 match `tools/proto` renders within 2 dB in every band within 30 dB of the peak, and the overall level matches within 1.5 dB. (Have the Python write the band levels to a small header or JSON that the test reads.)
9. **Safety.** No NaN, no denormal slowdown and no output above 10 V after 60 s of random knob, CV and trigger fuzzing at 44.1, 48 and 96 kHz, with 16 voices.
10. **Demo WAVs** for Hardy: every preset (12 to 20 s each), a gallery, and the eight patched demos of `render_ref.py` (hats by TRIG, bass by V/OCT to RATE, shaker by CLOCK, gated phrases, TEMP sweep, OPEN by CV, two choruses), into `/mnt/project-files/cicada-vcv/demos/`.

## 10. Manual

`docs/MANUAL.md` and a PDF made with `tools/pdf.js` as in the other plugins: what it is in two paragraphs, the three-layer idea (song, mechanism, body) with one small diagram, a labelled panel picture, each control in plain words, the eight songs with one line each, the presets with what animal each is after, and patch recipes. Written fresh. Hardy does not write C++ and is at home in Rack.

Recipes to include, aimed at house, techno, acid, hip-hop and reggae as well as at nature:

- A summer night: Individuals 6 on Field Cricket, a Tree Cricket beside it, slow LFO into TEMP.
- Cricket hi-hats: TRIG from a sequencer, CHIRP by CV for one, two or three pulses a step, PURE down for a raspier hat.
- Clocked leaf shaker with SWELL for eight-bar builds.
- Tymbal Bass: V/OCT switch at RATE, an envelope into PITCH as the "cutoff", PURE as resonance, a VCA on ENV or an outside envelope, slides from the sequencer's glide.
- Talking drone: Greengrocer, BODY up, an LFO or a sequencer into OPEN.
- The insect as a clock: PULSE and CHIRP outs driving a drum module or a sample-and-hold; ENV ducking a pad.
- A fly-by: Mosquito into a VCA and panner, both moved by one slow envelope, with a little V/OCT bend for Doppler.
- Riser for a drop: Dog-Day Cicada gated, LENGTH long, TEMP swept up.
- Dub siren cousin: Pharaoh Cicada triggered by GATE into a delay.
- Giant insects: any preset with PITCH down two or three octaves and RATE down with it.

## 11. Honest limits to state in the manual and hand-back

The three cricket presets are fitted to measurements of real recordings (ESC-50 research clips). The cicadas, katydids and grasshopper follow published descriptions and measurements (Bennet-Clark and Young on cicada tymbals; standard cricket and katydid bioacoustics), not recordings, so they are plausible rather than matched. Presets are named by type, not as species identifications. Nothing has been heard in Rack until Hardy tries it.
