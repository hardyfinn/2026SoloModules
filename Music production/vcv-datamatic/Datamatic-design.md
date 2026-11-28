# Datamatic: Ryoji Ikeda style generator (design proposal, 2026-11-22)

## What it borrows from Ikeda
- **Pure tones, hard cut**: sines with no envelope, so the on/off edges are part of the rhythm. Very low (30-60 Hz) and very high (8-18 kHz) zones.
- **Single-sample clicks** and gated white noise bursts as the "drums".
- **Data as score**: numbers (pi, primes, counting, Fibonacci, text) turned into bits; the bits are the gates (as in *test pattern* and *datamatics*).
- **Micro-rhythm**: grid rate jumps 1x, 2x, 4x, 8x, 16x; ratchet rolls that speed up into a tone.
- **Interference**: L and R sines a few Hz apart, so they beat; hard panning.
- **Digital silence**: sudden full cuts as structure.

## Module: Datamatic (one module, about 20 HP, MIT, plugin 2.0.0, Hær logo, 3.2 mm / 2.9 mm labels)
**Data section**
- SOURCE (8 steps): COUNT, PRIMES, FIB, PI, E, TEXT, RANDOM (seeded), INPUT (samples a CV into 8 bits)
- WORD: offset into the data (scrolls the pattern)
- LENGTH: loop length 4-64 bits (64 = never repeats on short scales)
- RATE: 1x / 2x / 4x / 8x / 16x of the clock (16ths up to 256ths)
- MUTATE: chance per bar of flipping bits (Autechre-ish drift)

**Voices** (each reads its own bit lane from the same data, so they interlock)
- SUB: pitch 25-120 Hz, LENGTH (fraction of the step)
- TONE: zone LOW / MID (1 kHz test tone) / HIGH, pitch, BEAT (L/R detune 0-20 Hz)
- CLICK: width 1-16 samples, PAN spread (alternates L/R by bit)
- NOISE: length, colour (white to thin hiss)
- Level knob per voice

**Structure**
- ROLL: chance a step turns into an accelerating ratchet
- CUT: chance a half bar drops into total digital silence
- A small barcode display shows the bits scrolling

**Jacks**: CLOCK, RESET, CV for SOURCE, WORD, RATE, MUTATE, ROLL, CUT, INPUT data; outs L, R, 4 voice gates, 4 voice audio outs, DATA CV (the current byte as 0-10 V).

## Variations
- **B, two modules**: Datamatic (data sequencer, gates and CV only) + Test Pattern (the four voices). More patchable, more panel space.
- **C, add a FORMULA/TEXT mode**: type a short word or number on the right-click menu and it becomes the pattern.

## Recommendation
A, one module, with the voice gates and DATA CV out so it can still drive other modules (Boohm, 808 Metal, Symbolic).

Prototype demo: datamatic-prototype-demo.wav (Python sketch of the engine, 132 BPM, 24 bars: pi, then primes, then counting, rate climbing 1x to 8x, rolls and cuts).
