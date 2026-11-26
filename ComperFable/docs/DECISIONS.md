# Build decisions (Opus, 2026-11-23)

Things the build had to decide that BUILD-INSTRUCTIONS.md did not, or where the module differs from it.

1. **External chord change releases at once.** In `simulate()` an external (CV or POLY) chord change releases held chords with the normal rule, which keeps a chord struck less than 0.9 beat earlier sounding over the new chord. The host releases everything immediately instead (`Host::releaseNow`). Chart mode is unchanged and still matches the golden takes. Fable: please confirm or change the reference.
2. **"Current slot holds a figure hit" in real time.** A change rarely lands on a slot. The host skips the catch-up strike when the next slot is less than 0.15 beat away and holds a figure hit, or a hit landed in the last 0.15 beat. The strike uses the most recently decided slot for its beat.
3. **CV chords also need to be stable**, for 2 ms (POLY: 3 ms as specified), so ROOT and QUAL arriving a sample apart are one change, not two.
4. **Decisions are timed in exact seconds**, not sample times: a decision at the first sample past its moment computes the slot's nominal time from the beat position, so note times are not quantised to the decision sample. On-time error against the golden takes is at most one sample.
5. **Same-pitch overlaps.** Staggered releases often leave a note sounding when the next chord strikes the same key. The spec's retrigger rule (gate down, up again 2 ms later) applies; in the 88 host-checked takes 469 of 6,985 notes (7%) arrive 2 ms late for this reason. Musically harmless; reported because it is more frequent than expected.
6. **Notes before beat 0.** After a reset, the first chord can want to land a few milliseconds early. In Rack it is clamped to the reset moment. The test harness starts the host half a beat early instead so the golden comparison is exact.
7. **BEATS** is a four-position switch (1, 2, 4, 8) on a knob; **NEW** sets SEED to a random 0 to 999.
8. **Tags**: "Clock" is not a Rack tag; used Sequencer, Polyphonic, Random, Clock modulator.
9. **Chart typing** accepts extra spellings beyond `parse_chart`: maj9, maj (6), 69, -7, -6, -7b5, mM7, o, o7, alt, 7#5, sus4, 7sus4, `-` (m7). Lower-case root letters are accepted.
10. **Engine compile flags.** The tests build without `-funsafe-math-optimizations`; the golden comparison also passes with `-O3 -funsafe-math-optimizations` (checked), so the Rack build plays the same takes.
11. **Demos** are rendered through Joanna's engine (preset "Late set (jazz club corner)"), driven by the host's outputs exactly as Joanna's module reads its jacks, with a quiet General MIDI band from `tools/proto/sketch_band_v0.py` for context.

# Walker build decisions (Opus, 2026-11-24)

Things the Walker build had to decide that WALKER-BUILD-INSTRUCTIONS.md did not, or where the module differs from it.

1. **CPU economies.** The first port cost 8.5% of one core for the 208 bpm line (target 6%). Two changes, both within the sound check:
   - The rotating phasors carry on from one 16-sample segment to the next instead of being rebuilt from the exact phase every segment (one sincos per component per segment instead of two), renormalised every 1,024 samples against rounding drift. This was most of the saving.
   - A burst partial is dropped, from the top down, once it is 100 dB (1e-5) under its own start, as section 5 suggests. The 1e-4 variant measured against the loudest partial was tried first: it moved some high-frequency cells of the sound check by up to 2 dB, so it was not used.
   The control rate was already 16 samples; `KB` is not capped. Result: 4.4% of one core on the build machine (x86, `-O3 -funsafe-math-optimizations`), the sound check's worst cell 0.02 dB, mean 0.00 dB.
2. **The clock code is copied, not shared.** Section 4.3 asks for Comper's clock follower as a shared helper. Taking it out of `Host.hpp` would touch the path Comper's 99 golden takes run through, so `WalkerHost::cableTransport` is a faithful copy instead (same median of three, snap and hold). Worth folding together in a later tidy-up.
3. **Stale link.** Comper stamps every message; Walker treats the link as gone when the stamp has not moved for 3 samples (Comper bypassed or deleted). Comper's `resetCount` goes up in `Host::doReset` (first start and the RESET jack). A Comper seed change does not reset Walker: Walker's own SEED is separate.
4. **Running.** Walker counts Comper as running only when Comper is running and not waiting for its first clock pulse after a reset.
5. **Six voices.** No voice was stolen in any of the 12 preset trios, the slow ballads included (`tools/render_trio.cpp` prints the count). Six is enough.
6. **LEAD at slow tempos.** In the host test the ballad cases (62 bpm) and the AHEAD cases match the golden releases within 1.5 samples, so no release had already happened when `perform()` changed it. Nothing seen to suggest LEAD is too short.
7. **Tests.** 4e counts blind notes up to half a beat before the end of the first pass, because the first learned note is played ahead of the beat (AHEAD). Check 5 counts repeated pitches between consecutive walking notes only; a repeated-note skip is one of the designed skips and still happens at ACTIVITY 0 now and then (the reference does the same). 4a runs 33 beats.
8. **Not checked by ear.** The soul and house cells, and the 4 ms late decision in cable mode, behave as specified in the tests; they have not been listened to by Hardy yet.
9. **Demos** (`tools/make_walker_demos.py`): Comper through Joanna with Comper's ROOTS off (as with Walker attached), Walker linked sample by sample exactly as the modules are in Rack, General MIDI drums for context (none on the ballads). Trios are balanced to equal bass and piano RMS with drums at 0.4. The ACTIVITY, LOOSE, ATTACK and tone tours keep the Medium Swing trio's gains so level changes from the controls can be heard.
10. **Panel.** NEW is a button next to SEED in the PLAYER row; the PLUCK and LINK lights sit at the end of the HANDS row. The panel is drawn by `res/make_walker_panel.py`.

# Rider build decisions (Opus, 2026-11-24)

Things the Rider build had to decide that RIDER-BUILD-INSTRUCTIONS.md did not, or where the module differs from it.

1. **Display counters.** The display needs to tell a big fill from a small one and see a bomb. The engine gained `lastFill` (the size `pickFill` returned in this plan) and a `bombs` counter in `Stats`. Neither draws a random number, so the 174 golden parts are untouched.
2. **Beat tolerance.** Section 10 asks for beats within 1e-9 in the linked and clock tests, but the golden file stores beats rounded to 7 decimals, so the tests use 1e-6 (as the player test does). Voices, velocities and stroke order are exact. The behaviour tests compare the host with itself and use 1e-9.
3. **Log order.** The host logs strokes as they are played; a stroke with an early offset can be played before one planned ahead of it. The tests sort the log by plan order before comparing with the golden case.
4. **Stopping.** While Comper is stopped the queued strokes are kept but not played, and play on when it runs again; a reset clears them.
5. **Cable clock.** `RiderHost::cableTransport` is a copy of Walker's (median of three periods, snap to the pulse, hold at 1.5 pulses), as Walker's was a copy of Comper's. Phi advances after the bar is planned and the strokes fired, so the first pulse is exactly beat 0.
6. **Resampling.** At rates other than 48 kHz the kit's stream is read with 4-point Hermite interpolation between the two middle points of the last four kit samples: one kit sample more delay than at 48 kHz, where the kit's sample is used directly. Over an 8-bar take the output energy is within 0.05 dB of the 48 kHz render at 44.1 and 96 kHz.
7. **Cost.** 2.4 to 2.5% of one core at 48 kHz on preset 02 (x86 build machine, `-O3 -funsafe-math-optimizations`), with the 104 band filters always running. No economies were needed.
8. **Panel.** 30 HP. ENERGY is the large knob. LOOSE is a normal knob (it sits in the HANDS row under the PLAYER captions). The lights HIT, LINK, FILL end the HANDS/KIT row; the six sound knobs are spread across the third row; inputs are spread across the top jack row and the ten outputs fill the bottom row. Drawn by `res/make_rider_panel.py`, preview by `tools/preview_rider.py`.
9. **The module struct is `RiderModule`** (the engine is `rider::Rider`); the model slug is `Rider`.
10. **Link.** `LinkPort` uses `Module::Expander`; a neighbour that is not Comper, Walker or Rider is ignored. More than one of a kind in a row does not crash: each section is taken from the nearest side that has it.
11. **Demos** (`tools/make_rider_demos_cpp.py`): all three hosts linked sample by sample in `tools/render_trio.cpp`, with Walker's off-beat notes reaching Rider and BAND reaching Comper (density, velocity) and Walker (activity, level). Not checked by ear by the builder.
12. **Output knee.** At kit x 18 a single loud stroke stays under about 8 V, but a big fill at high ENERGY can pile up to 17 V for a few milliseconds (preset 02, bar 16). The host leaves everything below 8 V untouched and rounds off above it towards 12 V (`RiderHost::knee`). In the 12 preset takes it touches fewer than 50 samples per take. The kit test checks the kit itself, before the knee.
