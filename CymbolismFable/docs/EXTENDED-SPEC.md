# Cymbolism Extended: design and build instructions

Written by Fable (2026-11-21) for Opus to build. Hardy's brief: take the Cymbolism engine and "change parameters to make them unrealistic", as "a new module called Symbolism Extended" (spelt Cymbolism, as before).

Read first: `docs/BUILD-INSTRUCTIONS.md` (the realistic module's spec) and the source in `src/`. This document only describes what is new. Where it is silent, Extended behaves exactly like Cymbolism.

---

## 1. What it is

Cymbolism is an honest cymbal. Cymbolism Extended is the same instrument with the laws of physics put on knobs. It keeps every control of the realistic module and adds a section called UNREAL.

The design rule: **every unreal control changes one physical law, not the signal.** There are no filters, pitch shifters or effects after the cymbal. Each control reaches into the model (the mode frequencies, the losses, the coupling, the contact, the pickup) so the result still behaves like an object: it still blooms when hit hard, still chokes, still answers to where and how it is struck.

With every UNREAL control at its neutral position, Extended must sound identical to Cymbolism (test X1).

### What was prototyped

Fable prototyped the main ideas offline on a hacked copy of the engine before writing this. The source is in `tools/extended-proto/` (`px.cpp` and `engine-hooks.diff`). The renders are in `/mnt/project-files/cymbal-vcv/extended-sketches/`. They were checked by measurement, not by ear.

| Idea | Result |
|---|---|
| TUNE to a chord | Works. At 100%, 99% of the energy between 100 Hz and 8 kHz sits within 20 cents of a chord tone (18% untuned). Zero guard events. |
| WARP (stretch, compress) | Works, zero guard events, but only with the A rescale in 3.2. Compressed spectra are much louder and need level compensation. |
| WARP negative (inverted spectrum) | Works. Many modes land below 30 Hz and must be dropped (3.3). |
| SUSTAIN (self-sustaining ring under an energy thermostat) | Works. Held a steady, evolving wash for 16 s with zero guard events. COLOUR moved the dominant region from 190 Hz to 3.3 kHz. |
| SWELL (reverse cymbal from silence) | Works with the same mechanism. |
| BLOOM negative, FLOW slow and fast | Work, zero guard events. |
| MORPH (crash to china to bell frequencies under a roll) | Works, zero guard events. |
| LOAD (sliding weight) | Works but is subtle at 40 g. Range raised to 500 g. |
| Exact time reversal (swap states, negate losses) | **Dropped.** It retraces a soft linear hit, but the statistical cascade is not reversible: energy it injected late regrows for the whole rewind and the result is a 10 kHz screech. SWELL covers the musical use. |

Three more things the prototype found, which the build must respect:

- A contact that is alive while the active mode set changes reads out of bounds. Contact vectors must be sized to all modes (3.3).
- A zero-loss freeze with BLOOM on trips the energy guard. FREEZE must be a thermostat, not zero loss (4.7).
- A pure frequency remap of the core without the A rescale is violently unstable (thousands of guard events).

Not prototyped, so build these last and cut them if they misbehave: PUMP, BOUNCE, RAIN, the new types, PIEZO.

---

## 2. Where it lives

- Second module in the existing plugin `CymbolismFable`: slug `CymbolismExtended`, name "Cymbolism Extended". Plugin version becomes 2.1.0.
- It shares `CyGeometry`, `CyModes`, `CyEngine`, `CyRunner`. Do not fork the engine. Add the new behaviour to the shared engine behind fields that default to neutral, so Cymbolism is untouched (X1 guards this).
- New files: `src/CyUnreal.hpp` (the frequency and loss transforms of section 3, pure C++), `src/CymbolismExtended.cpp`, `res/CymbolismExtended.svg` from `res/make_panel_extended.py`.
- Mono compatible, stereo out.

---

## 3. Engine changes that everything else rests on

### 3.1 The frequency pipeline

Today each mode's frequency is fixed at build (model frequency times hammering) and only SIZE and MOUNT change it. Extended computes the working frequency of every mode at control rate through this chain, in this order:

```
f_base   = model frequency x hammering                    (as now)
f1 = MORPH    f_base^(1-x) * f_B^x                         (4.1)
f2 = LOAD     f1 / sqrt(1 + m * phi_i(x_load)^2)           (4.9)
f3 = TENSION  f2 * sqrt(max(0.05, 1 + tau * s_i))          (4.8)
f4 = WARP     pivot * (f3 / pivot)^gamma                   (4.2)
f5 = TUNE     pulled towards the nearest allowed pitch     (4.3)
f6 = V/OCT    f5 * 2^volts                                 (4.3)
f  = f6 / lambda                                           (SIZE, as now)
```

Put this in `CyUnreal.hpp` as one function that fills a frequency table from a small struct of settings. `Engine::retune()` calls it instead of reading `om0` directly.

Cost control: recomputing 2,000 modes costs a `pow`, a `log2` and a `cos` each. Recompute at most every fourth control block (128 samples) and only when a setting changed by more than a small threshold. Smooth the knobs first. Target: under 8% of a core with every unreal CV moving.

### 3.2 Keep the nonlinear core stable (required)

The core's potential is bounded only while `diag(w^2) - A^T A` is positive semi-definite. The build guarantees that for the built frequencies. Any retune of a core mode breaks it unless A moves with it.

Rule: whenever core mode i's frequency changes from `f_built` to `f_now`, scale every `A` entry of that mode by `f_now / f_built`. This keeps the normalised matrix `D^-1/2 A^T A D^-1/2` identical, so the build's margin is preserved exactly. Implement it as a per-core-mode factor `aFac[i]` applied where `an` and `za` are formed in `nonlinearGradient`, not by mutating the shared model.

The B tensors are not rescaled. They depend on shapes, and shapes do not change.

### 3.3 Active modes become a mask

After WARP or MORPH the modes are no longer in frequency order, so "the first NA modes" is wrong.

- Replace the prefix count `NA` with a per-mode active flag and a compact index list rebuilt at retune.
- A mode is active when 25 Hz < f < 0.45 fs. Core modes are always active: clamp their frequency into that range instead of dropping them, because the nonlinear gradient indexes them.
- Modes below 40 Hz stay out of the pickup and out of the bow, as now.
- Size every contact vector (`Contact::phi`, `bowPhi`, grain projections) to all N modes, so a retune during a contact is safe.

### 3.4 The cascade follows shape, not pitch

Assign each upper mode's cascade band from its **built** frequency, not its warped one. Coupling between modes is set by their wavelengths, and those do not change when the frequency law is bent. A side effect worth having: with an inverted spectrum the bloom travels downwards in pitch.

Losses, by contrast, use the working frequency: what you hear at a pitch decays as that pitch should.

### 3.5 Level

Bent spectra change loudness a lot (a compressed spectrum sums coherently). Add an automatic make-up gain computed at retune from the spread of the working frequencies, calibrated so a medium hit stays between 2 and 6 V peak across the whole WARP and TUNE range (test X3). Smooth it over 50 ms.

---

## 4. The UNREAL controls

Each entry gives the law being changed, the range, and the neutral position.

### 4.1 MORPH and TYPE B
A second cymbal, B, is built in the background from TYPE B's default geometry at the same weight and alloy. Each mode of the main cymbal is matched to B's mode with the same angular order, the same parity and the same rank in frequency. MORPH interpolates each mode's frequency between the two on a log scale. Shapes, couplings and the pickup stay those of the main cymbal.

- Range 0 to 1, neutral 0. CV.
- Unmatched modes (B has fewer in that class) continue B's last ratio.
- B's table goes in the model cache under its own key.
- What it does: sweeps a crash's pitches into a china's or a bell's while it rings. At 1 it is "B's pitches on A's body".

### 4.2 WARP and PIVOT
The frequency law is bent around a pivot: `f' = pivot * (f / pivot)^gamma`.

- WARP is gamma, from -1 to +2.5, neutral +1. PIVOT 100 Hz to 5 kHz, default 700 Hz. Both CV.
- Above 1 the spectrum stretches: wide, sparse, glassy. Between 0 and 1 it compresses: dense, gong-like, louder. At 0 every mode sits at the pivot: one enormous beating unison. Below 0 the spectrum is mirrored: the big slow shapes ring high and the fine ripples ring low.
- Give the knob a detent or a wide dead zone at +1.

### 4.3 TUNE, SCALE, ROOT and V/OCT
Each mode is pulled towards the nearest pitch of a scale.

- TUNE 0 to 100%, neutral 0. At 100% keep a residual random spread of about ±4 cents per mode so the chorus survives; do not collapse modes onto identical frequencies.
- SCALE (stepped): CHROMATIC, MAJOR, MINOR, DORIAN, PENT MAJOR, PENT MINOR, BLUES, WHOLE TONE, FIFTHS (root and fifth), OCTAVES (root only), HARMONIC (the harmonic series of the root, not a scale), CHORD.
- CHORD takes its pitch classes from the polyphonic **CHORD** input (V/oct per channel, up to 16 notes, any octave). Unpatched, it is a minor ninth.
- ROOT: 12 semitones, plus the **V/OCT** input. V/OCT transposes every mode (step f6), so the cymbal can be played melodically whether or not TUNE is up. Sampled continuously, slewed 5 ms.
- Twins of a mode pair must land on the same target, so that hammering still makes them beat.

### 4.4 TILT
The frequency dependence of the loss is tilted: `sigma' = sigma * (f / 1 kHz)^(1.5 * tilt)`, clamped to between 0.05 and 60 per second.

- Range -1 to +1, neutral 0. CV.
- Negative: the highs outlast the lows, which no real plate does. Positive: an exaggerated, quickly darkening ring.

### 4.5 SUSTAIN, SWELL and COLOUR
Negative damping under an energy thermostat: the cymbal sings on its own.

- Each control block: `u = clamp(1 - E / E_target, -1, 1)`. For u > 0, mode i gets extra loss `-G * u * w_i`. For u < 0, every mode gets `+G * |u|`. E is the total modal energy.
- `w_i` is a Gaussian in log frequency around COLOUR, 1.5 octaves wide. COLOUR 60 Hz to 10 kHz.
- SUSTAIN sets `E_target` (0 = off, neutral). Scale it so that full is the energy of a hard hit. The **HOLD** gate input forces it on at the knob's level while high, so a gate plays a swell.
- SWELL sets G from 0.5 to 30 per second: the rise time, which is the length of the reverse cymbal.
- Something must seed it: if the energy is below a tiny floor when SUSTAIN engages, fire one very soft brush tap.
- The energy guard must treat this as driven, and separately hard-cap E at 4 x E_target.
- What it does: reverse-cymbal swells played from a gate; endless drones whose colour is steered by COLOUR while BLOOM keeps redistributing the energy; a crash that never dies.

### 4.6 BLOOM (both directions) and FLOW
- BLOOM runs from -2 to +2. The engine's form `V = (A.q) z + z^2 / 2` with `z = bloom * zeta` is bounded for either sign, so negative BLOOM is safe: it flips the sign of the curvature term, which reverses the direction of the pitch glide. The cascade uses `|bloom|`.
- FLOW scales the cascade rates from 0.05x to 10x (log), neutral 1x. Slow: the bloom takes seconds to climb, a slow-motion crash. Fast: the wash is there at once and burns out.

### 4.7 FREEZE
A button and a gate. While on, the thermostat of 4.5 holds the energy at whatever it was when FREEZE went on, with flat weights (`w_i = 1`) and all ordinary losses scaled to 10%. Not zero loss: that trips the guard. The spectrum keeps evolving through the coupling while the level stays put. Release returns to normal decay.

### 4.8 TENSION, live, and PUMP
TENSION currently needs a rebuild. Make it live.

- At build, export for every mode the sensitivity `s_i = (phi_i^T K_G phi_i) / w_i^2`, where `K_G` is the geometric stiffness of the unit residual-stress pattern already used by the prestress code. Then `w_i^2 -> w_i^2 * (1 + tau * s_i)`, clamped at 0.05 so nothing buckles.
- TENSION becomes -1 to +1 with no rebuild, and applies in both modules.
- **PUMP** is an audio input that adds to tau. For the core modes, recompute the coefficient every sample (64 to 96 cosines per sample is affordable). For the upper modes, every 32 samples.
- This is parametric excitation. A pump at twice a mode's frequency makes that mode grow from nothing once the depth passes a threshold. Feeding it an oscillator, or the module's own output, gives subharmonic tones no strike can.
- Test X6 checks the threshold. If it fails or sounds like nothing, keep live TENSION and drop PUMP.

### 4.9 LOAD and LOAD POS
A weight stuck to the cymbal: 0 to 500 g at a radius from bell to edge, 90 degrees round from the strike point.

- First-order mass loading: `f' = f / sqrt(1 + m * phi_i(x)^2)` with the mass-normalised shapes.
- Only modes that move at that point drop in pitch, so pairs split and beat. Moving LOAD POS by CV sweeps a flanging pattern across the spectrum.
- Both CV, neutral 0 g.

---

## 5. Shape, pushed past real

### 5.1 Wider ranges on the existing knobs

| Control | Cymbolism | Extended |
|---|---|---|
| SIZE | 0.6x to 1.5x the type | 0.2x to 3x (about 2" to 60") |
| WEIGHT | 0.5x to 2.5x | 0.2x to 5x |
| TAPER | 1 to 2.5 | 0.4 to 3 (below 1 the edge is thicker than the centre) |
| PROFILE | 0 to 14% | 0 to 100% of the radius (deep bowls) |
| BELL | 0 to 2x | 0 to 3x |
| FLANGE | -20 to +25 degrees | -80 to +80 degrees |

- The model is built to 1.5x the audio band so SIZE can scale without a rebuild. When the SIZE **knob** sits above 1.5x, rebuild with the band factor equal to the knob value so the top octave is not empty. CV excursions never trigger this.
- The shell solver is general, but the nonlinear part uses a shallow-shell stress function. For steep profiles rely on the existing per-mode A clamp, and report `aScale` and `minEigCore` for every new type in `modes_check`. If a profile cannot be made stable, limit that knob's range and say so.

### 5.2 HOLE
The centre hole radius, from the normal 6.5 mm up to 80% of the radius: a ring. Rebuild. The mount springs act at the hole as now.

### 5.3 More materials
ALLOY gains ALUMINIUM, GLASS and WOOD. Elasticity and density mostly move pitch, which SIZE already does, so the audible difference is the loss law:

- GLASS: very low loss, nearly flat with frequency.
- WOOD: loss rising with the square of frequency, so highs die in milliseconds.
- ALUMINIUM: lighter and brighter than bronze, with moderate loss.

### 5.4 New types
Added after the ten existing types. Each is just a row of geometry.

| Type | Geometry | Why |
|---|---|---|
| GONG | 28", 1.2 mm, nearly flat, no bell, rim turned back 60 degrees | tam-tam: the slowest, biggest bloom |
| SHEET | 30", 0.3 mm, flat, no bell | thunder sheet: extreme nonlinearity |
| BOWL | 8", 3 mm, profile 90%, no bell | singing bowl: bow it |
| PLATE | 12", 4 mm, flat, taper 1 | bell plate: pitched, long |
| RING | 14", 2 mm, flat, hole 60% | ring gong |
| DOME | 10", 1.5 mm, bell 75% of the diameter | a cymbal that is all bell |

---

## 6. Playing and output

- **BOUNCE** (0 to 1): the beater is held against the cymbal by a return force instead of flying off, so it ricochets: buzz rolls and accelerating bounces that emerge from the contact model. Use the existing `gravity` field on the beater contact (the rivets already work this way), from 0 to about 5,000 m/s², and extend the contact's lifetime to 1.5 s. Neutral 0.
- **RAIN**: a seventh PLAY position. While ROLL is high, small random impacts land all over the surface (the brush's grain engine with the patch widened to the whole cymbal). RATE sets density (20 to 2,000 per second), FORCE the size, HARDNESS the brightness. STRIKE gives a single burst.
- **Stereo**: two mics at plus and minus an angle around the cymbal. WIDTH 0 to 90 degrees. The pickup table already holds everything needed: for a mic at azimuth psi, the cos twin's weight is multiplied by `cos(m * (off - psi))` and the sin twin's by `-sin(m * (off - psi))`. One extra dot product per sample. R is normalled to L when only L is patched.
- **PICKUP** (AIR to PIEZO): crossfades the radiated-sound mic with a contact pickup at the MIC radius, which is simply the surface velocity there, `sum phi_i(x) v_i`. Dry, close and gritty, with no air in it. Match loudness on a medium hit.
- **Extra outputs**: CORE (the coupled low modes only), WASH (everything above them), and ENV (0 to 10 V, a follower of the core energy, for using the cymbal as a modulation source).

---

## 7. Panel

60 HP. Same house style and colours as Cymbolism, with the UNREAL section on a verdigris ground (`#7FA58F` accents) so it reads as the strange half. Control labels 3.2 mm, jack labels 2.9 mm, Hær logo on the bottom rail.

- Top: the screen (as Cymbolism, plus a line showing the scale and root, and a thin strip that draws the warped frequency law), then TYPE, TYPE B, NEW CYMBAL, large POSITION and FORCE, and the STRIKE, CHOKE and FREEZE buttons.
- Row 1, CYMBAL: SIZE, WEIGHT, TAPER, PROFILE, BELL, FLANGE, HOLE, ALLOY, HAMMER, TENSION.
- Row 2, UNREAL: MORPH, WARP, PIVOT, TUNE, SCALE, ROOT, TILT, SUSTAIN, SWELL, COLOUR, FLOW, LOAD, LOAD POS.
- Row 3, SOUND and PLAY: DECAY, BLOOM, DAMP, MOUNT, SIZZLE, MIC, PICKUP, WIDTH, PLAY, HARDNESS, RATE, BOUNCE, EXC.
- Jacks: STRIKE, VEL, CHOKE, ROLL, HOLD, FREEZE, EXCITE, PUMP, V/OCT, CHORD (poly), PLAY, then a CV input for every knob in rows 1 to 3 except FLANGE, HOLE, ALLOY and SCALE. Outputs L, R, CORE, WASH, ENV on a gold ground.
- If the jacks do not fit in two rows at 60 HP, go to three rows by shortening the screen. Do not drop CV inputs.

Context menu: as Cymbolism, plus "Reset the UNREAL section" and "Tuning follows V/OCT only at strikes".

---

## 8. Tests (add to `make test`)

- **X1** All unreal controls neutral: output matches Cymbolism for the same settings and seed to within 1e-6 relative, on CRASH and RIDE, soft and hard.
- **X2** Fuzz: 200 random settings of every unreal control, a hard hit each, 3 s. No NaN, zero guard events, peak under 10.5 V.
- **X3** Level: a medium hit peaks between 2 and 6 V for WARP at -1, 0, 0.5, 1, 2.5 and TUNE at 0 and 100%.
- **X4** TUNE 100%, minor pentatonic: at least 90% of the energy between 100 Hz and 8 kHz lies within 20 cents of a scale note.
- **X5** SUSTAIN: energy within ±20% of the target for 20 s, zero guard events; after release it decays. FREEZE: energy within ±5% for 30 s.
- **X6** PUMP: a sine pump at twice the lowest core mode's frequency grows that mode from a seed above a depth threshold and not below it.
- **X7** WIDTH 0 gives L equal to R. PICKUP at AIR matches the mono module.
- **X8** CPU at HIGH: static settings no more than 1.25x Cymbolism; every unreal CV moving no more than 1.6x.
- **X9** Retune during a contact, a roll and a bow stroke: no out-of-range access (run under AddressSanitizer once).
- **X10** Every new type builds, with `minEigCore` above 0 and build time under 3 s.

---

## 9. Build order

1. The engine groundwork of section 3, in the shared engine, with X1 and X9. Push and confirm Cymbolism is unchanged.
2. WARP, TUNE and V/OCT, TILT, BLOOM and FLOW, level compensation. X2 to X4.
3. SUSTAIN, SWELL, COLOUR, FREEZE. X5.
4. MORPH and TYPE B, LOAD.
5. **First audio to Hardy**: re-render the eight sketches through the real module code and post them. Hardy's reaction may change what follows.
6. Stereo, PICKUP, the extra outputs, the wider ranges, HOLE, materials, new types. X7, X10.
7. Live TENSION, then PUMP (X6), BOUNCE, RAIN. Cut any that fail.
8. Module, 60 HP panel, screen. X8.
9. Manual section with recipes, README row, push, Mac arm64 build green.

Recipes for the manual:

- **Tuned ride**: RIDE, TUNE 100%, CHORD from a chord sequencer.
- **Reverse cymbal**: a gate into HOLD, SWELL to taste, CHOKE on the downbeat.
- **Acid bell**: BELL or PLATE, SCALE HARMONIC, V/OCT from a bassline sequencer, short DECAY.
- **Endless crash**: SUSTAIN up, COLOUR by LFO, BLOOM 1.5.
- **Melting cymbal**: an envelope into MORPH.
- **Mirror metal**: WARP -1, PIVOT swept by hand.
- **Bowed bowl**: BOWL with BOW.
- **Slow-motion crash**: FLOW at minimum, hard hit, GONG.
- **Rain on a sheet**: SHEET with RAIN.
- **Dub siren plate**: PLATE, SUSTAIN on, V/OCT from an LFO.

---

## 10. Ideas parked for later

Not in this build. Listed so they are not lost.

- **Larsen feedback**: the mic signal drives the cymbal at the strike point through a short delay, as a speaker cone does. Howls on single modes chosen by MIC and POSITION.
- **Resonator mode**: wet-only EXCITE with FREEZE and TUNE, so the cymbal becomes a tuned metal reverb for other sounds.
- **Broken symmetry**: a crack, a keyhole or a square cymbal. Needs a solver that is not axisymmetric, which is a large job.
- **Sympathetic partner**: a second cymbal that rings only because the first one does.
- **Mode gating**: rhythmic muting of mode groups by angular order, patterned from a clock.
- **Ensemble**: several cymbals in one module. About 60% of a core each at HIGH, so two or three at ECO.
- **Per-hit geometry sequencing** inside the module, instead of by CV.
