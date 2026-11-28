# Greybox

One-pad 12-bit sampler voice in the manner of the late-80s rack samplers and the
drum-machine sampler of the same years, picked with a 3-position **MODEL** switch
(900 / 950 / 60). Stack copies for a kit. Sibling of Crate. Design spec:
`vcv-modules/greybox.md` in the project files.

- **900 / 950**: pitch is the speed of the DAC clock. Every stored word plays once, in order,
  so tuning up never aliases and tuning down slows the clock, pulling the stair-step images
  into the audible band where a 36 dB tracking filter decides how much you hear. The 900's
  input filter is leaky (fold-back), the 950's is sharp and goes to 48 kHz.
- **60**: companded 12-bit words (cleaner quiet tails, grainier peaks), fixed 40 kHz clock,
  linear interpolation through memory, digital level, no VCA saturation.
- **RATE** sets the sampling rate (7.5 kHz up to 40 or 48 kHz), non-destructively; pitch and length stay put.
- **FILTER** + **ENV**: 6th-order Butterworth voice filter that follows pitch (menu), closes with the
  envelope, and on 900 / 950 grits up and whistles at low cutoffs like a switched-capacitor filter.
- **STRETCH** + **D-TIME**: the old cyclic time-stretch with its seam buzz (centre = off).
- **LOOP** off / forward / alternating. Load a WAV from the menu or record IN with REC.
- **THRU** runs live input through the converters; RATE becomes a live sample-rate reducer.
- **AGE**: worn DAC bits, clock and filter drift, hiss, hold droop, memory rot past 60 %.
- **MUTATE + LOCK**: tune, decay, start, filter and reverse offsets per MUT trigger; LOCK keeps them,
  hold MUT 1 s to clear.

Offline DSP checks and demo renders: `cd test && g++ -O2 -std=c++17 -I../src render.cpp -o render && ./render checks .`
