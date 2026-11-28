# Cubit

8-bit wavetable oscillator, modulator and pattern source (26 HP, up to 16 voices). Design spec:
`vcv-modules/cubit.md` in the project files.

- **The clock is the pitch.** A clock at 64 × the note steps a 6-bit counter through a 32 KB ROM of
  512 single-cycle waves (64 bytes each). Every byte is held until the next clock edge, no interpolation,
  so the hard staircase and its images above the note are the sound. Rendered exactly (quadratic
  B-spline over the staircase), so nothing inharmonic folds back.
- **A / B / C** buttons with 7-segment digits pick the wave in an 8 × 8 × 8 cube; TRIG inputs step them,
  CV inputs (1.25 V per index) scan them, RESET returns A to 0.
- **RANGE LO** divides the clock by 256 (5 to 500 steps a second): bank C 7 then holds drum patterns
  (TAP 1–4 = kick, snare, hat, perc) and arps / bass lines (UNI is byte / 48 V, so every note is in tune).
- Outputs: ±BI, UNI, SUB (an octave down), four bit taps (bits 0, 3, 5, 7 as gates), CLK OUT (64 × note).
- Bends: **SPLICE** (a counter line wired to an address line: two waves alternate in blocks), **STUCK** bit
  LO / HI, **RES** (64 down to 4 steps per cycle), **DEPTH** (8 down to 1 DAC bits), **DRIFT**.
- SYNC with sub-sample timing, CLK IN replaces the internal clock, Expo and Lin FM (through-zero in the menu), VCA.
- **MUTATE + LOCK**: wave, splice, stuck bit, resolution and depth offsets per MUT trigger; LOCK keeps them,
  hold MUT 1 s to clear. Pitch and level are never touched.
- Menu: load your own ROM (.bin / .rom of 32768 bytes, or any WAV cut into 64 / 256 / 2048-sample frames),
  export the current ROM, back to the factory ROM. A loaded ROM is saved in the patch.

Offline checks (spec section 12):
`cd test && g++ -std=c++11 -O3 -funsafe-math-optimizations -I../src render.cpp -o render && mkdir -p out && ./render out && python3 check.py out`
