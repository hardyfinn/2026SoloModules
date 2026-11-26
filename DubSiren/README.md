# Dub Siren

A VCV Rack model of the classic UK sound-system transistor siren: the NJD SE-1
circuit, as rebuilt in boxes like the Rigsmith NS1. It follows Zack Nelson's
redrawn NJD schematic part by part, then adds an echo and a few extras.

24 HP. Plugin version 2.0.0, MIT licence.

## Controls (same as the hardware box)

| Control | In the circuit | What it does |
|---|---|---|
| PITCH 1-4 | S5: 470k / 220k / 100k / 68k base resistors | about 150 / 310 / 690 / 1010 Hz at rest |
| MODE 1-4 | S4: what feeds each half of the oscillator | 1 steady + shaped sweep, 2 steady + square (stutter beeps), 3 square + square with D1/D2 boost (hi-lo two-tone), 4 shaped + shaped (full wail) |
| BEAT 1/2/3/OFF | S3: 47k / 22k / 10k LFO resistors | sweep speed slow / mid / fast; OFF stops the sweep with its output low (C5 then drains over ~5 s) |
| SPEED | (Mk4 addition) | scales the BEAT rate 0.25x to 4x |
| TRIG | S6 power switch + C10 | on = the siren runs; off = the supply sags, so it slows, falls and fades |
| SIREN | S1 | pulls the sweep high and charges C5 fast; on release the sweep restarts from the top |
| TONE | S2 + R25 | charges C5 slowly: a rising whoop (fully only with BEAT OFF) |
| FEEDBACK | echo | repeats; fully left = no echo, past ~3 o'clock it runs away |
| SIREN VOL | R30 | output level (not touched by randomize) |
| ACTIVE LED | T5 + LED1 | follows the shaped sweep, as on the box |

## Extras

- DEPTH: how far the modulation pulls away from the supply (1 = as built).
- RELEASE: how long C10 holds the supply up after TRIG goes off (20 ms to 3 s).
- TIME: echo time, 40 ms to 1 s.
- ERA: 80s tape echo (wow, flutter, dark repeats) or 90s chip echo (PT2399-style grit, darker and grittier as the time gets longer).
- Jacks: V/OCT (scales the pitch resistors), SPEED CV (1 V/oct), FDBK CV, CLOCK (restarts the sweep on each pulse), TRIG / SIREN / TONE gates, IN (audio into the echo), OUT.

## Files

- `src/DubSirenDSP.hpp`: the circuit model (runs at 4x the sample rate) and echo.
- `res/make_panel.py`: draws the panel and knobs, writes `src/Layout.hpp`.
- `make test`: renders `test/dubsiren-demo.wav` and prints each PITCH frequency.
- `tools/preview.py`: panel picture.
