# Boohm DSP review (Fable, 2026-11-22)

Review of src/BoohmDSP.hpp and src/Boohm.cpp at 2.0.0. Measurements from offline renders at 48 kHz unless stated.
Headline: global oversampling not warranted; aliasing is fine except the GRIT stepped table and FM-2 at high ratios.

## Must-fix
1. Click level unbounded and sample-rate dependent (peaks 13 V alone, 26.8 V with COLOR=1; quieter at 96/192 kHz; top of TRS TONE dead). Fix: TPT SVF, noise scaled by sqrt(sr/48k), gentler bright mix, tick 1.0, bounded click level.
2. Voice cut off hard at LENGTH (step of up to 2.7 V when the click outlasts the body). Fix: 5 ms end fade (30 ms on MEMB), declick on deactivation; zero outputs on the inactive path.
3. TUBE folds back and adds DC. Fix: tanh(g*x + b) - tanh(b), plus an 8 Hz DC blocker on the output.

## Should
4. Sub not mono at STEREO=1 (side -14..-17 dB below 120 Hz). Fix: mid/side from the phase offset, side high-passed at 180 Hz.
5. CURVE is one exponential, no 909 "hold then fall"; 909 settings land ~55 Hz instead of 40. Fix: sw = exp(-(t/tau)^n), n 0.8..1.8, depth reduced toward 909.
6. MEMB room: sample-rate dependent, zipper when TRS DECAY moves, stale tails replay, 4.7 dB quieter. Fix: sr-scaled delays and damping, latch size per hit, clear buffers, body x1.6.
7. Controls latched on the trigger sample (sequencer CV can arrive a sample later); 8-sample steps on level controls. Fix: 1 ms hit delay; per-sample smoothing of SUSTAIN/VEL/FX/STEREO.
8. FX loudness unmatched (SOFT +9.8 dB). Fix: wet *= 1/(1+a*d), a = TUBE .47, SOFT .75, HARD .56, FOLD .45, BASS .25; CRUSH hold scaled by sample rate.
9. FM-2 aliasing at high ratios (+1.3 dB at ratio 15). Fix: cap the index by available bandwidth; softer square, polyBLEP saw modulator.

## Nice-to-have
10. GRIT stepped table aliases (-26 dB at 700 Hz). Fix: band-limited mip levels.
11. CPU ~0.7-1.1% of a core at 48 kHz; many per-sample pow/exp calls. Fix: control-rate prepare, recursive envelopes.
12. Smaller: click envelope applied twice (decay half the stated time); gate-hold level depends on LENGTH; rotation-form membrane resonators; denormal flush on declick; dead code.
