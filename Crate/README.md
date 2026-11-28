# Crate

One-pad 12-bit sampler voice in the manner of the classic late-80s hip-hop drum sampler.
Stack copies for a kit. Design spec: `vcv-modules/crate.md` in the project files.

- Memory is 12-bit at a fixed 26.04 kHz. Pitch is a step size through memory with
  **no interpolation**, so tuning down leaves stair-step images and tuning up folds aliases back down.
- **SPEED 33 / 45 / 78**: capture the source sped up, then play it back down to pitch, the
  "sample at 45, tune down" trick. Grittier at the same pitch and length.
- **CHANNEL**: 1·2 lowpass that closes as the sound decays, 3·4 and 5·6 fixed lowpasses, 7·8 unfiltered.
- Load a WAV from the context menu, or record IN with REC (button or gate). GAIN drives the capture
  clipper; GAIN and SPEED are non-destructive (capture re-runs from the kept source).
- **THRU** runs live input through the converters and channel filter at original pitch.
- **AGE**: worn DAC bit weights, slow clock and filter drift, hiss, and memory bit rot past 60 %.
- **MUTATE + LOCK**: each MUT trigger draws new tune, decay, start and channel offsets
  (scaled by MUTATE); LOCK keeps them; hold MUT for 1 s to clear.

Offline DSP checks: `cd test && g++ -O2 -std=c++17 -I../src render.cpp -o render && ./render .`
