# 909 Circuits: DSP review

27 November 2026. Reviewed on branch `claude/project-thread-a8xh2h`: `src/Common909.hpp`, `src/Engines909.hpp`, `src/Voice909.hpp`, `src/VoiceSpec.hpp`, `src/VoiceSpecs.hpp`, `res/voices.py`, `test/voices.cpp` and `README.md`. No file in the repository was changed.

## Summary

The lessons of the 808 review were carried over well. Pitch, level and decay of every voice are the same at 44.1, 48, 96 and 192 kHz (two small exceptions below), the accent law is the same everywhere, DRIVE leaves no DC, a NaN on V/OCT is harmless, and CPU is about half of what the 808 set cost. The five analogue voices have no serious bug. The problems are concentrated in the new part, the Cymbals module:

- **After a crash or ride at its stock DECAY, the ENV output stays at 5.6 V for ever** (10 V if accented), the light stays on and the module never goes to sleep.
- **Crash and ride tails stop dead.** The built-in sound fades *before* the 6-bit converter, so it gets grittier as it fades and cuts to silence at about 1.1 s (crash) and 0.9 s (ride). On the real 909 the fade is done after the converter, which is why its 6 bits sound as good as they do (medium confidence).
- **A loaded WAV is read without any resampling.** A 44.1 kHz file (the usual case) gets strong false tones: 19 dB below a 5 kHz partial, 10 dB below a 9 kHz partial. This is not 6-bit grit; it is still there at 12 bits and at a 96 kHz host rate.
- **The DECAY knob does not show the real decay on the hats.** "700 ms" gives 445 ms and "4 s" gives 0.94 s, because the built-in sound has its own fade underneath.
- **A WAV with an odd header can crash Rack** (a "bits per sample" below 8 divides by zero), and loading a long WAV copies up to 15 MB on the audio thread (up to 9 ms, enough for a dropout).

Smaller points across the other voices:

- **The Rim gets 10 dB quieter from the bottom to the top of V/OCT** (the level compensation only knows about the TUNE knob).
- **The Tom's skin noise gets louder at high sample rates** (+2.6 dB at 96 kHz, +5.1 dB at 192 kHz), the opposite of the 808 bug, and the AGE hiss still gets quieter.
- **The output stage is already bending at DRIVE 0.** Accent adds 4.4 to 4.9 dB of loudness but only 3.0 to 3.7 dB of peak, and a clean sine picks up a third harmonic at -34 dB.
- **Retriggering a ringing Kick or Snare clicks** (steps of up to 2.8 V), because the oscillator jumps back to zero.

Several stock values and structures need checking against the service notes or a recording before they are trusted; they are listed per voice with a confidence level.

**How to read the confidence notes.** Every number in this review is from my own probe programs run against the unmodified engines. Statements about the real TR-909 are from memory of the service notes, Gordon Reid's "Synth Secrets" drum articles and general teardown write-ups; I could not open any of them here. Each such statement carries a confidence level. Nothing marked "medium" or "low" should move an orange dot before someone checks it.

### Measured stock levels (48 kHz, LEVEL 100 %, DRIVE 0)

| Voice | Peak, no accent | Peak, accent | Accent, peak | Accent, loudness | Loudness (RMS, first 200 ms) |
|---|---|---|---|---|---|
| Kick | 4.76 V (4.54 to 5.08 hit to hit) | 7.28 V | +3.7 dB | +4.6 dB | 1.37 V |
| Snare | 6.20 V | 8.70 V | +3.0 dB | +4.5 dB | 0.82 V |
| Tom, mid | 5.13 V | 7.63 V | +3.4 dB | +4.6 dB | 1.29 V |
| Rim | 5.79 V | 8.40 V | +3.2 dB | +4.4 dB | 0.35 V |
| Clap | 6.11 V (4.32 to 6.72 hit to hit) | 8.73 V | +3.1 dB | +4.6 dB | 0.81 V |
| Closed hat | 3.92 V (2.09 to 4.13 hit to hit) | | | | 0.16 V |
| Open hat | 3.96 V | 6.38 V | +4.1 dB | +4.9 dB | 0.49 V |
| Crash | 4.08 V | 6.53 V | +4.1 dB | +4.9 dB | 0.78 V |
| Ride | 4.15 V | 6.62 V | +4.1 dB | +4.9 dB | 0.80 V |

### The same stock hit at four sample rates (loudness, RMS of the first 200 ms, below 20 kHz)

| Voice | 44.1 kHz | 48 kHz | 96 kHz | 192 kHz |
|---|---|---|---|---|
| Kick | 1.369 V | 1.369 V | 1.369 V | 1.369 V |
| Snare | 0.819 V | 0.819 V | 0.834 V | 0.827 V |
| Tom, mid | 1.290 V | 1.294 V | 1.285 V | 1.297 V |
| Rim | 0.357 V | 0.353 V | 0.357 V | 0.357 V |
| Clap (one hit, noise varies) | 0.812 V | 0.813 V | 0.742 V | 0.858 V |
| Open hat | 0.479 V | 0.486 V | 0.521 V | 0.532 V |
| Crash | 0.774 V | 0.780 V | 0.808 V | 0.816 V |
| Ride | 0.794 V | 0.798 V | 0.819 V | 0.825 V |

The kick's pitch per cycle is identical at all four rates (110.5, 79.2, 63.9, 57.4, 54.8 Hz, settling at 52.08 Hz). The cymbals are 0.8 dB brighter at 192 kHz than at 48 kHz (see Cymbals 9).

### CPU cost (measured)

Single thread, Intel Xeon 2.1 GHz, Rack's build flags (`-O3 -march=nehalem -funsafe-math-optimizations`), flush-to-zero on, 48 kHz, one hit every 250 ms. A current desktop CPU is roughly two to three times faster. 96 kHz doubles the share.

| Voice | ns per sample | Share of one core | When silent |
|---|---|---|---|
| Kick | about 90 | 0.4 % | asleep (zero) |
| Snare | about 145 | 0.7 % | asleep |
| Tom | about 107 | 0.5 % | asleep |
| Rim | about 75 | 0.35 % | asleep |
| Clap | about 105 | 0.5 % | asleep |
| Cymbals, closed hat | about 63 | 0.3 % | asleep |
| Cymbals, open hat | about 143 | 0.7 % | asleep |
| Cymbals, crash or ride | about 142 | 0.7 % | **never sleeps: about 85 ns, 0.4 %, for ever** |
| All six | about 650 | 3.1 % | |

The 808 set cost 6.5 % for eight voices on the same machine before its fixes.

---

## All modules (shared code and Rack glue)

1. **[Medium] The output stage is not clean at DRIVE 0, and it squashes the accent.** `Common909.hpp:169-172` (`opamp`), `Engines909.hpp:47-62` (`finish`), voice gains at `Engines909.hpp:90, 152, 214, 277, 334, 417`.
   - Every voice passes through `10.5 * tanh(x / 10.5)` even with DRIVE at zero. Unaccented peaks are already 4 to 6 V and an accented hit asks for 7 to 11 V, so the curve rounds the peaks off.
   - Evidence: accent adds +4.4 to +4.9 dB of loudness but only +3.0 to +3.7 dB of peak on the five analogue voices (table above). With the ACCENT knob at 100 % the law asks for +6.8 dB and delivers +5.9 to +6.2 dB of loudness and +3.6 to +4.8 dB of peak.
   - Evidence: a pure sine at 5.4 V through the Cymbals path comes out with a third harmonic at -33.8 dB. The Kick with SHAPE at 0 (a true sine) has -43 dB of harmonics unaccented and -34 dB accented.
   - Proposed change: give the stage headroom. Either scale the signal down by about 0.6 before the `tanh` and back up after it (so a stock accented hit sits near 5 V inside the curve), or use a curve that is straight up to about 8 V and only bends above that. DRIVE then starts from a clean stage.

2. **[Low-Medium] DRIVE is also a +9 to +14 dB volume knob.** `Common909.hpp:170-171`.
   - The make-up division (`1 + 0.6 * drive`) is much smaller than the gain added (up to 12 times).
   - Evidence, loudness of the first 100 ms from DRIVE 0 to DRIVE 1: Kick 1.87 to 5.44 V (+9.3 dB), Snare +10.0 dB, Tom +9.8 dB, Clap +11.1 dB, open hat 0.67 to 3.30 V (+13.8 dB).
   - Proposed change: stronger make-up, for example divide by `1 + 2.2 * drive`, tuned so loudness rises by no more than about 3 dB across the knob. This is a taste decision; the 808 set behaves the same way.

3. **[Medium] The Tom's skin noise gets louder as the sample rate goes up.** `Engines909.hpp:254`, `Common909.hpp:158-166` (`Pink`), `Common909.hpp:24` (`Rng::noise`).
   - `Rng::noise` scales white noise by `sqrt(fs / 48000)`, which is right for white noise. Pink noise made with fixed filter numbers already holds its level in a given band when the rate changes, so here the scaling is applied once too often.
   - Evidence, Tom noise alone (NOISE 100 %): 0.340 V RMS at 48 kHz, 0.459 V at 96 kHz (+2.6 dB), 0.614 V at 192 kHz (+5.1 dB); at 44.1 kHz it is 0.7 dB lower. With NOISE at 100 % the whole Tom peaks at 6.44, 7.09 and 8.10 V at 48, 96 and 192 kHz. At the stock NOISE (25 %) the effect hides inside the 1 dB test limit.
   - Proposed change: feed the pink filter with plain `rng.bi()` (no rate scaling). Add a test that the Tom at NOISE 100 % is within 1 dB at 48 and 96 kHz.

4. **[Low] The worn-part hiss still gets quieter at high sample rates.** `Common909.hpp:42` (`Age::hiss`).
   - It uses `rng.bi()` without the rate scaling the other noise sources got.
   - Evidence, AGE 100 %, idle, below 20 kHz: 3.55 mV at 48 kHz, 2.52 mV at 96 kHz, 1.79 mV at 192 kHz (3 dB per doubling). With DRIVE at 1 it is 26.6 mV, 7.5 times louder, the same on every voice (consistent, unlike the 808 set).
   - Proposed change: `hiss(age, fs)` with the same `sqrt(fs / 48000)` factor.

5. **[Low-Medium] Retriggering a voice that is still ringing makes a click.** `Common909.hpp:186` (`Vco::reset`), `Engines909.hpp:107-108, 167-168, 233-234`.
   - On each hit the oscillator jumps back to its zero crossing and the VCA jumps to full, so whatever the old tail was doing is cut off in one sample.
   - Evidence (ATTACK 0 so only the body is measured), largest sample-to-sample step: Kick single hit 0.12 V; 16th-note roll at 120 ms 0.61 V; at 60 ms 1.00 V; at 30 ms 2.07 V; with DECAY 2 s up to 2.83 V. Snare shells in a 60 ms roll: 2.28 V. Tom: 0.34 V (fine).
   - The real 909 does restart its kick oscillator on each trigger (medium confidence), so a small tick is authentic, but there it passes through analogue stages with limited speed. A one-sample step is brighter than that.
   - Proposed change: on a retrigger, fade the old output out over about 1 ms while the new hit starts (keep the old value and let it decay with a 0.3 ms time constant, added to the new signal). Costs almost nothing.
   - Related: with SHAPE above 50 % the ramp part starts at -0.5, so the very first sample of every hit is a step (-2.4 V at SHAPE 100 %). Start the ramp part from its own zero crossing.

6. **[Low] Oscillator aliasing is inaudible at stock and only matters at the ramp end of SHAPE played high with DRIVE.** `Common909.hpp:187-207`.
   - Evidence at 48 kHz, false (non-harmonic) content below 20 kHz relative to the whole signal:

     | SHAPE | Pitch | DRIVE 0 | DRIVE 1 |
     |---|---|---|---|
     | 15 % (Kick and Tom stock) | 55 to 873 Hz | -72 dB or better | -88 dB or better |
     | 50 % (triangle) | 218 Hz / 873 Hz | -78 dB / -60 dB | -92 dB / -88 dB |
     | 100 % (ramp) | 55 Hz | -67 dB | -42 dB |
     | 100 % (ramp) | 218 Hz | -56 dB | -34 dB |
     | 100 % (ramp) | 873 Hz | -50 dB | -28 dB |

   - The ramp itself is band-limited, but the output stage sharpens its edge again and nothing band-limits that. At 96 kHz the ramp figures improve by only 3 to 8 dB.
   - Proposed change: none for stock use. If the ramp with DRIVE is meant to be played melodically, run the output stage at twice the sample rate, and round the triangle's corners (polyBLAMP).

7. **[Low] Some per-sample maths is left.** `Engines909.hpp:51, 56` (two `exp` per sample in `finish`, both constants), `Common909.hpp:131, 148` (a `tan` on every filter call, with cut-offs that only change at control rate), `Engines909.hpp:115` (`exp2` per sample in the Kick, needed for the sweep).
   - CPU is already low (table above), so this is tidying. Caching the filter coefficients at the 16-sample control rate would take roughly another third off Snare, Tom and Cymbals.

8. **[Low] A trigger that arrives during the 0.5 ms accent window is dropped, which makes the hats depend on cable order.** `Voice909.hpp:197`.
   - If OPEN arrives one sample before TRIG (for example TRIG went through one extra module), the open hat plays and the closed trigger is ignored, so there is no choke. The other way round, the closed hat plays. When both arrive on the same sample, closed wins.
   - Proposed change: during the window remember that both arrived and decide when the window closes (closed wins, as now for the same-sample case). I do not remember which one wins on the real 909 when both are programmed on a step (low confidence).

9. **Things that checked out.**
   - Sample rate: level within 0.1 dB and identical pitch on Kick, Tom, Rim; Snare within 0.2 dB; see the table.
   - Accent: one law, +4.4 to +4.9 dB loudness at stock on every voice; a half-height accent voltage gives a half-way gain (0, 1.4, 2.4, 3.3, 4.0, 4.6 dB for 0 to 5 V in 1 V steps on the Kick). The 0.5 ms window is the same for every voice.
   - Level holding: Clap BAND 2.6 dB and RES 0.9 dB across the knob; Tom sizes 4.25 to 4.28 V; Kick TUNE 0.4 dB; Rim TUNE 0.8 dB in loudness.
   - Sleep: every voice except crash and ride sleeps 0.1 s after it falls silent and wakes on the next hit.
   - AGE: 12 different serial numbers at AGE 100 % stay inside 3.2 to 7.3 V peak; AGE 0 is identical on every unit.
   - The hand-over of a new sample uses a try-lock on the audio thread, so the audio thread never waits for the menu.

---

## 909 Kick

1. **[Medium, uncertain] The shape of the pitch sweep should be checked against a recording.** `Engines909.hpp:115`, stock values at `res/voices.py:30, 34-36`.
   - The model lets the pitch fall exponentially in octaves: `BASE * 2^(SWEEP * e1 + BEND * e2)`. In an analogue VCO fed from a discharging capacitor through a resistor, the frequency in Hz follows the envelope directly, so the fall is faster at the start and it lands on the note sooner (medium-low confidence that the 909's VCO works this way).
   - Evidence, stock: starts at 201 Hz; 143 Hz at 5 ms; 111 Hz at 10 ms; 80 Hz at 20 ms; 67 Hz at 30 ms; 57.6 Hz at 50 ms; within 5 % of the 52 Hz base only at 79 ms. A linear-in-Hz sweep with the same start and time constant would be at 132 Hz at 10 ms, 75 Hz at 30 ms and within 5 % at about 65 ms.
   - My recollection is that a real 909 kick starts somewhat higher (250 Hz or more) and is essentially on its note in 40 to 60 ms at a middle TUNE setting (low confidence).
   - TUNE as "how long the sweep takes" matches what I remember of the real knob (medium-high confidence). Measured range: on the note after 29 ms at minimum, 281 ms at maximum.
   - Proposed change: measure the first five or six half-cycles of a real kick at three TUNE settings and fit SWEEP, the time constant and the law to them. If linear fits better, `f = BASE * (1 + (2^SWEEP - 1) * e1 + ...)`.

2. **[Low] The click is a bare rectangular pulse plus noise.** `Engines909.hpp:124-133`.
   - Evidence at stock ATTACK: the pulse is a 2.1 V step up, then a 1.9 V step down at 1 ms; the noise burst peaks at about 1 V; together the click peaks at 2.9 V against a 4.55 V body. Because the noise is random, the stock peak varies from 4.54 to 5.08 V hit to hit (5.29 to 6.32 V at ATTACK 100 %).
   - The click is the same at every sample rate (band 1 to 10 kHz: 0.48, 0.49, 0.47, 0.49 V at 44.1, 48, 96, 192 kHz).
   - SNAP ("length of the click") only lengthens the noise; the pulse stops growing at 1 ms (`:100`). Say so in the tooltip or let the pulse follow.
   - I do not remember the real pulse width or noise filter well enough to judge the stock values (low confidence).

3. **[Low] The accented stock kick is purer than the unaccented one.** `res/voices.py:41` (SHAPE 15 %).
   - Evidence: third harmonic -33 dB unaccented, -42 dB accented. The 15 % of triangle in the stock SHAPE and the output stage's rounding have opposite signs and partly cancel when the level goes up.
   - Harmless, but a sign that the stock "imperfect sine" comes from two unrelated places. After fixing All modules 1, set SHAPE by ear or from a recording's harmonics. The real waveshaper leaves a few per cent of distortion (medium confidence).

4. **[Low] Retrigger click.** See All modules 5.

5. **Things that checked out.**
   - Pitch per cycle identical at all four rates; settles at 52.08 Hz.
   - Level: loudness within 0.01 dB at all rates; 3.0 dB peak spread across BASE, 0.5 dB in loudness.
   - First sample with ATTACK 0 is 0.1 V: the body starts from zero, no click of its own.
   - DECAY reads true: "500 ms" reaches -40 dB at 333 ms, which is -60 dB at 500 ms.

---

## 909 Snare

1. **[Medium, medium-low confidence] The noise section probably has two paths on the real machine, and TONE is not a plain low-pass.** `Engines909.hpp:186-195`, `res/voices.py:52-53, 61-62`.
   - As I remember the block diagram: the noise goes through a low-pass into a VCA that is always part of the sound, and separately through a high-pass into a second VCA with its own envelope; SNAPPY sets how much of the noise is mixed in, and TONE acts on the high-passed part (its level or its envelope time), not as a filter corner.
   - The model has one path: high-pass (COLOUR), low-pass (TONE), one envelope.
   - Proposed change: check the service notes. If my memory is right, split the noise into a low-passed layer and a high-passed layer with separate envelopes, and make the stock-row TONE the control the real one is.

2. **[Low-Medium, low confidence] The stock pitches are the 808's.** `res/voices.py:51, 55`.
   - 185 Hz and a ratio of 1.78 give 185 and 330 Hz (measured: 190 and 330 Hz), the figures usually quoted for the 808's two shells. I have no reliable figures for the 909's two oscillators.
   - Proposed change: measure a real 909 snare with SNAPPY at zero. The TUNE (90 to 400 Hz) and RATIO (1.2 to 3.0) ranges will cover whatever it turns out to be.

3. **[Low] The noise filters are very gentle, and let ultrasonic noise through at high rates.** `Engines909.hpp:189-191`.
   - Each is a single-pole filter (6 dB per octave). Evidence, noise in the first 100 ms: 0.08 V below 500 Hz, 0.33 V from 500 Hz to 2 kHz, 0.45 V from 2 to 5 kHz, 0.46 V from 5 to 10 kHz, 0.28 V from 10 to 20 kHz. COLOUR and TONE therefore act more like tilt controls.
   - Evidence: the peak rises from 6.20 V at 48 kHz to 6.48 V at 96 kHz and 7.28 V at 192 kHz, while the level below 20 kHz stays the same. The extra is noise above 20 kHz, which DRIVE will then fold back.
   - Proposed change: two-pole filters for COLOUR and TONE (the `Svf` is already there), and a two-pole 18 kHz guard.

4. **Things that checked out.**
   - Noise against shells at stock: -1.8 dB in the first 20 ms, +2.3 dB from 20 to 60 ms, +7.0 dB from 60 to 150 ms. That reads as a snare, not a tom (the 808's problem).
   - Shells reach -60 dB at 168 ms, noise at 251 ms.
   - BALANCE dips 1.9 dB at either end; RATIO, SWEEP and BEND move the level by less than 0.7 dB.

---

## 909 Tom

1. **[Medium, medium confidence] Each real 909 tom has more than one oscillator.** `Engines909.hpp:241-249`, `res/voices.py:80`.
   - As I remember the service notes, each of the three tom circuits has three VCOs at slightly different pitches with different envelope lengths, mixed together, plus the noise. That is where the 909 tom's slightly "chorused", thick attack comes from.
   - The model has one VCO; DOUBLE adds a second at a fixed 1.035 ratio and is 0 at stock.
   - Proposed change: check the notes; if confirmed, make three oscillators the stock sound (two short ones for the attack, one long), and let DOUBLE become their spread.

2. **[Medium] Skin noise level depends on the sample rate.** See All modules 3.

3. **Things that checked out.**
   - Pitches settle at 100.0, 145.0 and 210.0 Hz, each starting half an octave up (132, 195, 286 Hz on the first cycle).
   - The three sizes are level: 4.28, 4.27, 4.25 V peak without noise. Over V/OCT from -2 to +3 octaves: 3.75 to 4.27 V.
   - Decay to -60 dB: 0.62, 0.51, 0.43 s.
   - The second oscillator is skipped when DOUBLE is 0.
   - I have no confident figure for the real toms' pitches (low confidence); 100, 145 and 210 Hz are plausible mid-knob values.

---

## 909 Rim

1. **[Medium] V/OCT changes the level by 10 dB.** `Engines909.hpp:287-290` (compensation worked out from the TUNE knob only) against `:303` (the pitch actually used includes V/OCT).
   - Evidence, loudness of the first 50 ms over V/OCT -2, -1, -0.5, 0, +0.5, +1, +2: 1.30, 1.11, 0.91, 0.71, 0.81, 0.59, 0.41 V. Over the TUNE knob the same measure stays between 0.69 and 0.75 V.
   - Proposed change: pass the V/OCT voltage into `hit()` (or work the three `strikeComp` values out on the first sample after the hit, where V/OCT is known). Add a test like the one for TUNE.

2. **[Low] The peak still wanders with TUNE, RATIO and STRIKE although the loudness holds.** Evidence, peak: TUNE 4.3 to 6.5 V, RATIO 4.6 to 7.0 V, STRIKE 3.7 to 5.8 V. The three resonators add up at different moments inside a 6 ms gate, so this is natural; no change needed.

3. **[Low] SNAP at the top of its range is nearly silent.** `res/voices.py:97`. Loudness falls from 0.91 V at 100 Hz to 0.06 V at 5 kHz (24 dB), because the high-pass ends up above all three resonators. Narrow the range to about 100 to 2500 Hz or add some make-up gain.

4. **[Low, low confidence] The stock hit is very short.** Evidence: -20 dB at 8.4 ms, -60 dB at 14 ms. I remember the real rim shot as short and hard but with a little more ring (20 to 30 ms). Check against a recording; GATE and RING already reach it.

5. **[Low] Structure and tuning.** Three bridged-T resonators into a clipping stage and a high-pass matches what I remember (medium confidence). Measured at stock: 503 Hz (strongest), 1119 Hz (-3.5 dB), 1806 Hz (-8.8 dB). I am not confident of the real frequencies (low confidence).

6. **[Low] Each hit costs 13 microseconds at 48 kHz (26 at 96 kHz)** for the nine pulse transforms in `strikeComp` (`Common909.hpp:90-97`). Harmless; cache the result while STRIKE, TUNE and RATIO have not moved if you want it gone.

---

## 909 Clap

1. **[Low] MIX changes the level by 7 dB.** `Engines909.hpp:372-374`. Loudness is 1.14 V at the centre, 0.71 V fully to bursts and 0.50 V fully to tail. Raise the tail weight (0.6) or use an equal-power law if an even MIX is wanted.

2. **[Low] Peak varies from hit to hit**, 4.32 to 6.72 V over 50 stock hits. Natural for noise bursts; mentioned so nobody chases it.

3. **Things that checked out.**
   - Envelope: bursts at 0, 9, 18 and 27 ms, each a falling ramp, the last one ringing on, then the tail; the code now matches its comment.
   - BAND and RES hold the loudness (2.6 dB and 0.9 dB across the knobs).
   - Spectrum centred on 1.0 to 1.4 kHz, -3 dB at 600 Hz and about 3 kHz.
   - Sample rate: within 0.1 dB averaged over 16 hits.
   - On the real 909 the clap shares its noise generator with the snare (medium confidence). Here each has its own; that is the friendlier choice in a modular.

---

## 909 Cymbals

1. **[High] After a crash or ride at stock DECAY, ENV sticks at 5.6 V and the module never sleeps.** `Engines909.hpp:428-430` (DECAY fully up sets the VCA to "no decay"), `:483-497`, `:499`.
   - With DECAY fully up (the stock setting for crash and ride) the VCA level `ve` stays at 1 for ever. The sound ends because the built-in source fades out, but ENV is taken from `ve` alone.
   - Evidence: ENV reads 5.62 V at 0.1, 1, 2, 4 and 7.9 s after an unaccented crash (10 V after an accented one), while the audio is silent from 1.25 s. `sleeping()` never becomes true, so the module keeps costing about 0.4 % of a core, and the light stays lit. The same happens with a loaded WAV after it has finished.
   - Proposed change: end the hit when the source ends (`se` below 1e-5, or the WAV position past its end): set `ve = 0`. Take ENV from the product of the VCA and the source level. Add a test: crash at stock, ENV below 0.01 V and asleep after 4 s.

2. **[High] Crash and ride tails get gritty and then stop dead, because the fade happens before the 6-bit converter.** `Engines909.hpp:473-474` (source decay `se`), `:487-490` (quantising).
   - A 6-bit converter has 32 steps each way. The built-in sound starts at about 6 to 10 steps RMS, which is right. As it fades it uses fewer and fewer steps, and below half a step it rounds to nothing.
   - Evidence, crash at stock, level every 0.2 s: -0, -6, -11, -16, -21, -27 dB, then silence from about 1.1 s. At 8 bits it lasts to 1.7 s, at 12 bits to 2.7 s (the intended 2.2 s decay). Ride at stock: -1, -9, -18, -28 dB, silent before 1.0 s.
   - On the real 909 the recordings were stored at a steady full level and an analogue VCA after the converter put the decay back; that keeps the signal well above the 6-bit steps all the way down (medium confidence; this is the usual explanation of why its cymbals sound better than 6 bits should).
   - Proposed change: do not fade the built-in source before the converter. Keep it at full level, quantise, and apply both the "recording's own decay" and the DECAY knob in the VCA after the reconstruction filter. BITS then sets a constant amount of grit, as on the real machine, and the tail fades smoothly to silence.

3. **[Medium] The DECAY knob does not read true, and its stock dot is wrong for crash and ride.** `Engines909.hpp:423, 428-433`, `res/voices.py:128`.
   - The VCA decay multiplies the source's own fade (1.2 s for the hat), so the two add up.
   - Evidence (12 bits, to keep finding 2 out of it), time to -60 dB: open hat at "700 ms" 445 ms; open hat at "4 s" 0.94 s; closed hat at "45 ms" 49 ms (fine); crash with DECAY left on the hat's dot 0.55 s; crash with DECAY fully up 2.27 s.
   - Switching TYPE to CRASH therefore gives a half-second crash until the user finds that DECAY must go fully up; the orange dot on the panel is the hat's.
   - Proposed change: once finding 2 is done, the VCA is the only decay and the knob can read true for every type. Give crash and ride their own stock decay inside the engine (for example DECAY as a multiple of the type's natural length, with the dot at 1.0).

4. **[High for anyone loading samples] A loaded WAV is read with no resampling or interpolation.** `Engines909.hpp:450-454`.
   - Each clock tick takes `user[(size_t)sPos]` and advances by `userRate / 32000`. For a 44.1 kHz file that is 1.378 samples per tick with the fraction thrown away, so the timing of each reading is wrong by up to one sample, in a pattern that repeats 100 times a second. Anything in the file above 16 kHz also folds down.
   - Evidence, a pure sine in the WAV, 12 bits, RECON fully open, strongest false tone relative to the sine:

     | WAV rate | 1 kHz sine | 5 kHz sine | 9 kHz sine |
     |---|---|---|---|
     | 32 kHz (one sample per tick) | -45 dB | -42 dB | -49 dB |
     | 44.1 kHz | -36 dB | -19 dB (at 7.1 kHz) | -10 dB (at 3.1 kHz) |
     | 48 kHz | -39 dB | -19 dB (at 11 kHz) | -10 dB (at 7 kHz) |

   - The 44.1 and 48 kHz rows are the same at a 96 kHz host rate, so this is the reading, not the converter model. On hats it is masked; on a ride bell, a cowbell or anything tonal it is a clear false note.
   - Proposed change: when the file is loaded (on the menu thread), convert it once to 32 kHz with a proper low-pass (windowed-sinc, cut near 15 kHz) and store that. Each tick then reads exactly one stored sample, like a ROM, TUNE behaves exactly like the real clock, and memory use drops.
   - Also worth doing there: normalise the file to full scale (a quiet WAV currently uses only a few of the 64 steps and is crushed much harder than a loud one).

5. **[Medium] The new sample is copied on the audio thread.** `Voice909.hpp:182-185`, `Engines909.hpp:419` (`user = s`).
   - The try-lock is right, but `setSample` then copies the whole buffer and may allocate and free memory.
   - Evidence: 20 s at 44.1 kHz (3.5 MB) takes 0.3 to 2.1 ms; 20 s at 192 kHz (15.4 MB) takes 1.5 to 8.8 ms. One audio block of 256 samples at 48 kHz is 5.3 ms, so a long file can cause a dropout when it is loaded.
   - Proposed change: swap instead of copy (`std::swap(engine.user, newSample)` under the try-lock) and let the menu thread free the old buffer next time it takes the lock. After finding 4 the buffers are also much smaller.
   - Minor: "New serial number" (`Voice909.hpp:272-275`) writes the AGE table from the menu thread while the audio thread reads it. Harmless in practice (a few floats), but it could go through the same kind of flag.

6. **[Medium] A WAV with an unusual header can crash Rack, and unsupported formats load as silence.** `Voice909.hpp:153-172`.
   - `bps = bits / 8` is 0 when the header says fewer than 8 bits per sample, and the next line divides by it. Evidence: the reader, copied into a test program unchanged, dies with a division error on a header with 4 bits per sample. Because the path is saved in the patch, such a file would crash on every load of the patch.
   - 12-bit, 20-bit and 64-bit-float files return "ok" with all-zero data: a silent cymbal and no message.
   - A 32-bit float file with huge values is accepted as is (the engine clamps it, so no harm).
   - If the file is missing when a patch loads, the saved path is dropped silently and the module falls back to the built-in sound.
   - Proposed change: reject anything except 8, 16, 24 and 32-bit PCM and 32 or 64-bit float; use the header's block-align field for the frame size; keep the path when the file is missing; show the failure in the menu label.

7. **[Low] The built-in hat is different on every hit; a ROM plays the same thing every time.** `Engines909.hpp:442` (the six oscillator phases and the noise are not restarted).
   - Evidence, 16 closed hats: peak 2.09 to 4.13 V (6 dB), loudness 0.35 to 0.42 V (1.4 dB).
   - The real hats are recordings, so each hit is identical apart from analogue noise (high confidence). Identical hits are part of the 909's machine feel.
   - Proposed change: restart the oscillator phases and the noise sequence on each hit, so the built-in "recording" is a recording. If the variation is liked, keep it as a menu option or tie it to AGE.

8. **[Low] TUNE changes the level a lot, and V/OCT stops tracking at four times the host rate.** `Engines909.hpp:484-485`.
   - Evidence, loudness across TUNE -1 to +1 octave: hat 0.94 to 0.30 V (10 dB), crash 0.99 to 0.48 V (6.3 dB). Raising the clock moves the sound above the fixed 11 kHz RECON filter, as it would on the hardware, so this is authentic in kind. The real hats have no TUNE control; crash and ride do (high confidence).
   - The clock is limited to 4 times the host rate, so a tone that should be at 2000 Hz plays at 1503 Hz at 48 kHz and 1382 Hz at 44.1 kHz once V/OCT plus TUNE passes about +2.5 octaves. Say so in the manual, or raise the limit (the loop guard of 8 at `Common909.hpp:224` allows it).

9. **[Low] The stepped converter model works well.** `Common909.hpp:212-239`.
   - Evidence with a 32 kHz WAV sine at 48 kHz host: the false tones caused by folding are -42 to -49 dB below the sine; at 96 kHz they are gone (below -66 dB). The genuine images of a 16 kHz clock (TUNE -1) come through at -31 to -34 dB, as they should.
   - Clock near and above the host rate (0.94, 1.00, 1.01, 1.33, 2, 2.67, 3.77 times): correct pitch, everything else below -63 dB.
   - The band-limited steps cost a little top end at low host rates: the hat's 10 to 20 kHz band is 0.27 V at 44.1 kHz, 0.28 V at 48 kHz, 0.34 V at 96 kHz and 0.36 V at 192 kHz (2.2 dB between 48 and 192 kHz). Not worth fixing.
   - `corrNext` is never set to anything but zero and can be removed. The quantiser has 65 levels (-32 to +32), where a 6-bit converter has 64; inaudible.

10. **[Low] Choke and retrigger are click-free.** The step at the moment a closed hat chokes an open one is 2.3 V, smaller than the hat's own largest sample-to-sample step (3.6 V); a crash retriggered every 400 ms steps by 2.7 V against 2.5 V inside the sound. Late level 0.2 s after the choke is below 0.05 V (the existing test).

11. **[Low, low confidence] Clock rate, filter and source.** `Engines909.hpp:405` (32 kHz), `res/voices.py:134` (RECON 11 kHz).
    - I cannot confirm the real clock rate or reconstruction filter from memory; 32 kHz and 11 kHz are reasonable placeholders. I believe closed and open hat come from one ROM and the open hat is cut when the closed one fires (medium-high confidence), which is what the model does.
    - The built-in source's square waves are not band-limited at the 32 kHz clock. For a noise-like "recording" that moves with TUNE as a whole, this is fine.

---

## Suggested order of fixes

1. **Cymbals: end the hit when the source ends**, so ENV returns to zero and the module sleeps after a crash or ride. (Cymbals 1)
2. **Cymbals: move the fade to after the converter**, so tails are smooth and BITS is a steady amount of grit; with that, make DECAY read true and give crash and ride their own stock decay. (Cymbals 2 and 3)
3. **Cymbals WAV: convert to 32 kHz when loading, normalise, hand over by swapping, and reject bad headers.** (Cymbals 4, 5 and 6)
4. **Tom noise and AGE hiss at high sample rates**: unscaled noise into the pink filter, scaled noise for the hiss. Two one-line changes plus a test. (All modules 3 and 4)
5. **Rim: include V/OCT in the level compensation.** (Rim 1)
6. **Give the output stage headroom** so accents keep their peak and DRIVE 0 is clean; then decide how much louder DRIVE should be allowed to get. (All modules 1 and 2)
7. **Fade the old tail over 1 ms on a retrigger** (Kick, Snare), and start the ramp shape from its zero crossing. (All modules 5)
8. **Check against the service notes and a recording before moving any dot:**
   - Kick: sweep law, start pitch and time to land (Kick 1); click pulse and noise (Kick 2).
   - Snare: two noise paths and what TONE really does (Snare 1); the two oscillator pitches (Snare 2).
   - Tom: three oscillators per tom (Tom 1).
   - Rim: ring length and resonator pitches (Rim 4 and 5).
   - Cymbals: clock rate and reconstruction filter (Cymbals 11).
9. **Smaller items**: remember both hat triggers inside the accent window (All modules 8); restart the built-in hat on each hit (Cymbals 7); two-pole snare noise filters (Snare 3); narrow the Rim's SNAP range (Rim 3); Clap MIX law (Clap 1); V/OCT clock limit in the manual (Cymbals 8).
10. **Tests to add to `test/voices.cpp`**, so these stay fixed: crash ENV at zero and asleep after 4 s; crash level falling smoothly to -50 dB at 6 bits; a 5 kHz sine in a 44.1 kHz WAV with false tones below -40 dB; Tom at NOISE 100 % within 1 dB at 48 and 96 kHz; Rim loudness within 3 dB over V/OCT -1 to +1; a WAV header with 4 bits per sample returns "not loaded".

---

## How the measurements were made

- The probe program includes the engine headers unchanged and renders one voice at a chosen sample rate, with knob overrides, to a raw file; Python with numpy does the analysis. Everything is in `/tmp/claude-0/-home-user/9210c62a-eadd-5f6a-9252-fee15a6fe8dd/scratchpad/fable/` (`probe.cpp`, `pr.py`, `m1.py` to `m10.py`, `bench.cpp`, `src.cpp`, `wavt.cpp`). That folder is temporary.
- The offline test (`test/voices.cpp`) was built into the scratch folder, not into the repository; all of its existing checks pass.
- "Peak" is the largest absolute output sample. "Loudness" is RMS over the stated window. Pitch is from interpolated upward zero crossings. False (non-harmonic) content is the power below 20 kHz that is not within a narrow band around a harmonic, relative to the total; test pitches were chosen so that folded harmonics do not land on real ones.
- The WAV reader was tested by copying lines 133 to 177 of `src/Voice909.hpp` unchanged into a small program.
- I did not listen to anything; every statement about how something sounds is inferred from the numbers.
- Not measured: behaviour inside Rack itself (the glue in `Voice909.hpp` was reviewed by reading, apart from the WAV reader), and any comparison with a real TR-909, for which I had neither schematics nor recordings here.
