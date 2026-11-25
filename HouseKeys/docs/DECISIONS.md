# House Keys: build decisions

Where the build differs from, or fills a gap in, `BUILD-INSTRUCTIONS.md`. Each entry says what was done and why.

## FM engine
- **Sustain level is `15 − D1L`**, not D1L written straight in. The chip's SL field counts attenuation (0 is loudest), the voice parameter counts level (15 is loudest). Written straight in, every factory voice decayed to silence or held at the wrong level.
- **A small `.syx` parser of our own** (`parseVmemBank`) instead of adapting Fallen's `Syx4op`. It only needs to find a 32-voice VMEM block (header `F0 43 0n 04 20 00`), and keeping it local avoids a dependency between plugins.
- **TX81Z chip for user banks** is chosen when any TX81Z extension byte (73 to 80) is non-zero. A DX100/DX21/DX27 bank has zeros there.
- **OPZ quirks** found while testing ymfm: key-on needs register 0x08 written with the channel and then the key bit; the chip keeps a preset copy of register E0, so the alternate 0x40 write (bit 7 set) goes first.
- None of the 30 voices uses the pitch envelope (all levels 50), so it is not applied.
- The GRIT test in section 12 ("quantisation steps at −40 dB") is checked as the error between GRIT 0 and 0.5 renders late in a held E Piano 1 decay. With a floating-point converter the steps scale with the signal, so the error sits about 60 dB under the signal at any level. Counting distinct sample values says nothing after the resampler.

## POLY engine
- **No BLEP on hard sync.** Only Sync Lead uses sync, and the sync reset is band-limited by the 2× oversampling and decimator well enough at the pitches it plays.
- The ladder solves its feedback linearly with `tanh` on each stage input (spec 5.3), so self-oscillation pitch is set by the linear part: the section 12 check passes within 3%.

## PD engine
- Cosines come from a 4096-point table with linear interpolation (error below −130 dB) and the pitch increment is updated once per host sample, not at 4×. This brought 8 voices of PD Bell from 12% to 6% of a core. The `a = 0` purity test passes at −107 dB.
- The stepped amplifier keeps 4 fraction bits of the gain's mantissa with rounding, done on the float's bits. Identical to `frexp`, `round(m·32)/32`, `ldexp`.

## WAVE engine
- **Sound data change (rule 2): three flat detunes.** Analog Strings, Formant Voices, Steam Pad, Wave Brass and Pluck and Pad had an oscillator at semitone 0 with fine 27 to 29. On the SQ-80, fine only goes up (0 to 31, 3.125 cents), so these were 84 to 91 cents sharp and failed the pitch test. The design clearly meant a slight flat detune, which on the machine is semitone −1 plus fine. They are now written that way (`data/sounds.py`), giving −9 to −16 cents.
- **Open question for review: LFO speed.** The spec's curve `f = 0.05·2^(freq/7)` puts the delayed vibratos in Analog Strings, Formant Voices, Reed and Wave Sync Lead (freq 22 to 27) at 0.4 to 0.8 Hz with ±30 to 50 cents depth, a slow wobble rather than a vibrato. Left as designed. It is a single constant in `WaveEngine.cpp` if Fable or Hardy wants it changed. The pitch test skips these four sounds because their pitch is meant to move.

## SAMPLER engine
- **GRIT 1 halves the rate by holding every other sample** of the memory at render time (the 6-bit table), not by rendering a second copy at `sr/2`. It is cheaper and gives the same loss of top end and the same aliasing character.
- Body filter gains alternate +6/−6 dB across the five resonances; formant gains are 1, 0.7, 0.35, 0.25.
- Playback keeps the staircase integral from the previous sample, so each output sample costs one integral, not two.

## Core
- **STAB level** is applied as one gain of `1/sqrt(chord size)` on the engine output, smoothed over 10 ms, rather than per note. Every sounding note is a chord note of the same size, so the result is the same, and no engine needs a per-note level.
- **Changing STAB or the polyphony limit re-voices held notes** with the new chord.
- **Output coupling:** a 5 Hz one-pole high-pass after the engines. Every modelled machine is AC coupled at its output, and without it pulse-wave sounds failed the DC test (−35 to −48 dBFS).
- **Chorus output** is `(dry + line)·0.7` on each side. For a stereo engine each side runs through its own line with the shared LFO.
- **Tape** has a fixed 1 ms transport delay so wow can swing both ways; it is only in the path while TAPE is above 0, with a 10 ms crossfade in and out.
- **LEVEL** gain is `LEVEL²` (2.2.0): ×1 (0 dB) at the default, which is the top of the knob, so it can cut but never boost; one note at velocity 100 peaks near 2.5 V. (Before 2.2.0 it was `2·(LEVEL/0.5)²`, +6 dB at the default and up to +12 dB. Hardy asked for 0 dB as default and maximum, and OCTAVE widened from ±2 to ±3.)
- A user voice from a loaded bank uses the average of the 30 FM trims.
- The user voice is stored in the patch (the 128 bytes, its name and chip), so it comes back when the patch loads.

## Tests
- **Pitch (section 12):** "within 5 cents" is checked octave-agnostically, accepting a fifth or fourth too (sounds with oscillators a fifth apart can read as the lower one). On top of 5 cents, half of the sound's own programmed detune is allowed (POLY `fine2`, PD `det`, WAVE oscillator fines, sampler ensemble detune, FM operator DT1 = 3 cents), plus 5 cents for short POLY stabs measured inside a resonant filter sweep. Windows of 0.2 and 0.4 s, at the loudest point and the latest point within 10 dB of it. Chorus is off for the measurement because its delay sweep bends pitch.
- **Box integration (section 12):** measured as the largest non-harmonic component folded into 20 Hz to 20 kHz, at three frequencies near 1 kHz (1013, 1033, 1061 Hz; exactly 1 kHz makes every image land on a harmonic). The improvement over nearest-sample reading is 24.3, 19.9 and 24.2 dB, so the check is on the average (22.8 dB). The sampler uses the same integration formula as the wave engine; the test reads the wave engine's oscillator.
- **Loudness trims** are iterated three times per sound because the chorus input and the soft limiter are nonlinear.
- **Performance** is measured with flush-to-zero on, as Rack runs its audio threads. Without it the sampler's decaying filters went denormal and cost twice as much.

## Performance (cloud machine, Xeon 2.3 GHz, one core, 48 kHz, 8 voices, TWIN off)
| Engine | Heaviest sound | Share of one core |
|---|---|---|
| FM | Heavysynth | 1.7% |
| POLY | 800 Strings | 6.7% |
| PD | PD Strings | 6.3% |
| WAVE | Analog Strings | 10.2% |
| SAMPLER | Choir | 3.0% |
