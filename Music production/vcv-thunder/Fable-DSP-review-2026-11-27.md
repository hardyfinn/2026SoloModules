# Perun 2.0.0: Fable DSP review (27 Nov 2026)

## High
- H1 Voice stealing hard-cuts the oldest hit (0.565 jump in one sample on long kicks). Fix: 6 slots, logical polyphony 4, fade the oldest.
- H2 RESONANT BD "punch" mode broken: tanh on the state damps it (peak 0.33 vs 0.89, gone in 20 ms). Fix: saturate the output instead.
- H3 Algorithm/mode not latched per hit; A and B share the `first` flag, so moving MORPH mid-hit starts B uninitialised (Resonant silent).
- H4 Aliasing: overdrive (-28 dB), cowbell squares (-26 dB), wavetable square/digital (-26/-28 dB at 220 Hz), pulse hat squares. Fix: polyBLEP, 2x oversampled or ADAA drive.
- H5 Noise-based sounds get 3-6 dB quieter at 96 kHz; fixed one-pole coefficients change with sample rate.

## Medium
- M1 Karplus plays flat (up to -75 cents) - filter delay not compensated.
- M2 CPU: per-sample exp/pow/tan; cache coefficients, skip silent voice stage.
- M3 Delay TIME not smoothed (clicks); delay freezes while SEND is patched.
- M4 No smoothing on LEVEL/CUTOFF/DRIVE/SEND (zipper); TONE jumps at 0.99.
- M5 Accent read on the trigger sample; sequencers may send it 1 sample late.
- M6 Level differences between algorithms (Slap/Pulse hat quiet, cowbell/dust hot).
- M7 Resonance up to +20 dB into the drive, no compensation.
- M8 Sample swap has a small data race.

## Low
- Overdrive bias asymmetry fades at high drive; SAMPLE default attack 25 ms, loop/start clicks, linear interpolation; Karplus buffers cleared on every hit; equal-power morph +3 dB on similar sources; WEAR steals slot before the hit sounds.

## Weakest drums
Resonant punch (broken), Slap (thin, quiet), Pulse hat + cowbell (aliasing grit), Sample (quiet, soft attack), Res Snare (boxy: add HP noise layer, separate body decay), Wavetable vocal (static: sweep formant with envelope).
