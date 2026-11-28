# 808 Circuits: DSP review

27 November 2026. Reviewed: `src/KickEngine.hpp`, `src/Common808.hpp`, `src/VoiceEngines.hpp`, `src/VoiceSpecs.hpp`, `res/voices.py`, `src/Voice808.hpp`, `src/Kick808.cpp`, and the two offline tests. No source file was changed.

## Summary

The kick is in good shape: its pitch, level and decay are identical at 44.1, 48, 96 and 192 kHz, it settles at 49.4 Hz, the sigh lands in 55-59 Hz, DRIVE does not alias audibly, and nothing blew up at any extreme I tried. The other seven voices have several real problems:

- **The Clave's red dot is half an octave flat.** Stock plays 1768 Hz, not 2.5 kHz.
- **The Snare has almost no snare.** At stock the noise is 17 to 23 dB below the drum tone, and SNAPPY at maximum only brings it to -16.5 dB.
- **Noise voices get quieter at higher sample rates.** The Clap is 2.9 dB quieter at 96 kHz than at 48 kHz and the Maracas 3.7 dB quieter in the audible band.
- **Several "character" knobs are mostly volume knobs.** STRIKE changes level by about 16 dB on the Tom and 20 dB on the Kick (and wide is louder, not softer as the tooltips say). Clap RES and Cowbell RES change level by 10 to 12 dB.
- **Maracas GRAINS at maximum is silent on about 6 hits in 10.**
- **The Cowbell clicks on every hit** because the envelope is applied after the filter; the real circuit does it the other way round.

CPU is higher than it needs to be (about 1.2 % of a core for the Kick, 6.5 % for all eight on the slow test machine) because every knob curve is recomputed on every sample, even when the voice is silent.

**How to read the confidence notes.** Measurements are from my own probe programs run against the unmodified engines. Statements about the real TR-808 come from memory of the service notes and of Werner, Abel and Smith's papers; I could not open any of them here. Each such statement carries a confidence level, and nothing marked "medium" or "low" should change a red dot before someone checks it against the schematic or a recording.

### Measured stock levels (48 kHz, LEVEL 100 %, DRIVE 0)

| Voice | Peak, no accent | Peak, accent | Accent gain | Loudness (RMS, first 200 ms) |
|---|---|---|---|---|
| Kick | 4.43 V | 8.50 V | +5.7 dB | 1.17 V |
| Snare | 5.31 V | 9.16 V | +4.7 dB | 0.89 V |
| Clap | 4.52 V (3.96 to 6.97 hit to hit) | 6.53 V | +3.2 dB | 0.61 V |
| Tom, mid | 3.73 V | 7.76 V | +6.4 dB | 1.08 V |
| Rim | 5.21 V | 6.85 V | +2.4 dB | 0.39 V |
| Clave | 4.63 V | 8.70 V | +5.5 dB | 0.37 V |
| Maracas | 4.75 V | 6.80 V | +3.1 dB | 0.40 V |
| Cowbell | 4.69 V (4.69 to 5.92 hit to hit) | 6.73 V | +3.1 dB | 0.42 V |

### CPU cost (measured)

Single thread, Intel Xeon 2.1 GHz, built with Rack's flags (`-O3 -march=nehalem -funsafe-math-optimizations`), flush-to-zero on, 48 kHz, one hit every 250 ms. A current desktop CPU is roughly two to three times faster. 96 kHz doubles every figure.

| Voice | ns per sample | Share of one core at 48 kHz | Same, voice silent |
|---|---|---|---|
| Kick | 255 | 1.2 % | 257 |
| Snare | 200 to 235 | 1.0 % | 190 |
| Tom / Conga | 213 | 1.0 % | 135 |
| Clap | 165 to 180 | 0.8 % | 122 |
| Clave | 140 to 150 | 0.7 % | 91 |
| Rim | 135 | 0.65 % | 130 |
| Maracas | 100 to 120 | 0.5 % | 71 |
| Cowbell | 100 | 0.5 % | 78 |
| All eight | about 1350 | 6.5 % | |

---

## All modules (shared code and Rack glue)

1. **[High] Noise is quieter at higher sample rates.** `Common808.hpp:19` (`Rng::bi`), used at `VoiceEngines.hpp:67, 116, 181, 297`.
   - White noise made one random number per sample has the same total power at any sample rate, so the part inside a fixed filter band shrinks as the rate goes up: 3 dB per doubling.
   - Evidence: Clap loudness is 0.61 V RMS at 48 kHz, 0.44 V at 96 kHz (-2.9 dB) and 0.34 V at 192 kHz (-5.1 dB). Maracas below 20 kHz: 0.355 V at 48 kHz, 0.231 V at 96 kHz (-3.7 dB). Snare noise between 2 and 10 kHz: 0.046 V against 0.035 V (-2.4 dB). Resonator and oscillator voices are within 0.2 dB across rates.
   - Proposed change: multiply every noise sample by `sqrt(fs / 48000)`. Because the output then carries more ultrasonic energy at high rates, also low-pass the Maracas and Snare noise near 18 kHz so DRIVE sees the same signal at every rate.

2. **[High] STRIKE is mainly a volume knob, and its tooltip is backwards.** `Common808.hpp:43-54` (`PulseShaper`), `KickEngine.hpp:150-156`, tooltips at `res/voices.py:34, 70, 90, 103`, README kick table.
   - A wider pulse puts more energy into the resonator, so the drum gets louder until the pulse is about half a cycle long. Past that, the falling edge excites the resonator a second time and the level depends on whether the two edges add or cancel.
   - Evidence, peak volts as STRIKE goes from 0 to 1 in steps of 0.1:
     - Kick: 0.84, 1.25, 1.85, 2.58, 3.48, 4.43, 5.72, 7.39, 8.50, 6.89, 5.49
     - Tom: 1.00, 1.37, 1.94, 2.24, 2.91, 3.73, 4.93, 6.28, 6.21, 3.78, 5.96
     - Snare heads: 1.80, 2.52, 3.38, 4.04, 4.20, 5.12, 5.42, 4.20, 5.30, 4.89, 4.20
   - On the Clave, STRIKE above 0.35 leaves the peak at 4.63 V and adds a second click up to 10 ms later (a flam, not a soft mallet).
   - Proposed change: keep the 1 ms rectangular pulse as the stock setting, but compensate the level (divide the pulse height by the ring amplitude a pulse of that width produces at the current tuning, which is proportional to `|sin(pi * f * width)|` for a rectangular pulse, with a floor). For "soft mallet", low-pass the pulse edges (a one-pole at `1 / width`) instead of widening it; that removes highs from the attack without the second click.

3. **[High] Level depends on tuning through the same pulse effect.** `Common808.hpp:63-71`, `KickEngine.hpp:196`.
   - When pulse width times frequency is a whole number, the two edges cancel. The Clave at 2.5 kHz with a 1 ms pulse sits on a maximum; a few semitones away it does not.
   - Evidence: Clave peak over the TUNE knob (-2 to +1 octave) runs 8.85 V down to 1.77 V, a 14 dB spread. In semitone steps over V/OCT from -12 to +12: 6.69, 6.54, 6.39, 6.24, 6.08, 5.90, 6.17, 6.61, 6.75, 6.60, 6.12, 5.24, 4.63, 4.44, 4.26, 4.08, 3.91, 4.43, 4.48, 3.77, 3.22, 3.05, 2.86, 3.11, 3.20. Played as a tuned instrument, neighbouring notes differ by up to 1.5 dB and the top is 6 dB below the bottom.
   - The Kick is flat (4.2 to 4.7 V) across its 20 to 220 Hz knob, but V/OCT above that drops it: 1.78 V at 440 Hz, 1.29 V at 880 Hz. The attack jump multiplies the frequency by 2.2 while the pulse is on, which puts the 1 ms pulse near a full cycle.
   - Proposed change: the compensation in finding 2 fixes this too. A simpler alternative for the Clave only is to scale the pulse width with pitch (`width = strike * 2500 / f`).

4. **[Medium] Accent depth differs from voice to voice.** `VoiceEngines.hpp:12` (`accGain`), `Common808.hpp:128`.
   - Resonator voices scale with the trigger voltage (4 V to 10 V at stock ACCENT, which is +8 dB before the output stage rounds it off); voices with a VCA use a square-root law (+4 dB). The table above shows the result: +2.4 dB on the Rim, +3.1 dB on Clap, Maracas and Cowbell, +5.5 to +6.4 dB on Kick, Tom and Clave.
   - In the Snare the heads follow the first law and the noise the second, so an accented snare has relatively less noise.
   - On the real machine accent raises one common trigger voltage for all voices (high confidence), so the depth should be broadly similar.
   - Proposed change: pick one target (about +5 dB at stock ACCENT) and use the same law everywhere; in the Snare, scale heads and noise together.

5. **[Medium] Accent can be missed when it arrives one sample after the trigger.** `Voice808.hpp:107-112`, `Kick808.cpp:130-135`.
   - The accent voltage is read on the exact sample the trigger fires. In Rack every module a signal passes through adds one sample of delay, so an accent that goes through a mult, logic or attenuator module arrives late and is ignored for that hit.
   - Proposed change: when a trigger arrives, wait about 0.5 ms (24 samples at 48 kHz), track the highest accent voltage seen in that window, then fire. The delay is the same for every voice, so the kit stays aligned.

6. **[Medium] CPU: every knob curve is recomputed on every sample, even in silence.** `KickEngine.hpp:145-231`, `VoiceEngines.hpp:31` (the `V()` macro calls `pow` each time), `Common808.hpp:63-108`.
   - The Kick makes about 15 calls to `exp`, `pow`, `tan`, `tanh`, `log1p` and `sqrt` per sample; the Snare about 12. An idle Kick costs the same as a ringing one.
   - Proposed change, in order of payoff:
     1. Recompute knob-derived values (`V()`, `decayT60`, `toneHz`, filter `tan` terms, the fixed `exp(-dt / tau)` factors) every 16 samples, or only when the knob or CV value changed.
     2. Keep per-sample maths only where audio-rate modulation needs it (the Kick's sigh and jump; pitch bends).
     3. Skip processing when the envelope and resonator amplitude are below about -120 dB and AGE hiss is off.
   - Expected result: roughly a third of the current cost when playing and close to zero when idle.

7. **[Low] Without flush-to-zero the resonator voices run three to seven times slower.** `Common808.hpp:76`, `KickEngine.hpp:202-203`.
   - After a hit the filter states decay into denormal numbers and stay there (idle output measured at 1e-42 V).
   - Evidence: with `-O2` and no flush-to-zero, the Rim costs 981 ns per sample against 138 ns with it; Kick 717 against 222.
   - Rack turns flush-to-zero on for its audio threads (high confidence), so this only affects `make test` and any other host. Proposed change: when both states are below 1e-20, set them to zero.

8. **[Low] DRIVE leaves a small DC offset.** `KickEngine.hpp:215-224`, `Common808.hpp:122-125`.
   - The Kick's 12 Hz high-pass sits before the drive stage and the other voices have none, so an asymmetric signal comes out of the `tanh` with DC on it.
   - Evidence: mean over the first 150 ms at DRIVE 1 with accent: Kick +0.19 V, Tom +0.09 V, Snare +0.08 V, Clap -0.13 V. In the Kick's sustain mode the DC line is 23 dB below the fundamental.
   - Proposed change: move (or add) a 10 Hz one-pole high-pass after the drive stage in all eight voices.

9. **[Low] Worn-part hiss is seven times louder with DRIVE on seven of the eight voices.** `VoiceEngines.hpp:25-28` against `KickEngine.hpp:225-228`.
   - The seven voices add hiss before the drive stage; the Kick adds it after.
   - Evidence: idle noise at AGE 1 is 3.1 mV RMS everywhere at DRIVE 0 (64 dB below 5 V). At DRIVE 1 it is 23.5 mV (-47 dB) on Snare, Clap, Tom, Clave, Maracas and Cowbell, and still 3.1 mV on the Kick and Rim.
   - Proposed change: choose one. Before the drive is the more authentic place (hiss comes from the parts ahead of the output amplifier), so move the Kick's.

10. **[Low] LEVEL and other knobs are not smoothed.** `VoiceEngines.hpp:27`, `KickEngine.hpp:229`.
    - By inspection: Rack updates a knob dragged with the mouse about 60 times a second, so LEVEL moves in steps. This is inaudible on short hits but will tick on the Kick in sustain mode and on long Cowbell or Clave rings.
    - Proposed change: a 5 ms one-pole on LEVEL and on the DRIVE gain.

11. **[Low] A NaN on V/OCT silences the Cowbell until the patch is reloaded.** `VoiceEngines.hpp:333-335`, `Voice808.hpp:104`.
    - Evidence: after 10 samples of NaN on V/OCT, the next Cowbell hit peaks at 1.31 V instead of 4.69 V and stays that way (the oscillator phase is stuck at NaN). Kick, Snare, Tom, Rim and Clave recover on the next hit. Knob CVs are safe because Rack's `clamp` turns NaN into a number.
    - Proposed change: in the glue, `voct = isfinite(v) ? clamp(v, -10, 10) : 0`; in `pulse()`, reset the phase if it is not finite.

12. **[Low] Every noise voice uses the same random sequence.** `Common808.hpp:16`, `VoiceEngines.hpp:17`.
    - All engines seed their noise with 1, so a Snare and a Clap (or two Snares) loaded together produce the identical noise sample for sample. Two layered Snares sum to one louder Snare with no thickening.
    - This happens to match the real machine, where one noise generator feeds snare, clap and maracas (medium confidence). If kept, say so in the README. If not, seed from the module's serial number; the "AGE 0: every unit identical" test in `test/voices.cpp` would then need the same serial in both renders.

---

## 808 Kick

1. **[Medium] The attack jump is probably too small: about 1.5 octaves on the real circuit, 1.1 here.** `KickEngine.hpp:35` (`SWEEP = 0.45`), `:167`.
   - From memory of the service-note schematic (medium confidence; please check before changing the dot): the bridged-T uses C41 = C42 = 15 nF, R167 = 1 MΩ, and R165 = 47 kΩ to ground with R166 = 6.8 kΩ switched in parallel by Q43. That gives about 50 Hz with Q43 off, which agrees with the paper's 49.5 Hz, and about 137 Hz with Q43 fully on: a ratio of 2.8, or 1.47 octaves.
   - The code jumps 1.125 octaves (108 Hz). The first downward zero crossing is at 2.5 ms now.
   - Proposed change: if the schematic confirms the values, set stock SWEEP to 0.59 (1.47 octaves). The Q rise already follows the jump correctly (`:188`, decay rate held constant).

2. **[Medium] The jump ends as a hard gate; the real transistor lets go gradually.** `KickEngine.hpp:158-160`.
   - The model holds the full jump for 6 ms, then drops with a 0.7 ms time constant. In the circuit Q43 is driven from the envelope on C38, which decays, so the resistance (and the pitch) should glide down over the hold time (medium confidence on the mechanism, low on the exact curve).
   - Proposed change: drive the jump from a decaying envelope through a soft threshold: `env = exp(-t / tau)`, `jump = smoothstep((env - off) / width)`. With `tau` chosen so the jump is over at 6 ms, the stock sound keeps its timing and gains a short glide. Compare the first two cycles against a recording before choosing the curve.

3. **[Medium] In sustain mode the pitch is 84 to 170 cents sharp of the TUNE setting.** `KickEngine.hpp:169-173, 189-192`.
   - The self-oscillation level is high enough to keep the sigh permanently engaged.
   - Evidence: TUNE 49.5 Hz gives 51.95 Hz at DECAY 0.96 and 54.62 Hz at DECAY 1.0. The offset is the same at every pitch (+84 and +170 cents from 30 Hz to 1760 Hz), so V/OCT tracks; only the absolute tuning is off, and it moves with the DECAY and SIGH knobs. With SIGH at 0 the pitch is exact (49.50 Hz).
   - Proposed change: fade the sigh out as DECAY passes 0.95, or lower the limiter level in sustain so the swing stays under the leak threshold (the output gain can make up the level).

4. **[Low] Sustain level falls at high pitch.** `KickEngine.hpp:211`.
   - Evidence at DECAY 1.0: 4.06 V at 49.5 Hz, 3.82 V at 440 Hz, 3.21 V at 880 Hz, 2.17 V at 1760 Hz, with TONE at stock (1.2 kHz low-pass).
   - Proposed change: none needed if this is intended as tone; otherwise let V/OCT move the TONE corner with the pitch in sustain mode.

5. **[Low] Accented stock hits are already into the output stage's curve.** `KickEngine.hpp:223-224`.
   - Evidence: second and third harmonic at -54 and -67 dB without accent, -34 and -44 dB with full accent input (peak 8.5 V). Part of the second harmonic is the sigh itself, which is authentic.
   - Proposed change: none required. If a cleaner accent is wanted, lower `BODY_GAIN` slightly and make it up after the `tanh`.

6. **[Low] Things that checked out.**
   - Sample rate: peak 4.43 to 4.47 V and identical per-cycle pitch at 44.1, 48, 96 and 192 kHz.
   - Pitch: unaccented cycles run 55.1, 52.3, 51.0, 50.0, 49.6 Hz and settle at 49.43 Hz; accented start at 59.1 Hz.
   - Decay: measured T60 is within 2 % of the knob's figure from 80 ms to 8 s.
   - Retrigger: rolls down to 10 ms stay inside the 10.5 V rails at every DECAY setting, including sustain.
   - DRIVE aliasing: non-harmonic content is below -80 dB at 48 kHz even at 1 kHz in sustain with DRIVE 1.
   - Bad input: a NaN on V/OCT is cleared on the next hit.

---

## 808 Snare

1. **[High] The noise is far too quiet.** `VoiceEngines.hpp:44` (`GN = 1.45`), `:71`.
   - Evidence at stock: noise is 23 dB below the heads in the first 20 ms, 20.5 dB below from 20 to 60 ms and 17 dB below from 60 to 150 ms. With SNAPPY at 100 % it is still 16.5 dB below. The peak barely moves across the whole SNAPPY range (5.12 V to 5.51 V).
   - On the real snare the noise is at least as loud as the shells at mid SNAPPY (high confidence; it is what makes it a snare).
   - Proposed change: raise the noise gain by about 14 to 18 dB, so that SNAPPY 50 % gives noise RMS roughly equal to the head RMS over the first 60 ms, then lower the head gain `G` to bring the peak back to 5 V. Fit the final balance against a recording.

2. **[Medium] The 808's TONE knob is the balance between the two shells, not a filter on the noise.** `res/voices.py:27` (TONE), `:31` (BALANCE); `VoiceEngines.hpp:65-69`.
   - In the real circuit the two bridged-T outputs feed the two ends of the TONE pot (medium-high confidence), and SNAPPY sets the noise level. Here BALANCE does that job on the second row and TONE is a low-pass on the noise.
   - Proposed change: put the shell balance on the stock row as TONE with its red dot, and move the noise low-pass to the SNAPPY section under another name (for example HISS).

3. **[Medium] The head ring is long for an 808 snare.** `res/voices.py:32` (DECAY stock 220 ms), `VoiceEngines.hpp:62-64`.
   - Evidence: the heads reach -60 dB at 211 ms and the 180 Hz shell is 9 dB louder than the 330 Hz one, which together with finding 1 makes the stock sound close to a high tom.
   - My recollection is that the real shells are shorter than the noise (low-medium confidence).
   - Proposed change: after fixing the noise level, compare against a recording and shorten the stock DECAY if needed (the range 40 ms to 2 s already covers it).

4. **[Low, uncertain] Published shell frequencies disagree.** `res/voices.py:26, 30`.
   - The 180 and 330 Hz used here appear in several analyses. Other write-ups quote 238 and 476 Hz, an octave apart (low confidence on which is right; I could not check the source).
   - Proposed change: measure a clean recording with SNAPPY at zero before touching the dots. The RATIO range (1.2 to 3.0) covers both.

5. **[Low] The stock attack sweep has no counterpart I know of in the circuit.** `res/voices.py:33` (SWEEP 0.15, which is 0.3 octave over 3 ms).
   - The snare's bridged-T networks have no pitch-bending transistor that I am aware of (medium confidence).
   - Proposed change: set stock SWEEP to 0 unless a recording shows the bend.

---

## 808 Clap

1. **[Medium] BAND and RES change the level by 9 to 10 dB.** `VoiceEngines.hpp:117-119`.
   - Noise through a band-pass normalised to unity peak loses power as the band narrows.
   - Evidence, RMS over the hit: across BAND 0.35, 0.47, 0.62, 0.76, 0.86, 0.95 V; across RES 0.95, 0.54, 0.42, 0.36, 0.33, 0.30 V.
   - Proposed change: normalise for bandwidth, not peak: multiply by `sqrt(Q)` and by `sqrt(1000 / band_hz)`, pinned so the stock setting is unchanged.

2. **[Medium] All bursts have the same shape; on the real clap the last one rings longer.** `VoiceEngines.hpp:105-112`.
   - The real envelope is three quick sawtooth pulses about 10 ms apart followed by a fourth that decays more slowly, alongside the separate reverb envelope (medium confidence).
   - Proposed change: give the final burst a decay about four to five times the SNAP time. Also consider making each burst a linear ramp down to the next retrigger, which is closer to a sawtooth than the current exponential (4 ms time constant, 8 % left when the next burst fires).

3. **[Low] The code does the opposite of its comment on burst spacing.** `VoiceEngines.hpp:108-109`.
   - The comment says each gap is a touch longer than the last; `1 + 0.04 * burstsLeft` makes each gap shorter (12 %, 8 %, 4 % over the base).
   - Proposed change: use the count of bursts already fired, or correct the comment. On the real circuit I believe the spacing is even (low confidence).

4. **[Low] Peak level varies a lot from hit to hit.** Evidence: 3.96 to 6.97 V over 50 stock hits. This is natural for noise bursts and needs no change, but a mixer or compressor after it will see 5 dB swings.

---

## 808 Tom / Conga

1. **[Medium] The three sizes differ in level by 5.5 dB, high loudest.** `VoiceEngines.hpp:175-176` (`wRef = 2π · base`).
   - The excitation compensation is referenced to each range's own pitch, so it does nothing between ranges, and a resonator's ring is proportional to its frequency.
   - Evidence, peak / RMS: toms low 2.70 / 0.81 V, mid 3.73 / 1.08 V, high 5.09 / 1.40 V; congas 3.37, 5.09, 6.65 V peak.
   - Proposed change: use one fixed reference for all ranges (for example `2π · 130`), then trim `G` so the mid tom stays at 3.7 V.

2. **[Low-Medium] The SLAP noise has no high-pass, so it adds random low-frequency thump.** `VoiceEngines.hpp:181`.
   - Pink noise through a 2.5 kHz low-pass keeps all of its sub-bass. Evidence: stock peak varies 3.61 to 4.37 V hit to hit, and the output has a -0.02 V mean over 200 ms.
   - Proposed change: high-pass the noise at about 200 Hz before the envelope.

3. **[Low] The pink filter's colour shifts with sample rate.** `Common808.hpp:111-119`.
   - The Kellet coefficients are fixed for 44.1 kHz. Evidence: SLAP noise between 1 and 5 kHz is 1.2 dB lower at 96 kHz. Small, and finding 1 under "All modules" matters more.
   - Proposed change: none needed once the noise level is scaled by sample rate; or recompute the three pole positions from their corner frequencies.

4. **[Low] The second resonator runs even when DOUBLE is 0.** `VoiceEngines.hpp:177-178`. Skip it below about 0.001 to save roughly a quarter of the voice's CPU.

---

## 808 Rim

1. **[Low] Peak level wanders with TUNE, RATIO and STRIKE.** `VoiceEngines.hpp:218-222`.
   - Evidence: 3.84 to 6.02 V across TUNE, 4.31 to 6.13 V across RATIO, 4.60 to 6.44 V across STRIKE. The cause is the pulse-edge effect (All modules, finding 3) combined with the short gate catching the two resonators at different phases.
   - Proposed change: fixed by the shared pulse compensation; no Rim-specific work.

2. **[Low] The structure matches what I know of the circuit.** Two bridged-T resonators at about 455 Hz and 1667 Hz into a distorting gate and a high-pass (medium-high confidence). Measured spectrum at stock: 1667 Hz strongest, the 455 Hz region 6 dB down and spread wide by the 8 ms gate.

---

## 808 Clave

1. **[High] The stock setting is 1768 Hz, not 2.5 kHz.** `res/voices.py:100`, generated into `src/VoiceSpecs.hpp:135`.
   - The TUNE knob runs from -2 to +1 octave, so its zero is at position 0.667, but the stock position is written as 0.5, which is -0.5 octave.
   - Evidence: the spectrum of a stock hit peaks at 1767 Hz. With TUNE at 0.667 it peaks at 2500 Hz (4.48 V peak).
   - Proposed change: `pos_lin(0, -2, 1)` for the stock position, then regenerate the header and the panel so the red dot moves. Add a test that the stock Clave peaks within 2 % of 2500 Hz.

2. **[High] Level across TUNE and V/OCT is uneven.** See All modules, finding 3. This matters most here because the README offers the Clave as a tuned log drum and marimba.

3. **[Medium] STRIKE does not behave as "hard stick to soft mallet".** See All modules, finding 2. Above 0.35 it adds a delayed second click.

---

## 808 Maracas

1. **[Medium] GRAINS at the top of its range gives silent hits.** `VoiceEngines.hpp:296-299`.
   - At GRAINS 1 the smooth noise is fully off and the bead rate is 0.0002 per sample, about 10 per second, so most 60 ms hits contain no bead at all.
   - Evidence over 100 hits at 48 kHz: 63 silent at GRAINS 1.0, with the loudest at 9.15 V. At GRAINS 0.9 the median peak is 1.42 V and the maximum 9.05 V.
   - Proposed change: tie the bead rate to the envelope and give it a floor of a few hundred per second (so every hit has several beads), and lower the bead amplitude from 6 to about 3.

2. **[Medium] Bead density doubles at 96 kHz.** `VoiceEngines.hpp:298`.
   - The probability is per sample, not per second. Evidence: median peak at GRAINS 0.8 is 2.50 V at 48 kHz and 5.94 V at 96 kHz.
   - Proposed change: `p = rate_hz / fs`.

3. **[High] Audible level drops at high sample rates.** See All modules, finding 1 (-3.7 dB below 20 kHz at 96 kHz).

4. **[Low] The structure matches the circuit as I know it.** White noise, a short attack-decay VCA and a high-pass near 5 kHz (medium confidence). Measured envelope: peak at 2 ms, 20 % left at 16 ms, gone by 64 ms.

---

## 808 Cowbell

1. **[Medium] The envelope is after the filter; the real circuit gates the oscillators first and filters afterwards.** `VoiceEngines.hpp:348-356`.
   - In Werner, Abel and Smith's description the two square waves each pass through a VCA driven by the two-slope envelope, and the sum then goes through the band-pass (medium-high confidence on the order).
   - Because the oscillators run all the time (correct) and the gain jumps from 0 to 1 in one sample (`:325`), the output can step by several volts at the hit. Evidence: largest sample-to-sample step over 200 hits is 6.47 V, against 0.84 V in the steady tone; energy above 8 kHz in the first 3 ms is two to five times that 20 ms later; and the stock peak varies 4.69 to 5.92 V with the phase the oscillators happen to be in.
   - Proposed change: multiply the summed squares by the envelope, then band-pass. The filter rounds the onset the way the circuit does and rings a little after the envelope ends. Retrim `G`.

2. **[Medium] RES changes level by 12 dB.** `VoiceEngines.hpp:351`.
   - Evidence, peak across RES: 7.77, 4.30, 3.22, 2.62, 2.21, 1.92 V. FILTER moves it 6 dB (5.69 to 2.92 V).
   - Proposed change: do not divide by `q` in full; use `bp / sqrt(q)` pinned to the stock level, so more resonance sounds more resonant without dropping away.

3. **[Low, uncertain] The harmonics are louder than the fundamentals.** Evidence at stock: 2400 Hz at 0 dB, 2700 Hz at -4 dB, 800 Hz at -6 dB, 1620 Hz at -7 dB, 540 Hz at -9 dB. I do not remember the real band-pass centre or Q well enough to say this is wrong (low confidence). Compare with a recording; if the real one has stronger 540 and 800 Hz, lower the stock FILTER or RES.

4. **[Low] Oscillator aliasing is fine at stock and acceptable when pitched up.** `VoiceEngines.hpp:328-341`.
   - Evidence, non-harmonic content below 20 kHz relative to the whole signal: -57 dB at 48 kHz and -54 dB at 44.1 kHz at stock pitch; -51 dB one to two octaves up; -46 dB three octaves up (4.3 kHz).
   - Proposed change: none for normal use. If the Cowbell is meant to be played melodically high up, replace the two-sample polyBLEP with a four-sample one.

5. **[Low] NaN on V/OCT kills the oscillators.** See All modules, finding 11.

---

## Top 10, in order

1. **Clave stock tuning** is 1768 Hz, not 2.5 kHz: a one-line fix in `res/voices.py:100` plus regenerating the header and panel. (Clave 1)
2. **Snare noise** is 17 to 23 dB too quiet: raise the noise gain and rebalance. (Snare 1)
3. **Scale noise by `sqrt(fs / 48000)`** so Clap, Maracas, Snare and Tom noise sound the same at every sample rate. (All modules 1)
4. **Compensate the trigger pulse** so STRIKE and tuning stop changing the volume, and make STRIKE soften the edges instead of widening the pulse. (All modules 2 and 3)
5. **Cowbell: envelope before the band-pass**, as in the circuit; this also removes the click. (Cowbell 1)
6. **Maracas GRAINS**: guarantee beads in every hit and make the rate per second, not per sample. (Maracas 1 and 2)
7. **Level-compensate Clap BAND/RES, Cowbell RES and the Tom ranges.** (Clap 1, Cowbell 2, Tom 1)
8. **Read accent over a 0.5 ms window** after the trigger, and use one accent law for all voices. (All modules 5 and 4)
9. **Kick: check the attack jump against the schematic** (about 1.5 octaves by my recollection) and let it glide down; remove the sharp offset in sustain. (Kick 1, 2 and 3)
10. **CPU: update knob-derived values at control rate and skip silent voices**, for roughly a third of the cost; add a DC blocker after DRIVE and zero denormal states while in that code. (All modules 6, 7 and 8)

Also worth doing while the Snare is open: make the stock-row TONE the shell balance, as on the real panel (Snare 2), and give the Clap's last burst its longer tail (Clap 2).

---

## How the measurements were made

- The probe program includes the engine headers unchanged and renders one voice at a chosen sample rate, with knob overrides, to a raw file; Python with numpy does the analysis. Everything is in `/tmp/claude-0/-home-user/c9a435a6-5f50-51ef-8449-7ae34720aab5/scratchpad/fable-review/` (`probe.cpp`, `pr.py`, `m1.py` to `m7.py`, `nan.cpp`). That folder is temporary.
- "Peak" is the largest absolute output sample. "RMS" is over the stated window. Pitch is from interpolated upward zero crossings of the resonator's band-pass state. Aliasing is the power below 20 kHz that is not within a narrow band around a harmonic, relative to total power.
- `make test` equivalents were built into the scratch folder and all existing checks pass. I did not listen to anything; every statement about how something sounds is inferred from the numbers.
- Not measured: zipper noise (stated from reading the code), behaviour inside Rack itself (the glue was reviewed by reading only), and any comparison with real TR-808 recordings, which were not available here.
