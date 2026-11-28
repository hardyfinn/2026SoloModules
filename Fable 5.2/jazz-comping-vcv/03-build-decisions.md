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
