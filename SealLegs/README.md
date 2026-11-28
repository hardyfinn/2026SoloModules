# Seal Legs

Stereo character delay: two delay lines, three delay **MODELS** (TAPE / BBD / DIGI), a MOD section,
filters inside the feedback loop, drive, noise, a plate reverb ("FOG") and freeze. 40 HP.
Design spec: `vcv-modules/seal-legs.md` in the project files.

- **Position-buffer lines**: speed changes re-pitch what is already in the loop, like tape or a clocked BBD.
  **CHANGE** picks GLIDE (pitch bends while the time moves) or JUMP (crossfade, no pitch change).
- **TAPE**: saturating record chain with pre/de-emphasis, darker at long times, head bump on the output,
  COLOR adds drag and dropouts. **BBD**: 512 / 8192 / 32768 stages by RANGE, clock-tracking filters,
  2:1 compander, audible aliasing at long times, COLOR opens the filters and shakes the clock.
  **DIGI**: clean, COLOR crushes the sample rate and bits.
- **STEREO** PONG / T:W / L/R, **FB X** crosses the feedback, FB to 160 %.
- **MOD**: wow & flutter, envelope −/+, then random > S&H > square > sine > saw > triangle > ramp (morphing), sync, MOD OUT.
- **FREEZE**: TAPE / BBD keep the loop running (time and mod still bend it); DIGI holds the buffer exactly.
  **T-STOP** brakes a frozen TAPE loop to a halt and spins it back up on release.
- **REVERSE** off / rev / mix, **SPEED** ×½ / ×1 / ×2 (octave per repeat), **DUCK**, **DRIVE** at input,
  delay send or output, **NOISE** pre hiss > crackle > post hiss, **FOG** at input, delay, mix or inside the loop.
- **AUX** CV to drive, noise, mod shape, fog or mix. **MUTATE + LOCK** never touch time, width, feedback, mix or drive.

Offline checks (spec section 14): `cd test && g++ -O2 -std=c++17 -I../src render.cpp -o render && ./render checks [loop.wav]`
