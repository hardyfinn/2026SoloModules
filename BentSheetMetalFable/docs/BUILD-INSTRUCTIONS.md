# Bent Sheet Metal: build instructions

Design by Fable, 2026-11-24, for the model that builds it. Hardy named the module and judged the sound from prototype sketches.

The **reference engine** is `tools/proto/bent_ref.c` with `scenes.h` (plain C, no dependencies: `gcc -O3 -ffast-math -o bent bent_ref.c -lm`, then `./bent saw`, `./bent flex`, `./bent dip`, `./bent break`, `./bent alive`, `./bent fuzz <seed> <name>`; `post.py out <names>` turns the raw output into WAVs and a spectrogram; `lin.py` computes the linear modes of a bent sheet and was used to tune the saw melody). The C is the definition of the sound. Where prose here is ambiguous, the C decides.

The sketches are in `/mnt/project-files/bent-sheet-vcv/sketches/`. Hardy's verdict: "Dip and break up sound great. Saw is a bit boring. Flex is okay." Then: "I'm happy to include non-linearities in pushing the model past its physical dimensions in a way to make things sound more exciting and alive." So the module leans toward water, heavy bowing and big bend sweeps. The pure saw tone is the quiet end of the bow, not the headline. Every range below deliberately runs past what real steel does. Do not tame the top of any range.

Things marked **UNTESTED** were not in the prototype. Build them last, test them hard, and say in the README what you changed.

## 1. What to build

- Plugin folder `BentSheetMetalFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-wsdoxv` (not main; there is no main). `PlateDriverFable` on branch `claude/project-thread-due7lx` is the template for the Makefile, plugin.json, panel script, Haer logo, offline test harness, Mapping.hpp pattern, preset generator, PDF tooling and README (`git fetch origin claude/project-thread-due7lx`, then `git show FETCH_HEAD:PlateDriverFable/...`). Do not copy the PlateDriverFable folder into this branch. The workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `BentSheetMetalFable`, name `Bent Sheet Metal Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `BentSheetMetal`, name `Bent Sheet Metal`, tags Synth voice, Physical modeling, Drone, Effect. Description: "A strip of spring steel that keeps ringing while you bend it, flex it through flat, dip it in water and bow it. The shape sets the pitch. Musical saw, flexatone, wobble board, thunder sheet and water gong, with every range pushed past real."
- Panel 40 HP, text labels, the Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 20 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo WAVs rendered by the C++ engine.
- Rack's namespace has an `Engine` class. Put the DSP in `namespace bent` and qualify it (Plate Driver hit this clash).

## 2. The idea in one paragraph

One strip of spring steel, modelled as 240 resonators (6 families across the width, 40 sine modes along the length) plus a 48-point map of its curvature along the length. One stretching-energy formula does everything. Curvature stiffens the sheet locally, so bending raises the pitch. Where an S-bend crosses zero curvature the sheet is softest and a note is trapped there, away from the damped ends, so it rings for a long time and is nearly a sine (the musical saw). The same formula makes loud vibration push the pitch up and couple the modes (roar). The bend itself is a slow mass held by a "hand" spring; with SNAP it has two rest positions and jumps between them, and each pass through flat cracks the sheet (wobble board, thunder). Water adds mass to the wet part, which drags the pitch down. A bow (friction, solved exactly each sample) sustains it; strikes, a beater roll and audio in also excite it.

Measured on the reference:

- Arch bend (SHAPE 0, flat sheet, f1 110 Hz): lowest singing mode 112 Hz at bend 0, 157 at 1, 247 at 2, 454 at 4, 887 at 8. It follows `f1 sqrt(1 + bend^2)`. The partial cluster also crowds together as the bend grows.
- S-bend (SHAPE 1, GRIP 1): ring time of the lowest mode goes from 1.5 s (flat) to 14 to 16 s (trapped, bend 2 and up). The next trapped note is about a fifth above.
- Bowed melody (sketch 01, 13 notes played by SPOT and bend alone from `lin.py` values): within 9 cents on all 12 plain notes. The leaned-on last note sits 171 cents sharp from amplitude alone.
- Bowed note lowered into water (sketch 03): smooth drops of 2.6 and 4.3 semitones and back.
- Light bow (P 0.2): one partial, everything else 24 to 35 dB down. P 0.4: locks a higher family. P 1: chords, then roar.
- Fuzz (16 seeds x 30 s, every control random over the full ranges of section 5, steps every 0.5 s): no NaN, zero guard events. The stiffness limiter and energy clamp of section 3.9 do fire at the extremes. They never fire in the five sketch scenes.

## 3. Engine (port `bent_ref.c` to `src/dsp/SheetEngine.hpp`, no Rack code)

### 3.1 Rates

Engine rate `fe` = 2 x host rate when the host is below 88.2 kHz, otherwise the host rate. Decimate with a half-band FIR. Controls are read and coefficients recomputed every 32 engine samples (`BLK`). Smooth every knob and CV with a 5 ms one-pole before use, except where a different time is given. `T = 1/fe`. The reference runs at 96 kHz.

### 3.2 Modes and grid

`NF = 6` families, `M = 40` lengthwise modes each, `N = 240`. `G = 48` grid points, `x_g = (g + 0.5)/G`, `xi_g = 2 x_g - 1`. Basis `phi[m][g] = sqrt(2/G) sin((m+1) pi x_g)` (orthonormal on the grid).

Per family: `kap2 = {0, 0, 1, 2.757, 5.404, 8.933}` (cut-on ratios), `gam = {0, 0.35, 1, 1, 1, 1}` (how much curvature stiffens it), `ln = {0.3, 0.6, 1.0, 2.0, 3.2, 4.5}` (stretch weight), `eb = {0, 0, 1, 0.45, 0.25, 0.15}` (bow weight).

Mode frequency, with `a = (pi / (4.730 * 5))^2` and `mm = m + 1`:

```
r    = (n == 1) ? a mm^2 + 0.5 sqrt(a) mm : kap2[n] + a mm^2
f_j  = f1 * r * (1 + det_j (0.003 + 0.02 AGE)),  clamped to 0.42 fe          det_j uniform in -1..1
T60  = DECAY * 20 / (1 + f_j/500) / (1 + 3 AGE)      al_j = 6.908 / T60
a1_j = 2 exp(-al_j T) cos(w_j T)   a2_j = exp(-2 al_j T)   b_j = exp(-al_j T) T sin(w_j T) / w_j
q_j[n+1] = a1_j q_j[n] - a2_j q_j[n-1] + b_j * fm_j
```

`f1` is the flat cut-on of family 2 and is what PITCH and V/OCT set. Everything else is built on the grid.

### 3.3 Per sample, on the grid

Forward transform every family: `u[n][g] = sum_m phi[m][g] q[n*M+m]` (keep the previous sample's `u` as `upv`; grid velocity is `(u - upv) fe`).

Curvature (recomputed per block):

```
c0 = cos(SHAPE pi/2)      s0 = 1.6 sin(SHAPE pi/2)
K_g = bend * (c0 + s0 (xi_g - spot)) + 1.2 AGE dent_g,   clamped to +-0.18 fe / f1
```

`dent_g` is five fixed random Gaussian bumps (reference `render()`).

Stretch and forces, with `wB = 2 pi f1` and `eta = 6e5 * STRETCH`:

```
Q_g     = sum_n ln[n] u[n][g]^2
S[n]    = wB gam[n] K_g u[n][g] + 0.5 eta Q_g            Ss = sum_n S[n]
kloc    = wB^2 K_g^2 + 4.5 eta |Ss| + 4.5 NF eta^2 Q_g
sc      = min(1, 1 / (kloc T^2))                         (stiffness limiter, section 3.9)
fg[n][g] = -tap[n][g] u[n][g] - sc (wB gam[n] K_g S[n] + eta ln[n] u[n][g] Ss) - dg_g v[n][g]
```

This is the gradient of `0.5 sum S^2`, which is never negative: it can move energy between modes but cannot create it. Do not simplify it. The `K u` part squared is the bend stiffening and the trap. The cross term is the softening that makes hard hits dive. The `Q^2` part is the hardening that pushes loud sounds sharp and feeds the roar.

Taper (extra local stiffness, per block): `wd = exp(-1.1 TAPER (x_g - 0.5))`, `tap[n][g] = wB^2 kap2[n]^2 (wd^-4 - 1)`, clamped to `0.5 fe^2`.

Grip and water loss (per block): `e0 = max(0, 1 - x/0.22)`, `e1 = max(0, 1 - (1-x)/0.16)`, `dg_g = 900 GRIP^2 (e0^2 + e1^2) + 3 wet_g`.

Back transform: `fm_j = sum_g phi[m][g] fg[n][g] - 0.05 kick mu_j benda` (`mu_j`: family 0 odd `mm` get `1/mm`, everything else small random, reference `render()`; `kick = 1`).

### 3.4 Water

`z = clamp((DIP - x_g)/0.05, 0, 1)`, `wet_g = z^2 (3 - 2z)`. The sheet enters the water from the `x = 0` end.

Per mode `be = 1.2 |depth| / (sqrt(r) + 0.05)`, `Gw_j = sqrt(be / (1 + be))`. When DIP > 0.001, project the total force on the wet part:

```
ft_j   = fm_j - w_j^2 q_j - 2 al_j (q_j[n] - q_j[n-1]) fe
p[n][g] = wet_g * sum_m phi[m][g] Gw_j ft_j
fm_j  -= sign(depth) * Gw_j * sum_g phi[m][g] p[n][g]
```

Positive depth is added mass (pitch falls, low partials most). Negative depth is "lighter than air" (pitch rises); there `Gw_j^2` is also limited to `0.6 (1 + cos(w_j T)) / (w_j T sin(w_j T))` so the explicit term stays stable. This doubles the transform cost, so skip it when DIP is 0.

### 3.5 Bow (after the free update `qn_j = a1 q - a2 qp + b fm`)

```
gb    = grid index of the bow position, clamped to 1 .. G-2
Ybow  = fe * sum_j eb[n]^2 b_j phi[m][gb]^2                         (per block)
vfree = fe * sum_j eb[n] phi[m][gb] (qn_j - q_j)
Fmax  = 2000 P^2
solve  d + Ybow Fmax fr(d) = V - vfree   for d, six Newton steps from last sample's d,
       derivative floored at |0.2|, each step limited to +-0.5
Fb = Fmax fr(d);     qn_j += eb[n] b_j phi[m][gb] Fb
fr(d) = tanh(d/0.02) * (0.25 + 0.65 exp(-|d|/0.25))
```

Starting Newton from the previous `d` is what gives stick-slip its hysteresis. Keep it. The bow does not touch families 0 and 1; with them included it just drags the whole sheet sideways (tried).

### 3.6 Strike, roll, audio in

- **Strike:** `qp_j -= v phi[m][gs] T exp(-f_j / (400 + 6000 HARD^2)) * (n == 1 ? 0.5 : 1)` for every mode, `gs = floor(pos * G)`.
- **Roll:** in the reference this is scene code (`sc_flex`, first 10 s): strikes at 11.5 per second alternating position 0.37 and 0.58 and strength 16 and 11. As a control it is UNTESTED: see section 5.
- **Audio in. UNTESTED** (the fuzz drives it with noise bursts, no listening): add `in` to `fg[n][gs]` for every family, times 0.3 for families 0 and 1.

### 3.7 The bend (once per sample)

```
handS += (hand - handS)(1 - exp(-T/0.002))
we = 2 pi 7 sqrt(f1/130.81)       wh = 2 pi (1.5 + 60 HOLD^2)       z = 0.12 + 0.6 HOLD
dVdb = sum sc S[n] wB gam[n] u[n][g] (c0 + s0 (xi_g - spot))       kb = sum wB^2 gam[n]^2 u^2 (c0 + s0 (xi_g - spot))^2
acc  = -wh^2 (bend - handS) - we^2 snap (-2.2 bend + bend^3/2.25) - 2 z sqrt(wh^2 + we^2) bendv - dVdb + shake
dv   = T acc / (1 + T ct + T^2 kt)      ct = 2 z sqrt(wh^2 + we^2)     kt = wh^2 + kb + we^2 snap 3 bend^2 / 2.25
benda = clamp(dv fe, +-3e5);  bendv += dv;  bend += bendv T;  bend clamped to +-30 (zero the velocity at the stop)
```

The semi-implicit `dv` is required: with a plain explicit step the bend ran away whenever loud sound stiffened it (found by the fuzz).

**Crack:** when `bend` changes sign, `qp_j -= 0.08 crack |bendv| mu2_j T exp(-f_j / (700 + 3000 HARD)) * sign(bendv)` for every mode (`mu2_j` uniform in -1..1). This is the thwack of a wobble board. Hardy did not enjoy collision sounds in an earlier project, so CRACK has its own knob and must reach silence at 0.

### 3.8 Outputs

Area pickups, not point pickups (a point pickup away from the trap hears nothing of the saw note; this cost a debugging round): `aL_g = 0.5 cos(pi x_g/2)(0.6 + 0.4 r)`, `aR_g = 0.5 sin(pi x_g/2)(0.6 + 0.4 r)`, `r` fixed random.

```
L = fe * sum_n o_n sum_g aL_g (u[n][g] - upv[n][g])          o_0 = 0.5, others 1
R = fe * sum_n o_n (n odd ? -1 : 1) sum_g aR_g (u[n][g] - upv[n][g])
uLs = sum over n >= 2 and g of aL_g u[n][g];   gl = 1 + 0.85 PICKUP tanh(2500 uLs);   L /= gl^2      (same for R)
g = 1 + 14 OUT^2;   L = 1.5 tanh(g L / 1.5) / sqrt(g)
60 Hz one-pole high-pass
```

Because the trapped note lives at the S-point, moving SPOT pans the sound. Keep that.

After decimation, a leveller that matches what `post.py` did for the sketches: RMS of both channels over 150 ms, gain = `(max(rms, 0.01 ref) / ref)^-0.5` with `ref = 1.0`. Then LEVEL, scaled so the Init preset (section 9) is 5 V peak, then a final `11 tanh(v / 11)`.

- **SNAP out:** 1 ms 10 V trigger at each sign change of `bend` where `|bendv| > 2`.
- **BEND out:** `10 V * bend / 24`, the sheet's actual bend after HOLD and SNAP (bipolar, clamped to +-10 V).
- **ENV out:** `10 V * clamp(sqrt(E / 2e4), 0, 1)`, `E` the total mode energy of the reference diagnostics, smoothed 20 ms. Calibrate the 2e4 so the thunder section of the flex scene reaches about 8 V.

### 3.9 Safety (all in the reference; port all of it)

1. Stiffness limiter `sc` (section 3.3). It scales the stretch forces at a grid point when their local stiffness would exceed the explicit limit. Count its firings.
2. Soft energy ceiling: if total energy `E > 5e6`, scale every `q` and `qp` by `sqrt(5e6 / E)`. Count it.
3. Guard: if `E` is not below `1e12` (or NaN), halve every state and count it. No test may trigger this.
4. Clamps on `K_g`, `tap`, `benda`, `bend`, and the 2 ms slew on the hand, as written above.
5. Flush denormals. When nothing is exciting the sheet and `E < 1e-12`, skip the mode loop.

All constants that contain `fe` or `T` must use the real engine rate.

### 3.10 Cost

The reference (double, scalar, 96 kHz) uses about 30% of one cloud core without water and 55% with it. The cost is the two transforms, `2 * 240 * 48` multiply-adds per sample, doubled by water. Use float for the transforms and let the compiler vectorise them (store `phi` so the inner loop is contiguous). Keep the mode recursion in double if float drifts. Target below 20% of one core at 48 kHz host on Apple silicon; report the figure with and without water. Do not cut modes or grid points to reach it without saying so.

## 4. Seeds

One xorshift64 stream seeded `0x2545F4914F6CDD1D`, drawn in the order used in `render()` (dents, pickup weights, then per mode `det`, `mu2`, `mu`). The module has no CAST control; the sequence is fixed and must reproduce the reference. Check this in test 1.

## 5. Controls

Every control marked (CV) has a jack summed at 0.1 per volt (so 10 V sweeps a 0 to 1 knob fully, +-5 V sweeps a bipolar one) unless stated.

| Section | Control | Range and default | Maps to |
|---|---|---|---|
| SHEET | PITCH + V/OCT | -3 to +3 octaves, 0 | `f1 = 130.81 * 2^(PITCH + V/OCT)`. Resizes the whole sheet. Tracks exactly at fixed shape. |
| | TAPER (CV) | 0 to 1, 0.7 | `TAPER`. 0 is a rectangle, 1 a saw blade three times wider at the handle. |
| | GRIP (CV) | 0 to 1, 0.7 | `GRIP`. 0 hangs free and rings everywhere. 1 leaves only trapped notes ringing. |
| | DECAY | 0.25 to 4, 1 | `DECAY` |
| SHAPE | BEND (CV, large knob, bipolar) | -1 to 1, 0.45 | `hand = 24 x |x|`. Past about 0.6 is beyond what steel survives. |
| | BEND V/OCT (jack only) | volts | `hand' = sign(hand) sqrt(4^v (1 + hand^2) - 1)`, `v` clamped so the root stays real. Exact semitones for SHAPE 0 on an untapered sheet, approximate elsewhere. **UNTESTED.** |
| | SHAPE (CV) | 0 to 1, 1 | `SHAPE`: arch to S-bend |
| | SPOT (CV, bipolar) | -1 to 1, 0 | `spot = 0.8 x`: where the S turns over |
| | HOLD (CV) | 0 to 1, 0.9 | `HOLD`: firm hand follows BEND, loose hand lets the sheet wobble |
| | SNAP (CV) | 0 to 1, 0 | `snap = 1.5 x`. Above about 0.6 the sheet flips itself violently. |
| | CRACK | 0 to 1, 0.35 | `crack = 3 x^2` |
| WATER | DIP (CV) | 0 to 1, 0 | `DIP` |
| | DEPTH (bipolar) | -1 to 1, 0.5 | `depth = 4 x^2` for x > 0 (0.5 is water, 1 is far heavier), `depth = x` for x < 0 (lighter than air, pitch rises) |
| BOW | BOW (CV, large knob) | 0 to 1, 0.27 | `P = 1.6 x^1.5`. About 0.27 is the pure saw voice, 0.5 chords, above 0.7 roar. |
| | SPEED (CV) | 0 to 1, 0.34 | `V = 0.05 * 60^x` (0.05 to 3). Sets loudness and how far it is pushed. |
| | BOW POS (CV, bipolar) | -1 to 1, 0 | bow at `clamp(0.5 + 0.5 spot SHAPE + 0.4 x, 0.04, 0.96)`: it rides the S-point; the offset picks other trapped notes |
| | BOW GATE (jack) | | scales `P` through a 50 ms slew. Unpatched means on. |
| HIT | STRIKE (jack), VEL (jack) | | strike with `v = 60 * vel`, `vel = VEL volts / 10`, 0.7 unpatched, at position 0.37 |
| | HARD | 0 to 1, 0.5 | `HARD` |
| | ROLL (CV) | 0 to 1, 0 | above 0.02: self-strikes at `4 * 5^x` per second, alternating position 0.37 and 0.58 and `v` 16 and 11 times VEL/0.7. **UNTESTED as a control.** |
| | IN (jack), IN LEVEL | 0 to 1, 0.5 | `in = 3000 * IN LEVEL^2 * volts / 5` **UNTESTED**; calibrate so 5 V of pink noise at 0.5 makes the sheet ring about as loud as a medium strike |
| CHARACTER | STRETCH (CV) | 0 to 3, 1 | `STRETCH`. 0 is a linear sheet. 3 lets loudness throw the pitch by an octave or more. |
| | PICKUP | 0 to 1, 0.2 | `PICKUP` |
| | OUT | 0 to 1, 0 | `OUT` saturation |
| | AGE | 0 to 1, 0.3 | section 6 |
| | LEVEL | 0 to 1, 0.7 | output |

**Jacks.** Inputs: V/OCT, BEND V/OCT, BEND, SHAPE, SPOT, HOLD, SNAP, TAPER, GRIP, DIP, BOW, SPEED, BOW POS, BOW GATE, STRIKE, VEL, ROLL, STRETCH, IN. Outputs: L, R, SNAP, BEND, ENV. Monophonic: use channel 1 of any poly cable.

BEND, DIP, SPOT and BOW CVs must work at audio rate (do not smooth them more than 5 ms; the 2 ms hand slew is inside the engine).

## 6. AGE

One knob, 0 to 1. At 0 everything below is off.

- In the reference: dents in the curvature (`1.2 AGE dent_g`), mode detune (`0.02 AGE`), extra loss (`1 + 3 AGE`).
- **UNTESTED**, each small:
  - Tired hand: `hand *= 1 + 0.03 AGE r(t)`, `r` a random walk band-limited to 6 Hz.
  - Fatigue: a constant force `0.3 AGE we^2` added to the bend's `acc`, so with SNAP the sheet prefers one side and snaps unevenly.
  - Warm-up: `heat` follows total energy with a 20 s time constant, normalised so a held roar gives 1; `f1 *= 1 - 0.008 AGE heat`.
  - Rosin: the bow's `Fmax` drifts by `1 + 0.2 AGE r2(t)`, `r2` band-limited to 0.5 Hz.

## 7. Tests (`make test`, no Rack needed; shape of `PlateDriverFable/test/`; every check prints PASS or FAIL)

1. **Reference match.** With the leveller off and AGE as each scene sets it, the C++ engine at 96 kHz reproduces the reference scenes `saw`, `flex`, `dip`, `break`, `alive`. Compare third-octave band levels over each 2 s window, within 2 dB where the reference is within 40 dB of its peak. The first 2.5 s of `saw` (one pure tone) must match to 0.2 dB and 2 cents.
2. **Bend raises pitch.** SHAPE 0, TAPER 0, GRIP 0, struck softly (`v` 5): lowest strong partial within 2% of `f1 sqrt(1 + hand^2)` at hand 0, 1, 2, 4, 8.
3. **Trapping.** SHAPE 1, TAPER 0, GRIP 1, hand 4, struck (`v` 30): 3 s after the strike, level decays slower than 8 dB/s. Same with hand 0: faster than 25 dB/s.
4. **Saw tuning.** The first six notes of the `saw` scene are within 15 cents of 220, 329.63, 293.66, 261.63, 220, 196 Hz.
5. **Bow regimes.** SHAPE 1, hand 4, GRIP 1, f1 110, V 0.2: at P 0.2 the strongest partial holds more than 97% of the power; at P 1.0 and V 1.0 at least three partials are within 12 dB and the strongest sits more than 100 cents above its P 0.2 pitch.
6. **Energy.** After a strike of `v` 60 with no bow, bend fixed (HOLD 1, SNAP 0), DIP 0: total energy never rises (check every 10 ms), at STRETCH 0, 1 and 3.
7. **Snap.** SHAPE 0, HOLD 0.2, SNAP 0.55 (`snap` 0.83), hand swung +-3 at 2.5 Hz: `bend` changes sign at least 4 times a second, each crossing produces a SNAP trigger, and with CRACK 0 the crossings add less than 1 dB of broadband level compared with CRACK 0.35.
8. **Water.** The bowed section of the `dip` scene: pitch falls by 2 to 6 semitones as DIP rises and returns within 20 cents. With DEPTH -1 and the same DIP, pitch rises.
9. **STRETCH.** Bowed tone at P 1, V 1: pitch rise above the P 0.2 pitch is under 20 cents at STRETCH 0, 100 to 250 at 1, and larger at 3.
10. **Fuzz.** 400 random patches over every control at its full range (as `sc_fuzz`, plus the Rack-level mappings), new values every 0.5 s, strikes and noise bursts into IN, 20 s each: no NaN, no guard event, output within +-11 V. Report how often the stiffness limiter and energy clamp fire.
11. **Sample rates.** Tests 2, 4 and 10 (100 patches) at host rates 44.1, 48 and 96 kHz.
12. **Silence.** BOW 0, no strike, no input, CV still: output exactly zero after 60 s.
13. **Audio-rate bend.** BEND CV a 200 Hz sine of +-2 V on a bowed tone: no guard event, no NaN.
14. **Speed.** Report the engine's share of one core at 48 kHz host rate, bowed roar, with DIP 0 and DIP 0.5.

## 8. Panel (40 HP = 203.2 mm; adjust positions so labels do not collide; render a preview PNG and look at it)

Captions over each section. BEND and BOW are large knobs. A small horizontal display strip above the SHAPE section draws the sheet's current curve from `K_g` (a line bent by the live `bend`, SHAPE and SPOT, with a water level rising from the left as DIP rises and a mark at the bow position). It makes the module readable at a glance; draw it with nanovg in a widget, about 60 mm x 14 mm. If it costs more than half a day, ship a row of five lights showing `bend` instead and say so.

| Row | Contents, left to right |
|---|---|
| 1 knobs | SHEET: PITCH, TAPER, GRIP, DECAY. WATER: DIP, DEPTH. CHARACTER: STRETCH, PICKUP, OUT, AGE, LEVEL |
| 2 knobs | SHAPE: BEND (large), SHAPE, SPOT, HOLD, SNAP, CRACK. BOW: BOW (large), SPEED, BOW POS |
| 3 knobs | HIT: HARD, ROLL, IN LEVEL (smaller knobs; share the row with the display strip if that reads better) |
| 4 jacks | V/OCT, BEND V/OCT, BEND, SHAPE, SPOT, HOLD, SNAP, TAPER, GRIP, DIP |
| 5 jacks | BOW, SPEED, BOW POS, BOW GATE, STRIKE, VEL, ROLL, STRETCH, IN, then outputs SNAP, BEND, ENV, L, R on a dark plate (14 jacks: narrow the spacing or move SNAP, BEND and ENV up beside the display) |

## 9. Presets (20 .vcvm files, numbered; each sounds with nothing patched unless noted)

Lean toward water, heavy bow and bend sweeps. Where a preset needs motion, say in its notes which LFO to patch, and also make it sound good held still.

1. Init (defaults: a bowed S-bent blade singing one note; raise BOW and it roars)
2. Alive (the `alive` scene held at its middle: BOW 0.55, SPEED 0.6, GRIP 0.6, AGE 0.3, STRETCH 1.3; notes: slow LFO to BEND and to BOW)
3. Dragged Under (BOW 0.65, SPEED 0.7, DIP 0.35, DEPTH 0.55; notes: LFO to DIP)
4. Break-up (the `break` scene at 20 s: TAPER 0.5, GRIP 0.2, SHAPE 0.5, HOLD 0.3, SNAP 0.45, BOW 0.9, SPEED 0.9, AGE 0.6, STRETCH 1.8)
5. Water Gong (BOW 0, SHAPE 0, BEND 0.2, GRIP 0, TAPER 0; needs STRIKE; notes: envelope or LFO to DIP)
6. Mercury (as 5, DEPTH 1, DIP 0.5)
7. Lighter Than Air (bowed, DEPTH -1; notes: LFO to DIP, pitch rises as it dips)
8. Singing Saw (BOW 0.27, SPEED 0.34, GRIP 1, TAPER 1; notes: sequencer to SPOT or BEND V/OCT)
9. Saw Fifths (BOW POS 0.25 so the bow picks the next trapped note; BOW 0.4)
10. Overbowed (BOW 0.8, SPEED 0.8, STRETCH 2)
11. Scream (BOW 1, SPEED 1, PICKUP 1, OUT 0.6, STRETCH 3)
12. Flexatone (BOW 0, SHAPE 0, ROLL 0.65, GRIP 0.1, BEND 0.35; notes: LFO to BEND)
13. Wobble Board (BOW 0, SHAPE 0, HOLD 0.2, SNAP 0.37, GRIP 0.2, CRACK 0.5; needs a 2 to 3 Hz bipolar LFO in BEND)
14. Thunder Sheet (BOW 0, SHAPE 0, HOLD 0.38, SNAP 0.6, GRIP 0.05, STRETCH 2, PITCH -1; needs a fast LFO or noise in BEND)
15. Self Flipper (SNAP 1, HOLD 0.1, BOW 0.6: the sheet throws itself about with nothing patched, if it does; otherwise retune until it does or note the LFO needed)
16. Sub Sheet (PITCH -2.5, BOW 0.5, SPEED 0.5, BEND 0.3)
17. Glass Blade (PITCH +2, GRIP 1, BOW 0.25, DECAY 3)
18. Rusted (AGE 1, BOW 0.5, GRIP 0.4)
19. Linear Sheet (STRETCH 0, BOW 0.3: clean, for contrast)
20. Resonator (BOW 0, IN LEVEL 0.7, GRIP 0.3; needs audio in; notes: play BEND while audio runs)

Check every preset sounds through the full module chain, and that 2, 3, 4, 10 and 11 are clearly not a polite sine.

## 10. Manual

MANUAL.md and a PDF made with the Plate Driver tooling. Plain language, full sentences. Contents: what it is (a strip of steel whose shape is the instrument), the five things you do to it (bend, S-bend and trap, flex and snap, dip, bow), a panel tour, each control in terms of what you hear, where the ranges go past real, AGE, the presets, and at least ten patch recipes, including: LFO to DIP under a held bow; envelope to BOW so accents roar; sequencer to SPOT for saw melodies and to BEND V/OCT for arch melodies; bipolar LFO to BEND for wobble board; audio-rate oscillator into BEND; SNAP out clocking another module; BEND out to a filter so the filter follows the sheet; ENV out back into DIP so loud passages sink; drums into IN while BEND is swept; two modules a fifth apart.

## 11. Order of work

1. Port the engine and the tests. Get tests 1 to 9 and 12 green.
2. Fuzz (10, 11, 13), speed (14).
3. Module, panel, display strip, the presets.
4. The untested parts, one at a time, each behind the fuzz test: BEND V/OCT, ROLL as a control, audio in, AGE extras.
5. Push, confirm the Mac arm64 CI build is green and the plugin is on the preview release.
6. Manual, PDF, README, and demo WAVs from the C++ engine into `/mnt/project-files/bent-sheet-vcv/demos/`: the five scenes, plus a 10 s clip of each preset (drive the ones that need an LFO or trigger from the test harness and say so in the file name).
7. Record every deviation from this spec in the README.

Hardy's standing preferences: Apple silicon build only; text labels; short names searchable in Rack; PDF manual with patch recipes; do not write to the creations tracker sheet unless Hardy says so; when presenting demos, attach only a few and say which to start with (Hardy does not listen to them all): start with Alive, Dragged Under and Break-up.
