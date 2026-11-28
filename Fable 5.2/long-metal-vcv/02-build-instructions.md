# Long Metal: build instructions

Design by Fable, 2026-11-25, for the model that builds it. Hardy named the module and judged the sound from prototype sketches.

The **reference engine** is `tools/proto/longmetal_ref.c` with `scenes.h` (plain C, no dependencies: `gcc -O2 -o lm longmetal_ref.c -lm`, then `./lm rail out.f32`, `./lm hard out.f32`, `./lm live out.f32`, `./lm live2 out.f32`, `./lm zaps out.f32`, `./lm fuzz <seed> out.f32`, `./lm lossless`; `post.py outdir raw:name:ratio ...` turns raw output into WAVs and a spectrogram). The C is the definition of the sound. Where prose here is ambiguous, the C decides.

The sketches are in `/mnt/project-files/long-metal-vcv/sketches/`. Hardy's verdicts, which set the priorities:

- `01-rail-shimmer` (scene `rail`): "pretty good", then "the shimmer is fine". This is the reference rail sound, with its smooth onset. A version with a percussive break-loose burst at the start (`05`) was **rejected: "I don't like it. Please revert."** The train must never trigger a burst by default.
- `06-hard-hits` (scene `hard`): "excellent". `04-live-line` and `07-live-line-2` (scenes `live`, `live2`): "really good, a really fine model, really interested in hearing more from this".
- First half of `02-zaps-and-echoes` (clean strikes on a plain cable): "boring, sounds like a synthesizer". The pushed second half: "much more alive, more of this".
- Wind in cables: a pass. **There is no wind in this module.**

So the module leans on supports, the pushed controls (WARP, SWELL, LOOSE, GRIP) and the live range. Clean zaps are only the quiet end. Do not tame the top of any range. Hardy's standing view: physical accuracy is not the limit; push non-linearities past real if it sounds more alive.

Things marked **UNTESTED** were not in the prototype. Build them last, test them hard, and say in the README what you changed.

## 1. What to build

- Plugin folder `LongMetalFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-sjnwql` (not main). `BentSheetMetalFable` on branch `claude/project-thread-wsdoxv` is the template for the Makefile, plugin.json, panel script, Haer logo, offline test harness, Mapping.hpp pattern, preset generator, PDF tooling, display widget and README (`git fetch origin claude/project-thread-wsdoxv`, then `git show FETCH_HEAD:BentSheetMetalFable/...`). Do not copy that folder into this branch. The workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `LongMetalFable`, name `Long Metal Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `LongMetal`, name `Long Metal`, tags Synth voice, Physical modeling, Drone, Effect, Noise. Description: "A very long line of metal heard from one point on it. Rails that sing before the train arrives, struck cables that zing and crack, and a line that amplifies itself and screams. The sound is the journey: distance is the instrument."
- Panel 40 HP, text labels, the Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 20 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo MP3s rendered by the C++ engine.
- Rack's namespace has an `Engine` class. Put the DSP in `namespace longmetal` and qualify it.

## 2. The idea in one paragraph

The line is a chain of up to 320 identical cells with one wave travelling right and one travelling left. Each cell delays low frequencies more than high ones (a few first-order allpasses), so a click that has travelled far arrives as a falling zing, and each echo off the ends is longer than the last. Between cells a support bounces a little of the wave back. Regular supports block some frequency bands and pass others, and waves near the band edges travel very slowly; those slow waves are the glassy, inharmonic tones of a singing rail. Sound goes in at one cell (a train of eight axles, a hammer, or audio) and is heard at another, so everything heard has travelled. Every allpass and support is an exact rotation, so the core can only move energy around, never create it, even when its coefficients are driven at audio rate by the signal itself. That makes the pushed controls safe, and makes LIVE possible: per-cell gain above one with a soft ceiling, so the line sings by itself on its slow tones.

Measured on the reference:

- Lossless check (`./lm lossless`: WARP, SWELL, LOOSE all on, STIFF and SUPPORTS moving): total energy constant to 1e-12 over 10 s.
- One far strike on a plain cable (210 cells away, GAUGE HEAVY, `a0` -0.9, `d` 2): 8 kHz arrives after 21 ms, 2 kHz after 78 ms, 1 kHz after 177 ms, 500 Hz after 322 ms.
- Supported test rail (`ties` 0.12, `d` 10, 3 allpasses at -0.7): slow-wave tones near 0.85, 1.8, 3.0, 4.4 kHz. With `ties` 0.13 the rail is hiss with notches; from about 0.5 it is tones. That is the range that matters.
- `rail`: level rises about 40 dB over the 30 s approach; L and R are uncorrelated (0.04) with R about 3 dB louder until the train passes.
- Fuzz (8 seeds x 30 s, everything random over the full ranges of section 5 every 0.5 s, including N, d and GAUGE): zero guard events, peak below 35.
- Cost: `rail` (320 cells) uses about half a cloud core in the scalar double reference, most of it `cos`/`sin`/`sqrt`.

## 3. Engine (port `longmetal_ref.c` to `src/dsp/LineEngine.hpp`, no Rack code)

### 3.1 Cells

`N` cells (16 to 320). State per cell and direction: a delay buffer (64 samples), `M` allpass states (`M` is 3 or 4), a low-loss state, a high-loss state. Per cell: `envc` (10 ms energy envelope), `es` (1 s envelope), `gl`, `bz`, `stuck`. Fixed random per cell: `rndD, rndA, rndT, rndG`, drawn in that order per cell from the xorshift stream of section 4.

The buffer indexing makes the true cell delay `d + 1` samples. Keep it.

### 3.2 Per sample, per cell (both directions, see `line_step`)

```
d_i  = clamp(round(d + 1.5 AGE rndD_i), 1, 63)
x    = delay read
a    = a0 (1 + 0.08 AGE rndA_i)
e    = xr^2 + xl^2;   envc += 0.002 (e - envc)
a_r  = a + swell envc/(1+envc) + warp xr/(1+|xr|)      (a_l likewise with xl), clamp to [-0.985, 0.5]
b    = sqrt(1 - a^2)
M times:  y = a x + b s;  s = b x - a s;  x = y         (normalised allpass: a rotation of (x, s))
low loss:  ll += kl (x - ll);  x -= lowcut ll            kl from 500 Hz
high loss: lh += kh (x - lh);  x += hidamp (lh - x)      kh from 6 kHz
g = g0 (1 - 0.004 AGE |rndG_i|), times the grip gain of 3.4
x *= g;  if g0 > 1: x = 0.6 tanh(x / 0.6)
```

The reference scenes use `hif` 5, 6 and 7 kHz; the module fixes 6 kHz. Scene tests set it per scene.

### 3.3 Supports and ends

Between cell `i` and `i+1`: `th = ties (1 + 0.5 AGE rndT_i) + loose A/(1+A)`, `A = |rOut_i| + |lOut_{i+1}|`, then the rotation

```
rIn[i+1] =  cos(th) rOut[i] + sin(th) lOut[i+1]
lIn[i]   = -sin(th) rOut[i] + cos(th) lOut[i+1]
```

Ends: `rIn[0] = rhoL lOut[0]`, `lIn[N-1] = rhoR rOut[N-1]`. Injection adds half to each direction at the write. Listener: linear interpolation between cells `floor(pick)` and the next; **L = rOut, R = lOut**.

### 3.4 GRIP (off when `grip` is 0)

Each cell is held by friction until the wave reaching it passes a threshold. `thr = grip (1 + 0.6 AGE rndT_i)`, floored at `0.1 grip`. A stuck cell has gain 0.9. When `envc > thr` it releases, sets `es = thr`, and fires a noise burst `bz = snap sqrt(grip)`, decaying by 0.996 per sample, injected into its own cell; that trips the neighbours, so the release runs down the line as an avalanche. It re-grips only when the 1 s envelope `es` falls below `0.005 thr` (a fast re-grip test collapses the line). `gl` slews the gain at 0.004 per sample. `snap` is 12. Turning GRIP from 0 to above 0 calls `grip_reset`.

GRIP is for struck sounds. It is 0 in every rail preset.

### 3.5 Train

Eight axles in bogie pairs (offsets from `train_init`), trailing on the side away from the listener. Per axle: grainy roughness (Poisson impulses at `roughDens` per second with a heavy-tailed size, over a low Gaussian floor), low-passed by `roughBright` and high-passed; a patchy slow level; wheel flats (axles 0, 3, 6 full, 5 half) knocking once per wheel turn at `wheelHz`; a clack when the axle crosses one of three joints at `0.27 N`, `0.47 N`, `0.67 N` (rounded). Positions off the line are silent, so a train can roll onto the line.

### 3.6 Strike and audio in

Strike: raised-cosine pulse of `ms` milliseconds, `inject(src, amp * 0.5 (1 - cos) / len * 8)`. Audio in: `inject(src, in)` every engine sample. **UNTESTED.**

### 3.7 Live seed

Whenever `g0 > 1`, add `inject(0.5 N, 2e-5 * nrand())` every sample. Without it a live line that has gone silent stays silent.

### 3.8 Safety

1. Guard: any delay write not below 1e6 in magnitude is zeroed and counted. No test may trigger it.
2. The 0.6 ceiling of the live line, the clamps on `a`, and the bounded forms `x/(1+|x|)` are the stability argument. Port them exactly.
3. Flush denormals. When nothing is injected, `g0 <= 1` and total energy is below 1e-14, skip the cell loop and output zero.

### 3.9 Clocking and PITCH (**UNTESTED**)

The engine is defined at a nominal 48 kHz and knows nothing about the host rate. PITCH re-clocks it: engine rate `fe = 48000 * 2^pitch`, `pitch` clamped to -2.5 .. +1. Each host sample advances a phase by `fe / hostRate` and runs that many engine steps. This scales the whole structure exactly: support tones, echo times and zing lengths all move together, so V/OCT tracks.

- Engine to host: keep the last four engine outputs and read them with 4-point Hermite interpolation at the fractional phase. When `fe > hostRate`, first low-pass the engine output (2-pole Butterworth at `0.45 hostRate`, computed at `fe`).
- Host to engine: audio in and all CVs are linearly interpolated across the engine steps of one host sample.
- Things that must stay in real time are computed with `fe` in place of `SR`: train speed in cells per second, `wheelHz`, `roughDens`, the strike length in ms. Everything else is in samples and scales with size.

### 3.10 Output chain (host rate)

20 Hz one-pole high-pass. WIDTH: `mid = (L+R)/2`, `side = (L-R)/2 * WIDTH`. Leveller, because near and far differ by 40 dB: RMS of both channels over 250 ms, `gain = (max(rms, 0.003 ref) / ref)^(-0.67 EVEN)` with `ref = 0.05`. Then LEVEL, scaled so the Init preset peaks near 5 V, then `11 tanh(v / 11)`.

- **ENV out:** 0 to 10 V from the square root of the mean `envc` over the 16 cells around the listener, smoothed 20 ms. Calibrate so a live line at its ceiling reads about 8 V.
- **CLACK out:** 1 ms 10 V trigger on each joint clack, each wheel flat of the lead axle, and the first release of a GRIP avalanche (no more than one trigger per 30 ms).
- **PASS out:** 10 V gate while a PASS is running.

### 3.11 Cost

Cells are independent within a sample, so use structure-of-arrays float and let the compiler vectorise across cells (the allpass chain is serial only in `m`). Precompute `cos`/`sin` per junction when `loose` is 0 and `ties` and AGE are unchanged; with `loose`, use `s = sin` by a short polynomial and `c = sqrt(1 - s^2)` so the rotation stays exact. Compute `b = sqrt(1 - a^2)` once per cell when WARP and SWELL are 0. Check the float engine against test 2 (lossless within 1e-3 over 10 s). Target below 20% of one core at 48 kHz host, PITCH 0, 320 cells; report it at PITCH 0 and +1. Do not cut cells to reach it without saying so.

## 4. Seeds

One xorshift64 stream seeded `88172645463325252`, `urand` and `nrand` as in the reference, drawn in the reference order (`line_init`, then `train_init`). The sequence is fixed. Test 1 checks it.

## 5. Controls

Every control marked (CV) has a jack summed at 0.1 per volt (10 V sweeps a 0 to 1 knob, +-5 V sweeps a bipolar one) unless stated.

| Section | Control | Range and default | Maps to |
|---|---|---|---|
| LINE | PITCH + V/OCT | -2 to +1 octaves, 0 | `pitch` of 3.9 |
| | LENGTH | 16 to 320 cells in steps of 8, 320 | `N` |
| | SPACING | 1 to 40, stepped, 9 | `d`. Coarse tuning of the support tones and echo time. Steps click on purpose. |
| | GAUGE (switch) | LIGHT / HEAVY, LIGHT | `M` = 3 / 4 |
| | STIFF (CV) | 0 to 1, 0.51 | `a0 = -0.95 (1 - (1-x)^2)`. 0 is rope, 1 is rail. |
| | SUPPORTS (CV) | 0 to 1, 0.71 | `ties = 1.3 x^2`. Below 0.3 hiss with notches, above 0.6 glassy tones, near 1 a chain of ringing cells. |
| | END A, END B | -1 to 1, 0.31 both | `rhoL, rhoR = 0.98 x`. 0 absorbs, +-1 reflects, sign flips the wave. |
| | LIVE (CV, large knob) | 0 to 1, 0.225 | below 0.5: `g = 1 - 0.02 ((0.5-x)/0.5)^2.5` (damped). Above: `g = 1 + 0.03 ((x-0.5)/0.5)^1.5` (the line amplifies itself). Mark the centre. CV must work at audio rate: gating LIVE with a square wave is a headline patch. |
| | GROUND (CV) | 0 to 1, 0.5 | `lowcut = 0.07 x` |
| | BRIGHT | 0 to 1, 0.42 | `hidamp = 0.012 (1-x)^2` |
| PLACE | LISTEN (CV) | 0 to 1, 0.12 | `pick = 2 + x (N - 5)` |
| | DISTANCE (CV at 0.2/V, large knob, bipolar) | -1 to 1, 0.7 | `src = pick + x (N - 1)`. Where the train, hammer and audio enter. Positive is to the listener's right. Audio-rate CV allowed. |
| | PASS (button + trigger), PASS TIME | 4 to 120 s, 30 | The train runs from `src` through the listener to `pick - x (N-1) - side*60` in PASS TIME, then its level fades over 1 s, it returns to `src`, and fades back in. `side = sign(x)` is frozen during the pass. **UNTESTED as a control**; scene `rail` is one pass. |
| SOURCES | TRAIN (CV) | 0 to 1, 0.5 | `roughLevel = (2x)^2`. At rest the train rolls on the spot: a steady shimmer. |
| | SPEED (CV) | 0 to 1, 0.49 | `wheelHz = 2 * 8^x`, `roughDens = 120 (0.3 + 1.43 x)`, `roughBright = 0.15 + 0.4 x` |
| | FLATS | 0 to 1, 0.5 | `flatLevel = jointLevel = roughLevel * 2x` |
| | STRIKE (button + trigger), VEL (jack) | | `amp = 100 vel^2.5`, `vel = VEL volts / 10`, 0.6 unpatched, at `src` |
| | HARD | 0 to 1, 0.8 | `ms = 0.06 + 0.6 (1-x)^2` |
| | IN (jack), IN LEVEL | 0 to 1, 0.5 | `in = 4 x^2 volts / 5`. Calibrate so 5 V of pink noise at 0.5 is about as loud as TRAIN 0.5 at the same distance. |
| PUSHED | WARP (CV) | 0 to 1, 0 | `warp = 0.6 x` |
| | SWELL (CV) | 0 to 1, 0 | `swell = x` |
| | LOOSE (CV) | 0 to 1, 0 | `loose = 0.8 x` |
| | GRIP (CV) | 0 to 1, 0 | 0 is off; above, `grip = 2e-5 * 1000^x` |
| CHARACTER | AGE | 0 to 1, 0.35 | section 6 |
| OUT | EVEN | 0 to 1, 0.75 | leveller strength |
| | WIDTH | 0 to 1, 1 | |
| | LEVEL | 0 to 1, 0.7 | |

The defaults are the `rail` scene with the train at rest 222 cells away, except `hif` (6 kHz, not 7).

Train position: outside a PASS the lead axle sits at `src` and `side` follows `sign(DISTANCE)` with a dead band of +-0.05 so it does not flip on noise. Joint clacks fire whenever an axle crosses a joint, whether moved by PASS or by DISTANCE CV.

**Jacks.** Inputs: V/OCT, STIFF, SUPPORTS, LIVE, GROUND, LISTEN, DISTANCE, PASS, TRAIN, SPEED, STRIKE, VEL, WARP, SWELL, LOOSE, GRIP, IN. Outputs: L, R, ENV, CLACK, PASS. Monophonic: use channel 1 of any poly cable.

Smooth knobs over 5 ms at most; LIVE, DISTANCE, STIFF and SUPPORTS CVs go through unsmoothed.

## 6. AGE

One knob, 0 to 1. At 0 the line is perfect and the support tones are clean.

- In the reference: irregular cell length (`1.5 AGE rndD`), uneven stiffness (8%), uneven support strength (50%), patchy loss, uneven GRIP thresholds.
- **UNTESTED**, each small:
  - Thermal drift: `a0 *= 1 + 0.02 AGE r(t)`, `r` a random walk band-limited to 0.2 Hz.
  - Dead sleepers: cells with `rndT > 1.2` have their support angle scaled by `1 - AGE`; cells with `rndT < -1.2` by `1 + AGE`.
  - Worn wheels: wheel flat size scatter widens from `0.6 + 0.8 u` to `0.6 + 0.8 u + AGE u^4 * 2`.

## 7. Tests (`make test`, no Rack needed; shape of `BentSheetMetalFable/test/`; every check prints PASS or FAIL)

1. **Reference match.** With clocking at exactly one engine step per sample and the output chain off, the C++ engine reproduces scenes `rail`, `hard`, `live`, `live2`, `zaps`. Compare third-octave band levels over each 2 s window, within 2 dB where the reference is within 40 dB of its peak (3 dB for the chaotic parts of `live` and `live2` after 14 s). The first 5 s of `zaps` must match to 0.5 dB.
2. **Lossless.** Scene `lossless`: total energy constant within 1e-3 (float) from 0.1 s to 10 s.
3. **Zing.** One strike 210 cells from the listener, plain cable as in section 2: 8 kHz arrives within 5 ms of 21 ms, 1 kHz within 20 ms of 177 ms.
4. **Support tones.** Strike on the test rail of section 2: the four strongest tones 1 to 3 s after the strike are within 3% of 0.85, 1.8, 3.0, 4.4 kHz.
5. **Approach.** Scene `rail`: level at 26 to 28 s is 25 to 45 dB above level at 2 to 4 s; L/R correlation below 0.2 before the pass.
6. **Live.** Init with LIVE 0.74 and TRAIN 0: output starts from the seed alone within 5 s, stays below the ceiling, and has at least three spectral peaks. LIVE back to 0.3: silent within 10 s.
7. **Live gate.** LIVE switched between 0.45 and 1.0 at 3 Hz for 10 s: bursts follow the gate, no guard event.
8. **GRIP.** Last section of scene `hard`: the soft hit at 25.2 s produces less than -60 dB of the level of the hit at 26.4 s; that hit releases at least 200 cells. With GRIP 0 the soft hit is audible.
9. **No burst on the train.** Init (GRIP 0) from silence: the first second of train sound has a crest factor below 8 and no jump above 12 dB between 50 ms windows.
10. **PITCH.** Support tones of test 4 at PITCH -1, 0, +1 and V/OCT +0.5: within 5 cents of the scaled values.
11. **PASS.** Trigger at defaults with PASS TIME 10: level peaks within 1 s of the lead axle passing the listener, the balance moves from R to L, the PASS gate is high for the pass, and the train is back at `src` afterwards with no click above the train's own level.
12. **Fuzz.** 400 random patches over every control at its full range including the Rack-level mappings, new values every 0.5 s, strikes, PASS triggers and noise bursts into IN, 20 s each: no NaN, no guard event, output within +-11 V.
13. **Sample rates.** Tests 4, 10 and 12 (100 patches) at host rates 44.1, 48 and 96 kHz.
14. **Silence.** TRAIN 0, LIVE below 0.5, nothing patched: output exactly zero after 60 s.
15. **Audio-rate CV.** DISTANCE and LIVE CVs driven by 200 Hz sines of +-2 V: no guard event, no NaN.
16. **Speed.** Report the engine's share of one core at 48 kHz host, 320 cells, at PITCH 0 and +1, with the pushed controls off and on.

## 8. Panel (40 HP = 203.2 mm; adjust positions so labels do not collide; render a preview PNG and look at it)

Captions over each section. LIVE and DISTANCE are large knobs. A horizontal display strip, about 120 mm x 12 mm, draws the line: cell energy (`envc`, log scale) as brightness along its length, the listener as a marker, the axles as ticks, the strike/input point, stuck cells dimmed when GRIP is on, and the two ends drawn open or closed by END A and END B. Draw it with nanovg as on Bent Sheet Metal. If it costs more than half a day, ship five lights for energy along the line and say so.

| Row | Contents, left to right |
|---|---|
| top | display strip |
| 1 knobs | LINE: PITCH, LENGTH, SPACING, GAUGE switch, STIFF, SUPPORTS, END A, END B, GROUND, BRIGHT. CHARACTER: AGE |
| 2 knobs | LIVE (large). PLACE: DISTANCE (large), LISTEN, PASS TIME, PASS button. PUSHED: WARP, SWELL, LOOSE, GRIP |
| 3 knobs | SOURCES: TRAIN, SPEED, FLATS, STRIKE button, HARD, IN LEVEL. OUT: EVEN, WIDTH, LEVEL |
| 4 jacks | V/OCT, STIFF, SUPPORTS, LIVE, GROUND, LISTEN, DISTANCE, PASS, TRAIN, SPEED |
| 5 jacks | STRIKE, VEL, WARP, SWELL, LOOSE, GRIP, IN, then outputs ENV, CLACK, PASS, L, R on a dark plate |

## 9. Presets (20 .vcvm files, numbered; each sounds with nothing patched unless noted)

Lean toward the live line, hard hits and the rail. Where a preset needs motion, say in its notes what to patch, and also make it sound good held still.

1. Init (the rail singing with a train at rest far away; press PASS and it comes)
2. Train Coming (as Init, PASS TIME 30; notes: trigger PASS)
3. Close Rail (DISTANCE 0.12: knocks, flats and roar, right on top of you)
4. Live Line (scene `live` at 12 s: N 200, SPACING 7, STIFF from `a0` -0.75, `ties` 0.16, ENDS -0.92/+0.92, LIVE 0.76, WARP 0.3, TRAIN 0; notes: slow LFO to STIFF)
5. Live Glide (scene `live2` at 8 s: SPACING 3, `ties` 0.25; notes: LFO to STIFF and to SUPPORTS)
6. Live Scream (LIVE 1, WARP 0.5, LOOSE 0.2, SPACING 14)
7. Live Gated (LIVE 0.45; notes: square LFO at 2 to 4 Hz into LIVE with +5 V swing, as the end of `live2`)
8. Knocked Alive (LIVE 0.72, STRIKE into a singing line to throw it into new states)
9. Hard Hits A (scene `hard` section A: supported, WARP 0.42, SWELL 0.3; needs STRIKE)
10. Hard Cable (section B: SPACING 3, SUPPORTS 0, HEAVY; needs STRIKE)
11. Rattling Sleepers (section C: `ties` 0.3, LOOSE 0.63, WARP 0.67; needs STRIKE)
12. Break Loose (section D: SPACING 18, GRIP 0.67, LOOSE 0.25, WARP 0.5; needs hard STRIKEs; soft ones do nothing)
13. Laser (the clean far zap: HEAVY, STIFF 0.77, SPACING 2, SUPPORTS 0, ENDS -0.97; needs STRIKE. The quiet end, for contrast)
14. Never Repeats (as 13 with SPACING 40: discrete echoes each longer than the last)
15. Glass Rail (SUPPORTS 0.9, AGE 0, TRAIN 0.4: pure tones)
16. Rust Rail (AGE 1, SUPPORTS 0.8, GROUND 0.8)
17. Giant (PITCH -2: everything slow and huge)
18. Wire (PITCH +1, LENGTH 64, SPACING 2, ENDS -0.97, STIFF 0.4: a short pitched cable for V/OCT; needs STRIKE)
19. Feedback Rail (TRAIN 0.3 into a line just below live: LIVE 0.49, SUPPORTS 0.75; the train excites a line that almost sings)
20. Resonator (TRAIN 0, IN LEVEL 0.7, LIVE 0.4, SUPPORTS 0.6; needs audio in; notes: sweep DISTANCE while audio runs)

Check every preset through the full module chain, and that 4 to 8 are clearly not a polite tone.

## 10. Manual

MANUAL.md and a PDF made with the Bent Sheet Metal tooling. Plain language, full sentences. Contents: what it is (a long line you listen to from one point; the sound is the journey), the three things that shape it (stiffness makes zings, supports make tones, distance makes the difference between shimmer and impact), the live line, a panel tour, each control in terms of what you hear, where the ranges go past real, AGE, the presets, and at least ten patch recipes, including: PASS triggered by a slow clock; envelope or slow ramp into DISTANCE for your own approach; square LFO gating LIVE; slow LFOs to STIFF and SUPPORTS under a live line; sequencer to V/OCT on Wire; STRIKE with rising VEL into GRIP; CLACK out clocking another module; ENV out into WARP so loud passages warp themselves; drums into IN while DISTANCE is swept; audio-rate oscillator into DISTANCE; two modules an octave apart panned by their own L/R.

## 11. Order of work

1. Port the engine and the tests. Get tests 1 to 9 and 14 green.
2. Vectorise, then speed (16), fuzz at engine level.
3. Clocking and PITCH (10, 13), PASS (11), audio in, fuzz with the Rack-level mappings (12, 15).
4. Module, panel, display strip, presets.
5. AGE extras, one at a time, each behind the fuzz test.
6. Push, confirm the Mac arm64 CI build is green and the plugin is on the preview release.
7. Manual, PDF, README, and demo MP3s from the C++ engine into `/mnt/project-files/long-metal-vcv/demos/`: the scenes `rail`, `hard`, `live`, `live2`, plus a 10 s clip of each preset (drive the ones that need a trigger or LFO from the test harness and say so in the file name).
8. Record every deviation from this spec in the README.

Hardy's standing preferences: Apple silicon build only; text labels; short names searchable in Rack; PDF manual with patch recipes; do not write to the creations tracker sheet unless Hardy says so; when presenting demos, attach only a few and say which to start with (Hardy does not listen to them all): start with Live Line, Hard Hits and Train Coming.
