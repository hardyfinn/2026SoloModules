# Live Wire: build instructions

Design by Fable, 2026-11-26, for the model that builds it. "Live Wire" is a working name Hardy has not objected to. Hardy's brief was "the sound of electricity"; they have heard arcing and find it "very raw and alive".

The **reference engine** is `tools/proto/livewire_ref.c` with `mapping.h` and `scenes.h` (plain C, no dependencies: `gcc -O2 -o livewire livewire_ref.c -lm`, then `./livewire arc out.f32`, also `sing`, `kit`, `kit2`, `sub`, and `./livewire fuzz <seed> out.f32`; `post.py out <raw:name:ratio>...` writes WAVs and a spectrogram; `SR=96000` changes the host rate; `DBG=1` prints strike counts; `SOLO=<n>` solos one instance of the kit). Copy them from `/mnt/project-files/electricity-vcv/reference/`. **The C is the definition of the sound. Where prose here is ambiguous, the C decides.** Port it literally first, optimise second.

Hardy's verdict on the sketches (`/mnt/project-files/electricity-vcv/sketches/`): "Raw Arc and Substation are the most alive. I like those. Spark Kit could do with having a little more of that erratic nature." So the unstable, sputtering, self-interrupting behaviour is the point of the module. `kit2` is the answer to the last sentence (section 3.6) and Hardy has not heard it yet. Every range deliberately runs past what a real circuit does. Do not tame the top of any range, and do not smooth away dropouts, crackle or jitter.

Standing preference from project memory: Hardy likes pushing models past physical limits, and likes an AGE knob.

Things marked **UNTESTED** were implemented in the reference and survive the fuzz, but nobody has listened to them. Check them with care and say in the README what you changed.

## 1. What to build

- Plugin folder `LiveWireFable` in hardyfinn/2026SoloModules, on branch `claude/project-thread-ebi3d9` (already checked out at `/home/user/2026solomodules`; not main). `BentSheetMetalFable` on branch `claude/project-thread-wsdoxv` is the template for the Makefile, plugin.json, panel script, Haer logo, offline test harness (`test/bridge.h`, `quick.cpp`, `render.cpp`), `Mapping.hpp` pattern, preset generator, PDF tooling and README: `git fetch origin claude/project-thread-wsdoxv`, then `git show FETCH_HEAD:BentSheetMetalFable/...`. Do not copy that folder into this branch. The workflow `.github/workflows/build.yml` builds every folder with a plugin.json for Apple silicon and publishes to the "preview" release. Apple silicon only.
- plugin.json: slug `LiveWireFable`, name `Live Wire Fable`, version `2.0.0`, brand `Haer`, licence GPL-3.0-or-later. One module: slug `LiveWire`, name `Live Wire`, tags Synth voice, Physical modeling, Noise, Drum, Drone, Effect. Description: "One live electrical circuit: a source, a spark gap and a tuned coil. Buzzing arcs that sputter and tear, a Jacob's ladder, a singing arc that tracks V/oct, sparks as drums, corona sizzle and transformer hum. Every range pushed past real."
- Panel 40 HP, text labels, the Haer logo as on the other Fable modules.
- Deliverables: C++ module, panel SVG from a script, 22 factory presets, offline tests (`make test`), MANUAL.md, a PDF manual with patch recipes, README, demo audio rendered by the C++ engine, the reference C in `tools/proto/`, this file in `docs/BUILD-INSTRUCTIONS.md`.
- Rack's namespace has an `Engine` class. Put the DSP in `namespace livewire` and qualify it.
- Mono. Read channel 0 of every input.

## 2. The idea in one paragraph

One circuit. A source (DC, mains AC, or a blend) feeds a spark gap through a ballast resistor and a choke. Across the gap sit three things. (1) The arc: a conductance that grows when it is fed more power than it loses and shrinks otherwise (Mayr's arc equation with Schwarz exponents), with a breakdown voltage that depends on gap length, a memory of recent ionisation that makes re-striking easier, wind that stretches it, and a climb that lengthens it while it burns. (2) A series coil and capacitor: fire a spark and it rings; hold a DC arc and the arc's negative resistance makes it oscillate at that pitch (Duddell's Singing Arc, 1899). The coil saturates, so pitch falls as a ring decays. (3) Corona: random micro-discharges once the voltage nears breakdown. The sound is not any voltage directly: ARC is the time derivative of the power in the arc (how an arc heats air), FIELD is the time derivative of the wire currents (what a pickup coil hears), HUM is a transformer case shaken by flux squared and current squared.

Regimes, all from the same equations:

- AC, gap short enough to strike: buzzing arc that goes out and re-strikes every half cycle. Longer gap or less MEMORY: later, harder re-strikes, then sputter and dropouts.
- CLIMB: the arc lengthens until the source cannot hold it, tears, and re-strikes at the bottom.
- DC, moderate CURRENT, TRACK on: the arc sings at TUNE. High current: silent steady arc. Lower: sings. Starved: current hits zero each cycle and it tears (harsh).
- DC, tiny CURRENT, VOLTS below GAP: nothing happens until TRIG fires the gap; the tank capacitor dumps through the coil (a zap). Recharge time is the natural dynamics.
- DC, tiny CURRENT, VOLTS above GAP: relaxation oscillator. Ticks (Geiger) to buzz as CURRENT rises.
- AC below strike with CORONA: sizzle on the voltage peaks; flashovers when a fluctuation reaches the gap.

Measured on the reference (48 kHz):

- All five scenes and 24 fuzz seeds (every knob random each 0.5 s, triggers, surges, audio in): no NaN, zero guard events. Also clean at 44.1, 96 and 192 kHz.
- Singing arc with TRACK on (DC 1.6, gap 0.25, load Z 0.4, RING 28, COOL 63 us): tracks V/oct within about 15 cents from C1 to C6, but sits flat by an amount set by CURRENT: 132 cents flat at current 1.1, 215 at 0.7, 346 at 0.45. So CURRENT is also a pitch bend. Notes build in 20 to 40 ms. FIELD level rises about 6 dB per octave.
- Singing needs `omega * tau` below about 1 at the arc's working conductance. That is why TRACK exists.
- Sparks ring only if HEAT is low (arc voltage far below the capacitor voltage) and the arc does not "power-follow". With HEAT 0.0012, load Z 0.6, RING 14 a 44 Hz kick rings for about 100 ms.
- `kit2`: 39 kick triggers gave 63 strikes (ghost re-strikes), one audible misfire in the bars inspected, onset lags up to tens of ms.
- CPU: the reference renders 27 s in 4.7 s on the build container (one instance, `-O2`). See 3.8.

## 3. Engine (port `livewire_ref.c` to `src/dsp/LiveWireEngine.hpp`, no Rack code)

Port `lw_init` and `lw_process` as they are: same state, same order of operations inside the oversampled loop (the order of the semi-implicit updates matters), the same xorshift RNG and the same number of RNG draws per sample, so the C++ render can be compared with the C render (section 7). The notes below explain; they do not replace the C.

### 3.1 Rates and smoothing

Engine rate = host rate x `OS`, `OS` = 16 up to 50 kHz, 8 up to 100 kHz, 4 above (always 705.6 or 768 kHz). Three signals are decimated with one windowed-sinc FIR (24 x OS taps, Blackman, cutoff 19 kHz or 0.396 x host rate if lower): arc power, source current, tank current. Every knob (after CV is added and the sum clamped to 0..1) is smoothed with a 5 ms one-pole **on the 0..1 value**, then mapped. The reference maps every host sample. You may map every 16 host samples if the smoothing is kept per sample or the result is indistinguishable; say which you did.

### 3.2 Mapping (`mapping.h` is the definition; `Mapping.hpp` must match)

| Knob | Physical | Formula |
|---|---|---|
| VOLTS | E, 0 to 4 (1 unit strikes a gap of 1) | `4k` |
| MAINS | DC to AC blend | `Edc = E(1-k)`, `Eac = E k` |
| FREQ | mains 20 to 500 Hz | exp; 50 Hz at k = 0.2847 |
| CURRENT | `cur = 1/Rs`, 0.01 to 4 | exp |
| CHOKE | `Ls/Rs`, 50 us to 20 ms | exp |
| GAP | 0.01 to 4.01 | `0.01 + 4k^2` |
| HEAT | `P0`, 2e-4 to 0.6 | exp |
| COOL | `tau`, 8 us to 3 ms | exp; with TRACK on it is multiplied by `261.6256/ft` |
| QUENCH | `goff`, 2e-3 to 0.5 | exp |
| MEMORY | 1 ms to 200 ms | exp |
| RESTRIKE | 0 to 0.95 | linear |
| JITTER | 0 to 0.6 | linear |
| WIND | 0 to 0.15 | `0.15k^2` |
| CLIMB | 0 to 40 gap units/s | `40k^3` |
| TUNE | C0 to C10, clamped 16 Hz to min(18 kHz, 0.4 x host rate) | `261.6256 * 2^(10k-4)`, plus V/OCT |
| RING | tank Q 0.7 to 60 | exp |
| LOAD | tank impedance Z 8 down to 0.15 | exp (more LOAD = lower Z = bigger currents) |
| IRON | 0 to 3 | `3k^2` |
| CORONA | 0 to 3 | linear |
| HUM | 0 to 0.3 | linear |
| FEEDBACK | 0 to 1.6 | linear |
| DISTANCE | feedback delay 0.3 to 20 ms | exp |
| AGE | 0 to 1 | linear |
| ARC | mix level 0 to 8 | `8k^3` |
| FIELD | mix level 0 to 2 | `2k^2` |
| OUT | gain 0 to 4 | `4k^2` (k = 0.5 is 1.0) |
| DRAW | 0.05 to 10 s | exp |
| IN | audio-in gain 0 to 2 | `2k^2` |

The scenes are written with the inverse maps and warn if any value leaves a knob's range. None does.

### 3.3 The circuit (per engine sample, in this order)

```
source   ev = Edc + Eac*(sin - 0.25*sat*flux^9 + AGE harmonics) + feedback + IN*audio
corona   (3.5)
is      = (is + dt/Ls*(ev - vg)) / (1 + dt*Rtot/Ls)                 source current, implicit in R
it     += dt/Lt * min(1 + iron*it^2, ironMax) * (vg - vct - Rt*it)   tank current, saturating coil
vct    += dt/Ct * it                                                 tank capacitor
vg      = (vg + dt/Cp*(is - it)) / (1 + dt*(g + gp)/Cp)             gap node, implicit in the arc conductance
```

`Lt = Z/(2 pi ft)`, `Ct = 1/(2 pi ft Z)`, `Rt = Z/Q`. The stray capacitance `Cp` is chosen so it resonates with `Lt || Ls` at 17 kHz (`17000 (1 - 0.45 AGE)`), lightly damped by `gp`. That resonance is the sizzle on every strike; keep it. `ironMax = min(16, (0.5/(2 pi ft dt))^2)`: the fuzz found a blow-up without the 16.

### 3.4 The arc

- Length: `lt = (gapEff + climb state) * path * exp(wind*(4*ts + 0.97*tf))`, floor 0.004. `ts` is a 4 Hz and `tf` a 900 Hz unit-variance random process, updated once per host sample. `path` is a per-strike random factor `exp(0.6*jit*n)`, redrawn at every extinction (the arc root lands somewhere new).
- Breakdown: when the arc is out and `|vg| > Vb`, `Vb = lt*(1 + jit*jz)*(1 - RESTRIKE*h)`. `jz` is redrawn at every extinction. On breakdown `g` jumps to 0.4 to 0.8.
- Burning: `g *= exp(dt/tg * (pw/(Pw*sqrt(g)) - 1))`, `tg = tau*sqrt(g)`, `Pw = P0*lt`, `pw = vg^2 g`, exponent clamped to +-0.5, `g` at most 400. These exponents make thin arcs cool faster and are why it goes out at current zeros.
- Extinction: `g < goff`. QUENCH is real hardware (blown or quenched gaps) and is what stops sparks turning into silent DC arcs.
- Ionisation memory `h`: rises to 1 in 2 ms while burning, decays with MEMORY when out.
- Climb: `c += CLIMB*h*dt` while `h > 0.08`, otherwise collapses in 4 ms; capped at 12.
- `jit = JITTER + 0.3*AGE` (max 0.9).

### 3.5 Corona

When `|vg|` comes within a few tenths of `0.55*lt`, micro-discharges fire at a rate that grows exponentially with voltage: 9000/s scale and small on the negative half, 2500/s and four times bigger on the positive half (this asymmetry is real and gives the mains-rate rasp). Each one removes charge from the gap node, which kicks the stray resonance, and adds a click to the ARC output at a random pan position.

### 3.6 Triggers, key, touch, surge

- **TRIG** (triggered gap): if the arc is out, a trigger starts a strike regardless of voltage. With JITTER it becomes erratic, which is what Hardy asked for: (a) misfire probability `1.2*jit*(1 - r)`, `r = |vg|/Vb_cold` capped at 1, so a half-charged capacitor misfires more; a misfire just leaks 15% of the node charge; (b) a statistical lag, exponentially distributed, mean `jit * 8 ms * (1.5 - r)`; (c) while the lag is pending, leader ticks at `900*jit` per second, each dumping 12% of the node voltage (a "t-t-TZAK"); (d) `path` and `jz` redrawn per strike, so every hit rings differently; (e) ghost re-strikes: if VOLTS is close to `GAP*(1 - RESTRIKE)`, the hot gap re-fires by itself as the capacitor recharges, some of the time. With JITTER at 0 a trigger is sample-tight and always fires.
- **KEY** (only when patched): gate low drops the tank Q to 1.2 (note off). A rising edge zeroes the tank capacitor voltage and current: Duddell's keyboard switched in a different, discharged capacitor per key, and that step is what starts the note quickly. Unpatched, the tank is always in.
- **TOUCH** (gate and momentary button): while high the electrodes close to 0.012 (15 ms). On release they pull apart at a steady rate that would cross the full GAP range in DRAW seconds, stopping at the GAP knob. This draws an arc at a gap far too long to strike by itself, which is the opening gesture of the "Raw Arc" sketch.
- **SURGE** (trigger and button): VOLTS x3 and CURRENT x2, decaying with a 60 ms time constant. A flashover on demand.

### 3.7 Outputs

```
ARC   = d(arc power)/dt * sr/700 + 0.1*highpass25(arc power)        panned by a slow random wander
FIELD = d(is + 0.6 it)/dt * sr/2000 + 0.15*highpass30(is + 0.6 it)   centre
HUM   = six resonators (2f, 4f, 6f of the mains, and 417, 733, 1180 Hz) driven by flux^2 + 0.4 is^2, plus a rattle when the drive exceeds a threshold; different gains left and right
mix L/R = ARCknob*(arc*pan + corona clicks) + FIELDknob*field + HUMknob*hum
feedback: mono mix -> delay DISTANCE -> one-pole -> FEEDBACK*tanh(0.5 x) -> added to the source voltage
out = 1.6*tanh(0.35*OUT*mix/1.6), 20 Hz high-pass, x5 V
```

Separate ARC and FIELD outputs are post-decimation, pre-mix, fixed gain `1.6*tanh(0.35 x/1.6)`, x5 V. The feedback path depends on the ARC/FIELD/HUM knobs but not on OUT. The reference `LW` struct returns values before the x5.

The arc output of a spark with low HEAT is weak (little power in the arc), so drum presets take body from FIELD and click from ARC with a high ARC knob. Low notes are quiet on FIELD (it is a derivative). Level every preset with ARC, FIELD and OUT so loud moments peak between 5 and 8 V.

### 3.8 AGE (all UNTESTED by ear)

One knob, all in the C: more jitter (+0.3), wind (+0.01), a loose connection (random dropouts where series resistance jumps 8 to 38 times for 0.2 ms to several ms, rate `12*AGE^2` per second), dirty mains (3rd and 5th harmonics, 5% slow amplitude flicker, 0.4% frequency drift), tank drift (0.6%) and half the Q, stray resonance down to 9.4 kHz, more transformer saturation, and a lower rattle threshold. If any part of it sounds like a fault of the module instead of a fault of the circuit, reduce that part and note it.

### 3.9 CPU

If one instance costs more than about 12% of a core in Rack's meter on Apple silicon at 48 kHz, optimise without changing behaviour: `-O3`, a rotating phasor instead of `sin`/`cos` per engine sample, per-block mapping, skipping the corona branch when CORONA is 0, a polyphase decimator. Do not lower OS. Report the measured figure in the README.

## 4. Panel (40 HP)

Sections left to right: SOURCE, GAP, AIR, TANK, EXTRAS, OUT. Big knobs for VOLTS, CURRENT, GAP, TUNE. Text labels on everything.

**Knobs (28):** VOLTS, MAINS, FREQ, CURRENT, CHOKE | GAP, HEAT, COOL, QUENCH, MEMORY, RESTRIKE, JITTER, DRAW | WIND, CLIMB | TUNE, RING, LOAD, IRON | CORONA, HUM, FEEDBACK, DISTANCE, AGE, IN | ARC, FIELD, OUT.

**Switches:** TRACK (off/on; COOL follows pitch), V/OCT target (TANK / BOTH / MAINS).

**Buttons (momentary):** FIRE (same as TRIG), TOUCH, SURGE.

**Inputs (20):** V/OCT, TRIG, KEY, TOUCH, SURGE, AUDIO, and CVs for VOLTS, CURRENT, GAP, HEAT, COOL, WIND, CLIMB, RING, LOAD, IRON, CORONA, FEEDBACK, MAINS, AGE. CV scaling: 10 V sweeps the whole knob, added to the knob, clamped to 0..1, no attenuverters (follow the template if it does this differently, and say so). V/OCT is 1 V per octave on top of TUNE and/or FREQ. Audio in: 5 V = 1.0.

**Outputs (4):** L, R, ARC, FIELD.

**Lights:** ARC (ionisation memory `h`, so it glows while the arc burns and fades after), CHARGE (`|vct|` or `|vg|`, whichever is larger, divided by cold strike voltage, so you can see a drum hit recharge).

**Default (Init):** AC 1.7 at 50 Hz, current 1.0, choke 3 ms, gap 0.5, heat 0.35, cool 90 us, quench 3e-3, memory 6 ms, restrike 0.8, jitter 0.08, wind 0.03, climb 0, tune 2500 Hz, ring 4, load Z 6, iron 0, corona 0, hum 0.05, feedback 0, distance 2 ms, age 0, arc 1, field 0.15, out 1, draw 2 s, in 0, TRACK off, V/OCT to TANK. It buzzes as soon as it is placed.

Tooltips show physical units (volts as "x strike", Hz, ms, us).

## 5. Presets (22, values are physical; convert with the inverse maps; unlisted values are the Init ones)

1. **Init**.
2. **Drawn Arc**: gap 1.3, draw 4 s, wind 0.05. Silent until TOUCH; then it pulls out, sputters and breaks.
3. **Jacob's Ladder**: AC 2.0, gap 0.12, current 1.3, choke 5 ms, cool 150 us, memory 12 ms, climb 1.0, wind 0.02.
4. **Runaway Ladder**: AC 2.8, gap 0.12, current 2.4, choke 5 ms, cool 60 us, memory 12 ms, climb 6, wind 0.055, load Z 2.5, ring 9, tune 2000, feedback 0.5, distance 2.3 ms.
5. **Singing Arc**: DC 1.6, current 1.1, choke 12 ms, gap 0.25, load Z 0.4, ring 28, TRACK on, cool 63 us, jitter 0.05, wind 0.004, memory 20 ms, arc 0.8, field 0.45. Patch V/OCT and gate to KEY.
6. **Starved Arc**: as 5, current 0.45, wind 0.015.
7. **Iron Throat**: as 5, DC 1.9, current 0.5, iron 1.5, load Z 0.28, cool 100 us.
8. **Howl**: as 5, DC 2.3, current 0.9, gap 0.6, wind 0.04, iron 0.8, feedback 1.1, distance 3.1 ms.
9. **Spark Kick**: DC 2.5, gap 4, current 0.04, choke 0.2 ms, heat 0.0012, cool 1.2 ms, memory 4 ms, restrike 0.5, quench 0.2, jitter 0.1, wind 0.01, tune 44, load Z 0.6, ring 14, iron 0.22, arc 6, field 1.3. TRIG.
10. **Erratic Kick**: as 9, DC 3.0, jitter 0.3, iron 0.5, memory 60 ms, restrike 0.62.
11. **Spark Snare**: DC 0.6, current 0.04, gap 1.2, choke 0.2 ms, heat 0.3, cool 25 us, quench 0.02, memory 2 ms, restrike 0.92, jitter 0.5, wind 0.07, climb 30, tune 205, load Z 1.0, ring 5, iron 0.3, arc 0.11, field 0.5. Played by a 5 V gate into both the VOLTS and CURRENT CVs (about 90 to 120 ms long): that lifts it to DC 2.6 and current 0.8, it strikes, climbs, tears and crackles out.
12. **Spark Hat**: DC 2.5, gap 4, current 0.02, choke 0.2 ms, heat 0.01, cool 15 us, quench 0.01, memory 4 ms, restrike 0.5, jitter 0.15, tune 8200, load Z 5, ring 7, arc 3, field 0.07. TRIG. (Open hat: tune 5200, ring 25, heat 0.002.)
13. **Erratic Hat**: as 12, jitter 0.45, memory 20 ms, restrike 0.4, corona 0.05.
14. **Relaxation Buzz**: DC 2.6, gap 0.5, current 0.2, choke 0.1 ms, heat 0.03, cool 30 us, quench 0.2, restrike 0.3, tune 700, load Z 1.5, ring 12, arc 0.8, field 0.6. Self-firing; CURRENT is the rate.
15. **Geiger**: as 14, current 0.012, jitter 0.5, tune 3000, ring 3.
16. **Substation**: AC 0.95, gap 1.25, current 1.5, choke 4 ms, cool 140 us, memory 8 ms, jitter 0.2, wind 0.02, load Z 3, ring 10, tune 740, field 0.3, hum 0.14, corona 1.6. SURGE makes a flashover.
17. **Fault**: as 16, AC 2.4, current 2.8, climb 3, wind 0.05, corona 2.2, feedback 0.5, distance 4 ms.
18. **Mains Hum**: AC 0.7, gap 4, hum 0.3, corona 0, field 0.3, age 0.3. No arc at all.
19. **Power Line**: AC 1.0, gap 1.6, corona 2.0, hum 0.04, wind 0.03. Corona only, the odd flashover.
20. **Plasma Speaker** (UNTESTED): DC 1.8, current 1.4, gap 0.3, choke 1 ms, load Z 6, ring 1, in 0.7. The arc speaks the audio input.
21. **Worn Out**: Init with age 0.8.
22. **Tesla**: AC 3.0 at 300 Hz, gap 1.5, current 1.5, tune 1200, ring 20, load Z 1.5, V/OCT target MAINS. A buzzing, pitched discharge played from the mains frequency.

Presets 6 to 8, 10, 13 to 15, 18 to 22 were not rendered in the reference. Render each with the offline harness, check it does what its line says, adjust if it does not, and list the adjustments in the README. For the singing presets (5 to 8) offset TUNE so that 0 V gives C4 within 10 cents on the FIELD output (they sit 1 to 3.5 semitones flat otherwise; see section 2), and say in the manual that CURRENT bends pitch.

## 6. Manual (MANUAL.md and PDF, same tooling as the template)

Plain language, short. What the circuit is (one diagram: source, ballast and choke, gap, tank), what each control does to the sound, the six regimes of section 2 as "how to get", and patch recipes:

1. Strike and draw an arc by hand (TOUCH, DRAW, GAP).
2. Ladder with a slow LFO on CLIMB.
3. Acid line on the singing arc: V/OCT, gate to KEY, envelope or LFO to CURRENT (current is filter-like: high is silent, low is torn), IRON for growl.
4. Spark drum kit from three instances (kick TRIG, snare gate into VOLTS and CURRENT, hat TRIG), then JITTER for the erratic version.
5. Substation drone with SURGE on a slow random trigger.
6. Corona as hi-hat texture gated by a sequencer on VOLTS.
7. Plasma speaker: drums into AUDIO, arc as a raw distortion.
8. Feedback howl tuned with DISTANCE.
9. Mains tuned to the key of the track (FREQ, V/OCT target MAINS).
10. AGE on everything.

Include a line of history (Duddell's Singing Arc) and a safety-free tone: this is a model, nothing here is wiring advice.

## 7. Tests (`make test`) and demos

- `quick`: 8 fuzz seeds x 30 s at 48 kHz plus one each at 44.1, 96 and 192 kHz (same recipe as `main` in the C: all 28 knobs random every 0.5 s, trigger every 0.11 s, surge every 1.7 s, bursts of audio in): no non-finite output, guard count 0, |out| below 1.6 before the x5.
- Port fidelity: render the `arc` and `kit2` scenes through the C++ engine with the same seeds and compare with the C output. They are chaotic, so require agreement to 1e-6 over the first 1 s and matching strike counts within 2% over the whole scene. If you changed the order of RNG draws for speed, drop the sample-exact check and compare band energies per 2 s instead, and say so.
- Every preset: 8 s render with a sensible driver (a touch at 0.5 s for 2; notes and gates for 5 to 8; a 124 bpm trigger pattern for 9, 10, 12, 13; gates for 11; a surge at 4 s for 16), not silent, not clipped flat, no guards.
- Demos, rendered by the C++ engine, MP3, into `/mnt/project-files/electricity-vcv/demos/`: the five scenes and one file per preset. Put a trailing slash on `/mnt/project-files/` paths, do not search that folder recursively, and re-read anything you wrote there after ten seconds.

## 8. Finish

Commit on `claude/project-thread-ebi3d9`, push, check the Mac arm64 workflow is green and the preview release has the plugin. Add the module to the repository README table. Then add one row to Hardy's "Claude creations" tracker sheet (see project memory `claude-creations-tracker`) and update project memory `electricity-vcv-project`. Reply to Hardy with what was built, what was changed from this spec and why, which UNTESTED items you could not vouch for, and start them on the "Raw Arc" and "Substation" demos and then `kit2`.
