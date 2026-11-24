# Essphor: research notes

Fable, 2026-11-24. The full proposal Hardy read is a Claude Doc: https://claude.ai/artifact/FxvRLt9nSpAdCKZaXeqcPc. This file keeps the facts the build needs.

## What is being modelled

The Torso Electronics S-4 has four stereo tracks, each a fixed chain of five device slots: Material (tape, disk or poly sampler), Granular (MOSAIC), Filter (RING), Color (DEFORM), Space (VAST). Essphor follows MOSAIC, Essphor Res follows RING, and Essphor Mod follows the four modulator types (WAVE, RANDOM, FOLLOW, ADSR). The modules are not affiliated with Torso Electronics and carry none of its names.

## What the maker documents (docs.torsoelectronics.com/s4, read 23 and 24 November 2026)

MOSAIC, after firmware 2.2 (3 June 2026): "rhythmic granular processing with 64 grains" in a 4 second buffer. PITCH (scales; a review gives ±36 semitones). RATE 4/1 to 1/64 or FREE. SIZE in beats. CONTOUR: sine at centre, downward ramp negative, upward ramp positive. SCAN modes STRETCH (read head slower than write head), POSITION (fixed, can be modulated), DELAY SYNC, DELAY FREE. SPRAY modes RANDOM, RANDOM MONO, WARP, WARP MONO. PATTERN: a fixed pseudo-random pitch sequence, unique per instance. WET modes MIX and SEND. FEEDBACK modes GRAIN OUTPUT and SOS. RAND PITCH modes DETUNE, RANDOM, RANDOM MONO. RAND RATE, RAND SIZE, RAND AMP, RAND REV. LOCK freezes the buffer and disables feedback.

RING: "morphing, resonant 48-band filterbank". CUTOFF, RESONANCE, DECAY ("increases the resonance of individual bands to create ringing frequencies"; SUSTAIN applies it post-filter, CHOKE pre-filter so CUTOFF can filter the ringing out), PITCH −24 to +24 semitones, SLOPE (morphs low-pass, band-pass, high-pass), TONE (tilt), SCALE (quantises band positions; CHROMATIC is the default and "replicates the incoming signal"), WET.

Modulators: WAVE (RATE with FREE, STRAIGHT, TRIPLET, DOTTED; AMOUNT, PHASE, OFFSET, SKEW, FOLD, CURVE, SPREAD; sine, triangle, square). RANDOM (RATE, AMOUNT, PHASE, OFFSET, LENGTH, VARIATION, SMOOTH, SPREAD). FOLLOW (DELAY, AMOUNT, RISE, FALL, INPUT, FILTER, HOLD, SPREAD). ADSR (ATTACK, DECAY, SUSTAIN, RELEASE, three curve controls, SPREAD).

Scales: the nineteen in the build instructions plus FREE are the maker's list.

## What is not documented, and so was designed here

Ranges of almost every control; the contents of the pattern; the WARP drift; the level law for overlapping grains; how grains avoid the write head; how the 48 bands are laid out and how they sum; the ring-time law; everything about AGE (the hardware has no such control). The band layout in particular is a guess with a reason: the filter is taken to be the band gains themselves, which is the one reading that makes SUSTAIN (gains at the band inputs) and CHOKE (gains at the band outputs) fall out naturally.

The PDF manual could not be fetched. One source conflict: Sound On Sound (March 2025) says up to 128 grains, the maker's page says 64; the maker is followed.

## Checked how

By measurement on the Python references only (`tools/proto/check_*.txt`). Nobody has listened to the sketches yet.
