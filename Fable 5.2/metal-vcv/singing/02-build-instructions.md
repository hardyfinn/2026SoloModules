# Plate Driver: build instructions

Design by Fable, 2026-11-24, for the model that builds it. Hardy named the module and approved the sound from prototype sketches.

The **reference engine** is `tools/proto/plate_driver_ref.c` (one C file, no dependencies, `gcc -O3 -ffast-math plate_driver_ref.c -lm`, then `./a.out swell`, `./a.out h-max` and so on; `post.py` turns the raw output into WAVs). It is the definition of the sound. Where prose here is ambiguous, the C decides. `first_sketches_ref.c` is the earlier 128-mode version that made the first seven sketches; keep it for reference only.

The sketches Hardy approved are in `/mnt/project-files/metal-vcv/singing/sketches/` (clean) and `sketches-harsh/` (with the saturation stages). Hardy's words: "this is great", "make sure that we model in some non-linearities and some saturation", "harsh is not necessarily bad", "happy to hear where the model breaks up to". So keep the extremes reachable. Do not tame the top of any range.

Things marked **UNTESTED** were not in the prototype. Build them last, test them hard, and say in the README what you changed.

## 1. What to build

- Plugin folder `PlateDriverFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-due7lx` (not main; there is no main). Use `SawstackFable` as the template for the Makefile, plugin.json, panel script, offline test harness, PDF tooling and README. The workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `PlateDriverFable`, name `Plate Driver Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `PlateDriver`, name `Plate Driver`, tags Synth voice, Drone, Physical modeling, Distortion. Description: "A thin metal plate that is never struck: a pickup and a driver hold it in a feedback loop, so it sings one pure tone, splits into multiphonics and breaks into a roaring wash as DRIVE rises. Saturating driver, pickup and amplifier, a rattling driver head, a pumping limiter, strike and audio inputs, and an AGE knob".
- Panel 40 HP, text labels, the Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 20 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo WAVs rendered by the C++ engine.

## 2. The idea in one paragraph

An EBow held on a tam-tam. A pickup reads the plate's velocity at one point. That signal goes through a focus filter, a short delay, a limiter and three saturating stages, and comes back as a force at (by default) the same point. Velocity fed back as force at the same point is negative damping, so the plate's own modes grow. The plate is thin, so large vibration stretches it. The stretching couples every mode to every other. Below a threshold amplitude one mode wins and you hear a pure tone. Above it the tone feeds pairs of other modes whose frequencies add up to its own (multiphonics). Higher still, the whole plate is in motion (roar), the stretched plate is stiffer, and every pitch slides up. Coming down, the roar persists well below the drive that started it.

Measured on the reference (128 modes, CURVE 0.6, FOCUS 0.5, drive swept up over 30 s and down over 15 s): one mode holds more than 98% of the energy up to drive 1,200; 2 to 5 modes share it from 2,200 to 14,000; about 17 modes at 51,000; on the way down it is still about 16 modes at 6,700 and back to 2 at 1,900. Zero guard events in every render made.

## 3. Engine (port `plate_driver_ref.c` to `src/dsp/PlateEngine.hpp`, no Rack code)

### 3.1 Rates

Engine rate `fe` = 2 x host rate when the host is below 88.2 kHz, otherwise the host rate. Decimate with a half-band FIR (the reference renders at 96 kHz and `post.py` uses `resample_poly`). Controls are read and coefficients recomputed every 32 engine samples (`BLK`), as in the reference. Smooth every knob and CV with a 5 ms one-pole before use, except where a different time is given.

### 3.2 Modes

`N = 192` modes. Each is an exact two-pole resonator driven by a force (reference `coefs()` and the update line):

```
al_j  = (6.908 / T60_j) * (1 + damp * (8 + f_j / 40))
a1_j  = 2 exp(-al_j T) cos(w_j T)      a2_j = exp(-2 al_j T)      b_j = exp(-al_j T) T sin(w_j T) / w_j
q_j[n+1] = a1_j q_j[n] - a2_j q_j[n-1] + b_j * (d_j * F - bloom * fn_j)
T60_j = decay * 9 / (1 + f_j / 700)   seconds
```

`f_j = f0_j * tune`. Position weights are a one-dimensional stand-in for mode shapes: `shape_j(x) = cos(kw_j x + ph_j)` with `kw_j = 3.1 sqrt(f0_j / f0_0)`, `ph_j` random. Driver weight `d_j = shape_j(xD)`, loop pickup `p_j = shape_j(xP)`, output pickups `oL_j = shape_j(0.13)`, `oR_j = shape_j(0.88)`.

### 3.3 Stretching (the plate non-linearity). Do not simplify this.

`C = 6` stretch channels. Per channel c, with `A_cj = +-1/sqrt(C N)`, `Lm_cj = 0.8 (0.3 + rnd) / sqrt(C)`, `Sg_cj = +-1/sqrt(C)`:

```
s   = sum_j A_cj  w_j q_j
s2  = sum_j Lm_cj w_j q_j^2
gq  = sum_j Sg_cj sqrt(w_j) q_j
S_c = curve * s + 0.5 * s2 + 0.5 * eta^2 * gq^2            eta = 1.0
fn_j = sum_c Seff_c * ( curve * A_cj w_j + Lm_cj w_j q_j + eta^2 gq_c Sg_cj sqrt(w_j) )
```

`fn` is the gradient of the potential `0.5 sum_c S_c^2`, which is never negative, so the stretching can move energy between modes but cannot create it. The `gq^2` term carries the cross-mode products. Without it there are no multiphonics (tried: with only the diagonal `s2` term the tone stayed pure until it was far too loud).

Two additions on `S_c` before it is used in `fn` (reference, same line):

```
Sbar_c += (S_c - Sbar_c) * 0.001                   (slow mean, about 15 Hz at 96 kHz; scale the coefficient with fe)
Seff_c  = S_c - (1 - stretch) * Sbar_c + nu * dS_c/dt       nu = 4e-5
```

`stretch` scales the pitch rise. Measured on one pure tone pushed to amplitude 0.48: +20 cents at stretch 0, +124 cents at 1, +211 cents at 2. At low stretch the tone also finds an octave partner; that is expected. The `nu` term is a loss on the rate of stretching. It is always dissipative and keeps the roar moving.

### 3.4 The loop, in signal order (reference, the block from `// pickup` to the driver)

1. **Pickup.** `y = fe * sum_j p_j (q_j[n] - q_j[n-1])`, `up = sum_j p_j q_j`. Magnetic bend: `g = 1 + 0.85 * PICKUP * tanh(0.6 up)`, `y /= g^2`. The same law is applied to each output pickup with its own local displacement.
2. **Audio in. UNTESTED.** `y += inLevel * 600 * (IN volts / 5)`.
3. **Focus.** Chamberlin state-variable band-pass at `fc`, `Q = 1 + 8 * FOCUS`; `yf = (1 - FOCUS) * y + FOCUS * 2 * bp`.
4. **Phase. UNTESTED** (the reference has the delay line but every sketch used 0). Delay `yf` by 0 to 4 ms with linear interpolation. If it only ever kills the sound, replace it with a first-order all-pass and say so.
5. **Normalise.** `x = gain * yd / max(drive, 1)`.
6. **Pump** (trip-and-recover limiter):
   ```
   k = 1 - exp(-T / (0.6 * 0.02^RATE))            time constant 0.6 s down to 12 ms
   env += (|x| - env) k;  env2 += (env - env2) k;  pk = max(pk (1 - T/8), env2)
   if (!trip && env2 > 0.85 pk) trip = 1;  else if (trip && env2 < 0.85 pk (1 - 0.9 PUMP)) trip = 0
   target = (trip && PUMP > 0) ? 1 - 1.6 PUMP : 1;   pg += (target - pg)(1 - exp(-T/0.003));   x *= pg
   ```
   At full PUMP the loop gain swings to -0.6, so the loop brakes the plate and then lets it grow again. Measured: clear bursts whose rate rises with RATE, from one every 3 s to about 8 per second.
7. **Amplifier.** `g = 1 + 29 AMP^2`, `b = 0.45 AMP`, `x = tanh(g x + b) - tanh(b)`. At AMP 0 this is the plain `tanh` limiter of the first sketches.
8. **Driver.** `h = clip(x (1 + 12 DRV^2), -1, 1)`; `x = (1 - 0.5 DRV) h + 0.5 DRV h^5 + 0.7 DRV (|h| - 0.6)`. A squarer push with an even-order part. (A wavefolder was tried here and rejected: its fundamental goes negative when driven, so it quietened the plate instead of roughening it.)
9. **Force.** `F = drive * x + 1e-4 * noise`. The noise seeds the loop. Keep it.
10. **Rattle** (only when RATTLE > 0): the driver head is a small mass pressed on the plate by a preload, with a one-sided contact spring.
    ```
    P = drive (2 (1 - RATTLE) + 0.03);  kc = 1.1e6;  md = 0.003;  d = dx - up;  Fc = d > 0 ? kc d : 0
    dv += T (F + P - Fc - 12 dv) / md;  dx += T dv;  F = Fc - P
    ```
    With a heavier head (md 0.05) the drive point became a node and the loop died. Do not raise md.
11. **Strike.** A trigger adds velocity to every mode: `q_j[n-1] -= v d_j T exp(-f0_j / mallet)`, `v = 420 * velocity`, `mallet` 300 (soft) to 3000 (hard), default 900.

### 3.5 Outputs

```
vL = fe * sum_j oL_j ow_j (q_j[n+1] - q_j[n])      ow_j = 1 / (1 + f_j / (1500 (1 + 4 BRIGHT)))
L  = vL / (1 + 0.85 PICKUP tanh(0.6 uL))^2 + BLEED * 0.06 * F
g  = 1 + 14 OUT^2;   L = 2500 tanh(g L / 2500) / sqrt(g)
```

Same for R. Then, after decimation, a leveller that matches what `post.py` did for the sketches Hardy approved: RMS of both channels over 150 ms, gain = (max(rms, 0.01 ref) / ref)^-0.5 with ref = 2500 (calibrate so the result matches the sketches' balance between pure tone and roar within 2 dB). Then LEVEL, scaled so a pure tone at DRIVE 0.4 is 5 V peak, then a final `11 tanh(v / 11)`.

- **ROAR out:** participation ratio `PR = (sum E_j)^2 / sum E_j^2` with `E_j` the mode energies averaged over 50 ms. Output `10 V * clamp(log2(PR) / log2(24), 0, 1)`, smoothed 100 ms.
- **ENV out:** `10 V * clamp(sqrt(sum E_j / 3e6), 0, 1)`, smoothed 20 ms.

### 3.6 Safety

Keep the reference guard: if total energy exceeds 1e9, halve every state and count the event. No test in section 7 may trigger it. Flush denormals. When DRIVE is 0, no strike or audio is arriving and energy is below 1e-9, skip the mode loop.

### 3.7 Cost

The reference (double precision, scalar, 192 modes, 96 kHz) rendered 52 s in 4.7 s on one cloud core, about 9%. Use float and let the compiler vectorise the two mode loops. Target below 12% of one core at 48 kHz host rate on Apple silicon. Report the figure.

## 4. Plates (TYPE) and CAST

| TYPE | Mode frequencies | Status |
|---|---|---|
| SHEET | `f0_j = f1 (1 + 0.6 (j + 0.45 jit_j))`, `jit_0 = 0`: even density, as the reference | Prototyped. This is the approved sound and the default. |
| TAM-TAM | The TAMTAM ratio list from `Gongoozler/src/GongEngine.hpp` (copy the generator; same author, say so in a comment), first 192 ratios | UNTESTED |
| BOSSED | Gongoozler's BOSSED list: near-harmonic partials 1, 2, 3 ... as beating pairs, then plate modes | UNTESTED |
| BOWL | Pairs at ratios 1, 2.76, 5.10, 7.90, 11.1, 14.6, 18.4, each pair split by 0.3 to 1.2 Hz at 200 Hz, then SHEET-density modes above ratio 20 | UNTESTED |

For the three untested types: run the tests of section 7 on each, render the swell and h-max scenes, and put the WAVs in the demos folder so Hardy can judge. If a type will not sing cleanly at low drive or trips the guard, fix its coupling scale first (`bloom`), then drop it and say so. Do not ship a type that fails.

CAST (0 to 15) seeds the jitter, phases and coupling signs. CAST 0 must reproduce the reference sequence: xorshift64 seeded `0x2545F4914F6CDD1D`, drawn in the order used in `render()`. Check this in a test.

## 5. Controls

Every knob with a jack beside it has an attenuverter-free CV input summed at 0.1 per volt unless stated.

| Section | Control | Range and default | Maps to |
|---|---|---|---|
| PLATE | TYPE | 4 positions, SHEET | section 4 |
| | CAST | 0 to 15, 0 | seed |
| | SIZE (CV) | 20 to 200 Hz, 65.4 | `f1` at C4, see PITCH |
| | THICK (CV) | 0 to 1, 0.5 | `bloom = 4^(1 - 2 THICK)` (thin 4, thick 0.25) |
| | CURVE (CV) | 0 to 1, 0.6 | `curve` |
| | STRETCH | 0 to 2, 1 | `stretch` |
| | DECAY | 0.15 to 4, 1 | `decay` |
| LOOP | DRIVE (CV, large knob) | 0 to 1, 0.35 | `drive = 0` at 0, else `150 * 600^DRIVE`, times the scaling in PITCH |
| | GAIN | 20 to 600, exponential, 60 | `gain`: low starts slowly and organically, high starts at once |
| | FOCUS (CV) | 0 to 1, 0.5 | `FOCUS` |
| | PITCH + V/OCT | -3 to +3 octaves about C4 | `fs = 261.63 * 2^(PITCH + V/OCT)` |
| | TRACK switch | PLATE / FOCUS, PLATE | see below |
| | PHASE (CV) | 0 to 1, 0 | delay 0 to 4 ms |
| TOUCH | DRIVER (CV) | 0.05 to 0.95, 0.31 | `xD` |
| | SPREAD (CV) | 0 to 0.5, 0 | `xP = xD + SPREAD`, wrapped into 0.05 to 0.95. At 0 the loop is collocated and always sings. Away from 0 some modes get positive feedback and some negative, so the set of tones changes. UNTESTED away from 0. |
| | GRAB (CV, 10 V = full) | 0 to 1, 0 | `damp = 8 GRAB` |
| | RELEASE | 0 to 1, 0 | while the gate is low, `damp += 8 RELEASE` |
| HEAT | PICKUP, AMP, DRIVER, RATTLE (CV), BLEED, OUT | each 0 to 1, defaults 0.2, 0.2, 0.1, 0, 0.1, 0 | section 3.4 and 3.5 |
| PUMP | PUMP (CV), RATE (CV) | 0 to 1, defaults 0 and 0.3 | section 3.4 step 6 |
| EXTRA | MALLET | 0 to 1, 0.5 | 300 * 10^MALLET |
| | IN LEVEL | 0 to 1, 0.5 | audio in |
| | BRIGHT | 0 to 1, 0.5 | `BRIGHT` |
| | AGE | 0 to 1, 0.3 | section 6 |
| | LEVEL | 0 to 1, 0.7 | output |

**TRACK = PLATE** (melodic): the whole plate follows the pitch. When the mode table is built, find the anchor mode `a`: the mode whose `f0_a / f0_0` is nearest to `261.63 / SIZE`. In plain terms, at C4 the plate's lowest mode sits at about SIZE, and the anchor is the mode that then sits nearest C4. Then `tune = fs / f0_a` and `fc = fs`. Small SIZE values mean a big plate singing one of its high modes, with many close neighbours, so it splits sooner. **TRACK = FOCUS** (overblowing): `tune = SIZE / f0_0`, fixed, and only `fc = fs` moves, so the loop hops between the plate's own tones.

**Drive scaling with pitch. UNTESTED.** The force needed to reach the splitting amplitude grows with the frequency and the loss of the sung mode. Multiply `drive` by `(fs / 261.63) * (1 + fs/700) / (1 + 261.63/700)`. Test 6 checks it. Adjust the law until the test passes.

**Jacks.** Inputs: V/OCT, GATE, DRIVE, FOCUS, PHASE, SIZE, THICK, CURVE, DRIVER, SPREAD, GRAB, RATTLE, PUMP, RATE, STRIKE, VEL, IN. Outputs: L, R, ROAR, ENV. GATE unpatched means always on. GATE scales `drive` through a 4 ms slew. VEL unpatched means 0.8. Monophonic: use channel 1 of any poly cable.

## 6. AGE (UNTESTED as a whole; each part is small)

One knob, 0 to 1, with a per-instance seed stored in the patch. At 0 everything below is off.

- Loop gain drifts: `gain *= 1 + 0.25 AGE * r(t)`, `r` a random walk band-limited to 0.3 Hz.
- Hiss in the loop: the seed noise rises from 1e-4 to 1e-4 * (1 + 400 AGE^2) (so different tones win on different notes).
- Hum: 50 Hz plus its 2nd and 3rd harmonic added to the pickup signal, from silent to about -38 dB under a pure tone at DRIVE 0.4.
- Dents: each mode detuned by a fixed random amount up to +-1.5% * AGE.
- Loose head: RATTLE gets +0.3 AGE^2.
- Warm-up: `heat` follows total energy with a 20 s time constant, normalised so a held roar gives 1. `tune *= 1 - 0.012 AGE heat` and `decay *= 1 - 0.3 AGE heat`.
- Tired amplifier: the AMP bias `b` gets +0.2 AGE.

## 7. Tests (`make test`, no Rack needed; shape of `SawstackFable/test/render.cpp`; every check prints PASS or FAIL)

1. **Reference match.** SHEET, CAST 0, AGE 0, leveller off: the C++ engine at 96 kHz reproduces the reference scenes `swell`, `h-stages`, `h-pump` and `h-pulse`. Chaotic passages cannot match sample for sample, so compare third-octave band levels over each 2 s window, within 2 dB where the reference is within 40 dB of its peak. The first 1.5 s of `swell` (pure tone, linear growth) must match to 0.1 dB.
2. **Three regimes.** Drive swept up as in the reference `sw2` with focus 0.5, curve 0.6: PR below 1.2 at drive 1,000; between 1.8 and 8 at 8,000; above 10 at 50,000.
3. **Hysteresis.** Same sweep back down: PR above 8 at drive 7,000 on the way down, where it was below 4 on the way up.
4. **Energy.** With drive 0 after a strike of velocity 1, total energy never rises (check every 10 ms) for every TYPE, at THICK 0 and CURVE 1.
5. **Tuning.** TRACK PLATE, DRIVE 0.2, FOCUS 0.8: the sung pitch is within 10 cents of the commanded pitch from C2 to C6.
6. **Drive scaling.** TRACK PLATE, FOCUS 0.5: the DRIVE knob value at which PR first exceeds 1.5 lies within +-0.15 of its value at C4, for C2, C3, C5 and C6.
7. **STRETCH.** One tone at amplitude 0.45 to 0.5: pitch rise 10 to 40 cents at STRETCH 0, 100 to 150 at 1, 180 to 250 at 2.
8. **Pump.** PUMP 1, drive 16,000: the output level swings by more than 12 dB, and the number of bursts in 4 s at RATE 0.9 is at least four times that at RATE 0.1.
9. **Rattle keeps singing.** RATTLE 0.95 at drive 2,600: energy stays above 20% of its RATTLE 0 value, and the output between 3 and 10 kHz rises by at least 10 dB.
10. **Fuzz.** 400 random patches over every control at its full range, with random CV steps every 0.5 s, strikes and noise bursts into IN, 20 s each, all four types: no NaN, no guard event, output within +-11 V.
11. **Sample rates.** Tests 2, 5 and 10 (100 patches) at host rates 44.1, 48 and 96 kHz.
12. **Silence.** DRIVE 0, no input: output exactly zero after 60 s. AGE above 0 may leave hum only.
13. **Speed.** Report the engine's share of one core at 48 kHz host rate in the roar.

## 8. Panel (40 HP = 203.2 mm; adjust positions so labels do not collide; render a preview PNG and look at it)

Ten columns. Captions over each section. DRIVE is a large knob with a ring light that shows ROAR (dim blue pure, white multiphonic, red roar). Outputs on a darker plate.

| Row | Contents, left to right |
|---|---|
| 1 knobs | PLATE: TYPE, CAST, SIZE, THICK, CURVE, STRETCH, DECAY. Then BRIGHT, AGE, LEVEL |
| 2 knobs | LOOP: DRIVE (large, two columns), GAIN, FOCUS, PITCH, PHASE, TRACK switch. TOUCH: DRIVER, SPREAD, GRAB |
| 3 knobs | HEAT: PICKUP, AMP, DRIVER, RATTLE, BLEED, OUT. PUMP: PUMP, RATE. EXTRA: MALLET, IN LEVEL, RELEASE (move RELEASE here from TOUCH if it fits better) |
| 4 jacks | V/OCT, GATE, DRIVE, FOCUS, PHASE, SIZE, THICK, CURVE, DRIVER, SPREAD |
| 5 jacks | GRAB, RATTLE, PUMP, RATE, STRIKE, VEL, IN, then outputs ROAR, ENV, L, R on the dark plate (11 jacks: narrow the spacing on this row or move ROAR and ENV up beside the DRIVE light) |

The HEAT knob for the driver is labelled DRIVER SAT so it is not confused with the DRIVER position knob (label that one DRIVER POS).

## 9. Presets (20 .vcvm files, numbered; each sounds with nothing patched unless noted)

1. Init (defaults: a pure tone at C4 that splits when DRIVE is raised)
2. Pure Singer (FOCUS 0.9, DRIVE 0.25, HEAT all 0)
3. First Split (DRIVE at the edge of multiphonics)
4. Full Roar (DRIVE 0.95, FOCUS 0.3)
5. Infinite Gong (GATE and STRIKE from the same trigger source; notes say so)
6. Overblown Horn (TRACK FOCUS; play V/OCT)
7. Breathing Drone (SIZE 27 Hz; notes: slow LFO to DRIVE)
8. Sub Plate (SIZE 20 Hz, PITCH -2)
9. Glass Lead (THICK 0.8, FOCUS 0.85, BRIGHT 0.8)
10. Scream Lead (PICKUP 1, AMP 0.75, BLEED 0.35, OUT 0.45)
11. Square Pusher (DRIVER SAT 1, BLEED 0.6)
12. Loose Head (RATTLE 0.9)
13. Motorboat (PUMP 1, RATE 0.8)
14. Slow Surge (PUMP 0.7, RATE 0.1)
15. Metal Stabs (GAIN 500, RELEASE 0.8; gate it)
16. Meltdown (everything in HEAT at 1, THICK 0.1, DRIVE 1: the h-max sketch)
17. Flat Sheet (CURVE 0, STRETCH 2: clean pitch dives)
18. Bowl Halo (TYPE BOWL, low DRIVE)
19. Bossed Organ (TYPE BOSSED, FOCUS 0.7)
20. Resonator (DRIVE 0.3, IN LEVEL 0.8; needs audio in)

Drop or replace 18 and 19 if those types are dropped.

## 10. Manual

MANUAL.md and a PDF made with the Sawstack tooling. Plain language, full sentences. Contents: what it is (the EBow on a gong picture), the three regimes and the hysteresis, a panel tour, each control in terms of what you hear, HEAT and PUMP, AGE, the presets, and at least ten patch recipes, including: an envelope into DRIVE for notes that split on accents; a sequencer into V/OCT in both TRACK modes; ROAR out into another module's filter; a kick drum into IN; the infinite gong; a slow LFO across the roar threshold; two Plate Drivers with different CAST for stereo; PUMP clocked by ear against a tempo; GRAB from a gate for chokes; feeding L back into IN through a delay. State plainly which parts are a physical model (the plate and its stretching, the contact rattle) and which are behavioural (positions along one line, the saturation curves).

## 11. Order of work

1. Port the engine with SHEET only and the tests. Get tests 1 to 9 green.
2. Module, panel, the SHEET presets.
3. The untested parts, one at a time, each behind the fuzz test: PHASE, SPREAD, audio in, drive scaling, the other three types, AGE.
4. Push, confirm the Mac arm64 CI build is green and the plugin is on the preview release.
5. Manual, PDF, README, and demo WAVs from the C++ engine into `/mnt/project-files/metal-vcv/singing/demos/`: the seven clean scenes, the five harsh scenes, and swell plus h-max for each extra type.
6. Record every deviation from this spec in the README.
