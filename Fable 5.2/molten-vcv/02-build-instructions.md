# Molten: build instructions

Design by Fable, 2026-11-26, for the model that builds it. Hardy named the idea and judged the sound from prototype sketches.

Molten is **Plate Driver with a temperature map**. Hardy rejected a first version (a sparse struck bar that turned into a drop) and asked for the variety of Plate Driver. He then approved sketches made by adding heat to the Plate Driver body: "much better. Excellent job." So the starting point for all code is `PlateDriverFable` on branch `claude/project-thread-due7lx` (`git fetch origin claude/project-thread-due7lx`, then `git show FETCH_HEAD:PlateDriverFable/...`). Copy its files into `MoltenFable` and change them; this time copying is right, because Molten is a sibling built on the same engine. Read `PlateDriverFable/docs/BUILD-INSTRUCTIONS.md` first. Everything there about the plate, the stretching, the loop, the leveller, the decimator and the safety applies here unless this document says otherwise.

The **reference engine** is `tools/proto/molten_ref.c` with `scenes.h` (plain C: `gcc -O3 -ffast-math -o molten molten_ref.c -lm`, then `./molten heat`, `cast`, `flame`, `pushed`; `post.py out <names>` makes WAVs and a spectrogram). It is Plate Driver's first reference (`sing.c`) plus the thermal code. The C defines the sound of everything thermal. Where prose here is ambiguous, the C decides.

Hardy's taste, from this and the sibling threads: one continuous body that evolves by itself; harsh is fine; show where it breaks up; ranges run past what real metal does and are not to be tamed; sparse struck notes with plain pitch glides bored him; he did not enjoy dry collision sounds, so the pings stay small and can be turned off.

Things marked **UNTESTED** were not in the prototype. Build them last, test them hard, and say in the README what you changed.

## 1. What to build

- Plugin folder `MoltenFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-oz0x23` (not main). The workflow builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `MoltenFable`, name `Molten Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `Molten`, name `Molten`, tags Synth voice, Physical modeling, Drone, Effect. Description: "A sheet of metal held singing by a pickup and a driver while you heat it. Heat loosens it into chords and roar, sinks its partials, melts it into a dense seething liquid, and every quench casts a new tuning."
- Panel 44 HP, text labels, the Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 20 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo audio rendered by the C++ engine into `/mnt/project-files/molten-vcv/demos/` (MP3 is fine).
- Put the DSP in `namespace molten` (Rack has an `Engine` class; Plate Driver hit that clash).

## 2. The idea in one paragraph

The body is Plate Driver's: 192 resonators, six stretch channels that let them feed each other, and a pickup, focus filter, amplifier and driver holding it in a feedback loop. On top is a strip of 16 thermal zones. A flame heats the zones, heat spreads between them, and each zone must take in extra heat at the melting point before it turns liquid. Each resonator knows how much of itself lives in each zone. Heat does four things. It **sinks** each resonator's pitch and shortens its ring by the temperature of its zones. It **loosens** the metal, so the stretching acts as if the motion were larger and the loop splits and roars at a drive where cold metal sings one tone. It **melts**: a molten zone swaps plate stiffness for surface tension, so the same resonators slide into a dense, low, very loose cluster. And it **casts**: at the moment the liquid sets, the shape it holds becomes a new tuning of the solid, so every melt and quench gives a different bell.

Measured on the reference (f1 65.4 Hz, focus at 196 Hz, drive 2500, sketch `heat`):

- Cold: one tone at 252 Hz, participation ratio (PR) 1. Red-hot (T 0.4 to 0.9): PR 3 to 7 with the sung tone sinking from 252 to 174 Hz. Molten: PR 3 to 10, tones between 79 and 242 Hz. After the quench it settles on a new tone (130 Hz in that run).
- `cast`: four melts and quenches from the same body set on 52/130, 108, 225 and 131 Hz as the loudest tone, each with a different chord round it. While molten under drive 5000 the PR is about 30.
- No guard events in the four scenes. With the drive off the energy decays to nothing.

## 3. Engine (`src/dsp/MoltenEngine.hpp`, from `PlateEngine.hpp`, no Rack code)

Keep from `PlateEngine.hpp`: rates and block size, exact two-pole resonators in double precision, the SHEET mode table and CAST-0 random sequence, pickup, focus filter, amplifier stage, strike, output pickups, OUT clip, the energy ceiling and guard, the meters, the decimator, the leveller. **Remove**: the other plate types, the delay (PHASE), pump, driver saturation, rattle, bleed, STRETCH/Sbar (use `S + nu dS/dt` only, as the reference does). SPREAD is removed: pickup and driver are collocated.

### 3.1 Thermal map (reference `thermal()`, run once per block, `dt = BLK / fe`)

`K = 16` zones, `x_k = (k + 0.5)/K`. State per zone: enthalpy `h_k`, temperature `T_k`, liquid fraction `phi_k`, a liquid flag and a front timer. Temperature units: 0 cold, 1 melting point, 2 white-hot.

```
g_k  = exp(-((x_k - xf)/wf)^2)
dh_k = (heat > T_k ? rate * g_k * (heat - T_k) : 0.6 * rate * (heat - T_k))
     - air * T_k + cond * (T_{k-1} - 2 T_k + T_{k+1})            (ends mirror)
     - quench * 2.5 * (0.5 + 1.5 |2 x_k - 1|) * T_k
h_k += dh_k dt, floored at 0
```

Phase: `h >= 1 + latent` sets the liquid flag. A flagged zone with `h < 1 + latent` is supercooled: it stays liquid (`T = h - latent`, `phi = 1`) until `T < 1 - uc` (it freezes by itself), or a non-liquid neighbour is present, in which case it freezes after a delay of `0.004 + 0.03 max(0, 1 - 3 (1 - T))` seconds (the front moves faster the deeper the undercooling), or a strike with `strikeV > 250` lands in it. Unflagged zones: `h < 1`: `T = h, phi = 0`; otherwise `T = 1, phi = (h - 1)/latent`.

Aggregates: `M` = mean `phi`; `Tsol` = mean `T` weighted by `1 - phi` (1 if nothing is solid); `Tliq` = mean `T` weighted by `phi` (1 if nothing is liquid).

**Pings.** Per zone per block, with probability `ping * (0.6 |dT_k| (1 - phi_k) + 2.0 max(0, -dphi_k))`, a small impulse at a random point `x` in the zone: `q_j[n-1] -= amp * shape_j(x) * T * f^2/(f^2 + 1500^2) * exp(-f/6000)`, `amp = +-(60 + 250 r^2)`. Scale the probability by `dt` relative to the reference block (it was tuned at 32 samples at 96 kHz) so other rates give the same density.

### 3.2 What heat does to each mode (reference `coefs()`, every block)

Zone weights, built with the mode table: `zw[j][k]` = mean of `shape_j(x)^2` over 8 points in zone k, plus 0.1, normalised to sum 1 over k. Per block: `Tb_j = sum_k zw[j][k] min(T_k, 1)`, `pb_j = sum_k zw[j][k] phi_k`.

```
sig  = max(0.2, 1 - 0.5 (Tliq - 1))          visc = exp(-2.2 (Tliq - 1))
fs   = f0_j * tune * cast_j                   (solid)
fl   = f1 * tune * tens * (f0_j/f1)^0.75 * sqrt(sig)      (liquid: surface tension instead of stiffness)
f    = sqrt((1 - pb)(1 - sag Tb^2) fs^2 + pb fl^2),  at least 4 Hz, muted above 0.25 fe as in Plate Driver
al_s = 6.908 / (decay * 9 / (1 + fs/700)) * (1 + 2 Tb^3)
al_l = 6.908 / (decay * 4) * sqrt(fl / (f1 tune tens)) * visc
al   = ((1 - pb) al_s + pb al_l) * (1 + 8 pb (1 - pb)) * (1 + damp (8 + f/40))
```

`cast_j` slews toward its target `castT_j` by 0.02 per block.

**Retuning and energy.** Plate Driver rescales `q` when a mode is retuned so its energy is kept. Do that only for the part of the change that comes from `tune` (pitch). Changes that come from heat, melting and casting must **not** rescale: the reference keeps the displacement, so a mode whose pitch rises as it freezes gains energy, which is the bloom on a quench. Track the two factors separately.

### 3.3 Looseness (reference: `lam`)

```
lam^2 = exp(loose * min(Tsol, 1) * (1 - M)) * (1 + liqnl * M)          curveEff = curve * (1 + 0.5 M)
```

The stretching is evaluated on `qs_j = lam q_j` and the resulting force is divided by `lam`. That leaves the linear part of the stretching unchanged and multiplies the quadratic part by `lam` and the cubic part by `lam^2`. Do **not** implement looseness by multiplying `bloom`: the curve term's linear stiffness then exceeds what the explicit step can follow and it blows up (tried). In Plate Driver's code this means: use `lam q_j` wherever `q_j` enters `s`, `s2`, `gq` and the `a` of `grad2`, use `curveEff` for `curve`, and multiply the final `fn` by `kscale / lam`. The stiffness the stretching adds is the same in either coordinate, so `smax`, `kroom` and `kscale` are used as they are.

A first version limited each mode's force separately. That is not a gradient force: it pumped energy and left the body ringing for ever with the drive off. Use the global `kscale` only.

### 3.4 Casting

In `thermal()`, when `M` falls through 0.5:

```
rms = sqrt(mean_j (w_j q_j)^2) + 1e-9
castT_j = 1 + 0.14 * cast * tanh(1.5 w_j q_j / rms) * (0.4 + 0.6 rnd)
```

So the tuning depends on what the liquid was doing at that instant. With CAST 0 the tuning returns to the original. The module stores `castT` in the patch (192 numbers) and offers "Forget casting" in the context menu, which sets all targets to 1.

### 3.5 Rest state

Add, with the drive at 0: if kinetic energy is below 0.001 of total energy, pull `q` and `q[n-1]` toward zero by `2e-4` of their mean per sample (reference). It lets a bent-in rest shape relax so the idle detection can fire.

### 3.6 Self-heating. UNTESTED.

`dh_k += 1.5 * SELF^2 * tanh(E / 3e6)` for every zone, E the total mode energy. Hard playing then heats the body by itself, loosens it, and at high settings melts it. Check that at SELF 1 a held roar reaches the melting point within 5 to 20 s and that the fuzz test stays clean. If it only ever ends in a stuck molten roar, scale it down and say so.

### 3.7 Cost

The reference is unoptimised (it rebuilds products per sample) and costs about 36% of a cloud core. Plate Driver's per-block tables (`P`, `Lw`, `Gs`) bring that down; the zone sums are 192 x 16 per block. Use float where Plate Driver does. Report the figure.

## 4. Controls

Every knob with a jack has a CV input summed at 0.1 per volt unless stated. 44 HP, 11 columns.

| Section | Control | Range and default | Maps to |
|---|---|---|---|
| HEAT | HEAT (CV, large knob) | 0 to 1, 0 | `heat = 2 HEAT`. Mark 0.5 on the panel as the melting point. |
| | MASS | 0 to 1, 0.3 | `rate = 12 * 0.0125^MASS` per second |
| | FLAME (CV) | 0 to 1, 0.3 | `xf = 0.05 + 0.9 FLAME` |
| | WIDTH (CV) | 0 to 1, 1 | `wf = 0.08 * 25^WIDTH`, `cond = 0.6 + 3.4 WIDTH^2`, `air = 0.9 (1 - WIDTH)^2` |
| | QUENCH (button, gate in, knob 0 to 1, 0.4) | | while held: `quench` = knob, and the flame is off (`heat = 0`) |
| | LATENT | 0 to 1, 0.45 | `latent = 0.1 + 0.9 LATENT` |
| | SUPERCOOL | 0 to 1, 0 | `uc = 0.6 SUPERCOOL` |
| | SELF | 0 to 1, 0 | section 3.6 |
| METAL | TENSION (CV) | -2 to +1 octaves, 0 | `tens = 2^TENSION` |
| | SAG | 0 to 1, 0.67 | `sag = 0.9 SAG` |
| | LOOSE (CV) | 0 to 1, 0.625 | `loose = 8 LOOSE`, `liqnl = 4 * 10^(3.2 LOOSE)` |
| | CAST | 0 to 1, 0.5 | `cast = 2 CAST` |
| | PINGS | 0 to 1, 0.3 | `ping` |
| | BODY | 0 to 15, 0 | Plate Driver's CAST seed (renamed, because CAST means something else here) |
| | SIZE (CV), THICK (CV), CURVE, DECAY | as Plate Driver | as Plate Driver |
| LOOP | DRIVE (CV, large), GAIN, FOCUS (CV), PITCH + V/OCT, TRACK switch | as Plate Driver | as Plate Driver, including drive scaling with pitch |
| TOUCH | POS (CV), GRAB (CV), RELEASE | as Plate Driver's DRIVER, GRAB, RELEASE | pickup and driver both at POS |
| SAT | PICKUP, AMP, OUT | 0 to 1, defaults 0.2, 0.2, 0 | as Plate Driver. The section is called SAT here because HEAT now means temperature. |
| EXTRA | MALLET, IN LEVEL, BRIGHT, AGE, LEVEL | as Plate Driver | as Plate Driver |

Note what tracking means here: PITCH sets the cold, uncast body. Heat sinks the body under the focus filter, and a casting retunes it. With FOCUS high the loop keeps singing near the commanded pitch and hops between the body's tones as they slide past. With FOCUS low the song sinks with the body. Both are wanted. Say so in the manual.

**Jacks.** Inputs: V/OCT, GATE, STRIKE, VEL, QUENCH, IN, HEAT, FLAME, WIDTH, TENSION, LOOSE, DRIVE, FOCUS, SIZE, THICK, POS, GRAB. Outputs: L, R, TEMP (10 V times mean zone temperature / 2), LIQUID (10 V times M), ROAR, ENV. GATE, VEL, mono handling as Plate Driver. A strike with velocity above 0.6 seeds freezing in a supercooled zone (the reference threshold `strikeV > 250` at `v = 420 * velocity`).

**Display.** A strip of 16 cells across the panel showing each zone: black when cold, through dull red, orange and yellow to white at T = 2, with a blue-white edge on cells that are liquid, and a small marker for the flame position. Draw it on layer 1 so it glows in a dark room. This display matters: the lag between the knob and the body is the heart of playing the module.

## 5. AGE (UNTESTED as a whole)

Keep Plate Driver's parts except the loose head: loop gain drift, hiss, hum, dents, warm-up, tired amplifier. Add:

- Flickering flame: `heat *= 1 + 0.08 AGE * n(t)`, `n` noise low-passed at 3 Hz.
- Uneven metal: each zone's melting point offset by a fixed random amount up to +-0.12 AGE (apply as an offset to that zone's `h` thresholds), and its heating rate scaled by 1 +- 0.3 AGE.
- Slag: each zone adds loss to the modes living in it, `al *= 1 + 2 AGE sum_k zw[j][k] r_k^2`, `r_k` fixed random in 0..1.

## 6. Tests (`make test`, no Rack needed; every check prints PASS or FAIL)

1. **Reference match.** BODY 0, AGE 0, leveller off, controls set as in `scenes.h`: the C++ engine at 96 kHz reproduces `heat`, `cast`, `flame`, `pushed`. Chaotic passages and random pings cannot match sample for sample: compare third-octave band levels over 2 s windows, within 3 dB where the reference is within 40 dB of its peak, and compare the logged `M`, `Tsol` and energy curves (thermal curves within 0.02; they do not depend on the audio except through casting). The first 5 s of `heat` (cold, pure tone) must match to 0.1 dB.
2. **Cold equals Plate Driver.** HEAT 0, CAST 0, PINGS 0: the three-regimes and tuning tests of Plate Driver (its tests 2 and 5) pass unchanged.
3. **Heat loosens.** Drive fixed where the cold body has PR below 1.2: PR above 2.5 somewhere between HEAT 0.2 and 0.45 after 10 s at each setting.
4. **Sag.** Light drive, FOCUS 0: the sung pitch at HEAT 0.45 is 5 to 9 semitones below cold at SAG 0.67, and within 30 cents of cold at SAG 0.
5. **Melting and latent heat.** HEAT stepped from 0 to 1: LIQUID out rises from 0 to 10 V; the time spent between 5% and 95% liquid at LATENT 1 is at least 3 times that at LATENT 0.
6. **Narrow flame.** WIDTH 0.2, HEAT 1 held for 20 s: LIQUID settles between 20% and 70% and never reaches 100%; moving FLAME from 0.1 to 0.9 changes which zones are liquid.
7. **Casting.** Ten melt, drive and quench cycles at CAST 0.5: the loudest partial after each lands on at least 5 different modes; at CAST 0 it lands on the original each time. "Forget casting" restores the original tuning.
8. **Supercooling.** SUPERCOOL 1, HEAT dropped from 1 to 0.4: still fully liquid after 5 s; one strike at velocity 1 freezes it within 0.5 s. At SUPERCOOL 0 the same drop freezes without a strike.
9. **Energy.** Drive 0 after a strike, at HEAT 0, 0.45, 0.6 and 1 (held steady): energy never rises (check every 10 ms). After hard drive in every one of those states, with the drive cut, the output falls below -80 dB of its level within 30 s.
10. **Quench bloom.** Quenching a ringing molten body with the drive off raises the output level by at least 3 dB during the freeze.
11. **Fuzz.** 400 random patches over every control at full range, random CV steps every 0.5 s including HEAT and QUENCH, strikes and noise into IN, 20 s each: no NaN, no guard event, output within +-11 V. Report how often the energy ceiling clamps.
12. **Sample rates.** Tests 2, 3, 5 and 11 (100 patches) at 44.1, 48 and 96 kHz.
13. **Silence.** DRIVE 0, no input, any HEAT: output exactly zero after 60 s (AGE hum excepted).
14. **Speed.** Report the engine's share of one core at 48 kHz, cold and molten.

## 7. Panel (44 HP = 223.52 mm; render a preview PNG and look at it; fix collisions)

Eleven columns. The zone display runs across the top under the title. HEAT and DRIVE are the two large knobs, HEAT on the left with the melting point marked at noon, DRIVE with Plate Driver's ROAR ring light. The QUENCH button sits beside HEAT and is large. Captions over each section. Outputs on a darker plate. Suggested rows: row 1 HEAT section then METAL; row 2 LOOP and TOUCH; row 3 the rest of METAL (BODY, SIZE, THICK, CURVE, DECAY), SAT and EXTRA; rows 4 and 5 jacks.

## 8. Presets (20 .vcvm files, numbered; each sounds with nothing patched unless noted)

1. Init (cold pure tone; raise HEAT and it splits, raise it past noon and it melts)
2. Red Heat (HEAT 0.38: the wash of sketch `heat` at 10 to 17 s)
3. Edge of Melting (HEAT 0.5, LATENT 1: sits half liquid)
4. Seething (HEAT 0.9, DRIVE up: molten roar)
5. White Hot Drone (HEAT 1, SIZE 30 Hz, low DRIVE)
6. Foundry (notes: gate QUENCH from a slow clock while HEAT is high; each quench casts a new bell)
7. Flash Freeze (SUPERCOOL 1, HEAT 0.4 after melting; notes: strike to set it)
8. Walking Flame (WIDTH 0.2; notes: slow LFO to FLAME)
9. Spot Weld (WIDTH 0, HEAT 1, FLAME 0.7: one molten patch)
10. Slow Furnace (MASS 1: a minute to melt)
11. Runaway (SELF 0.8, DRIVE high: it melts itself)
12. Cold Bell (HEAT 0, DRIVE 0, struck; notes: melt and quench to recast it)
13. Annealed (HEAT 0.3, DECAY low, soft MALLET: dull warm strikes)
14. Mercury (TENSION -2, HEAT 0.8: very low liquid)
15. Light Metal (TENSION +1, HEAT 0.7, BRIGHT 1)
16. Molten Lead (TRACK FOCUS, HEAT 0.75; play V/OCT)
17. Scream (HEAT 0.45, PICKUP 1, AMP 0.75, OUT 0.45)
18. Meltdown (the `pushed` sketch: notes say to put a fast LFO on HEAT)
19. Stiff (LOOSE 0, SAG 0: heat only melts)
20. Resonator (DRIVE low, IN LEVEL 0.8, HEAT 0.6; needs audio in)

Check each by rendering it. If a preset is silent or stuck, change it.

## 9. Manual

MANUAL.md and a PDF made with Plate Driver's tooling. Plain language, full sentences. Contents: what it is (Plate Driver's singing sheet, now with a flame under it); the four things heat does; the zone display and why the body lags the knob; quench, supercooling and casting; a panel tour; each control in terms of what you hear; pitch and what tracking means when the body sags; AGE; the presets; at least ten patch recipes, including: a slow LFO on HEAT across the melting point; an envelope on HEAT per note; QUENCH from a clock for a new bell every bar; FLAME from a sequencer; TEMP out into FOCUS or a filter; LIQUID out into a reverb send; SELF with a gate so loud notes melt; a kick into IN on a molten body; two Moltens cast differently for stereo; saving a casting you like (it is stored in the patch) and "Forget casting". State which parts are physical (heat flow, latent heat, supercooling, the plate and its stretching, loss of stiffness with temperature) and which are invented (the size of every effect, casting from the frozen shape, looseness as a law, positions along one line).

## 10. Order of work

1. Copy Plate Driver's engine, strip what section 3 removes, add the thermal map, the heat laws, looseness and casting. Get tests 1 to 10 green.
2. Module, panel with the zone display, presets.
3. The untested parts behind the fuzz test: SELF, AGE, audio in on a molten body, TRACK modes with heat.
4. Push, confirm the Mac arm64 build is green and the plugin is on the preview release.
5. Manual, PDF, README (list every deviation from this document), demos from the C++ engine: the four scenes and one render per preset.
