# Tapeizer Fable

A VCV Rack 2 module (50 HP) for the sound of cheap but good 1980s and 1990s recording:
a desk channel, a multitrack deck (Fostex 260-style four-track cassette, Tascam-style
eight-track cassette and reel machines), a bus compressor, and a mixdown machine
(cassette deck, boombox, quarter-inch reel or DAT). Companion to Vinylizer.

**STRENGTH** scales every departure from a perfect machine, from fully clean (0) to the
full measured character (1). **AGE** scales the unit-to-unit errors and wear.

- Manual: `docs/MANUAL.md` and `docs/Tapeizer-Manual.pdf`
- Design: `docs/BUILD-INSTRUCTIONS.md`, research in `docs/RESEARCH.md`
- Panel preview: `docs/panel-preview.png`
- 12 factory presets in `presets/Tapeizer/`

## Building

`make RACK_DIR=<path to Rack SDK> dist`. CI builds Apple silicon Mac only.

`make test` builds and runs the offline engine checks (no Rack needed). They compare
the C++ engine against the Python reference in `tools/proto/tape_ref.py`
(frequency response within 0.02 dB, unit numbers exact, latency, dry/wet alignment,
hiss, wow, drop-outs, speed, finiteness at the extremes, no clicks on switching).

## Deviations from the reference

- The desk stage has a 5 Hz DC blocker after the 2x clipper.
- Gaussian noise is approximated from four uniforms (Irwin-Hall), not Box-Muller.
- TRACKS channels are summed onto the tape at unity gain.
- Latency is reported as 116.5 samples at 48 kHz (dry path is delayed to match, so MIX stays phase-aligned).

## CPU

Offline, the full chain runs about 17x real time on one core for stereo, and about 3x for
eight TRACKS at bounce 4 (measured on the build container, not a Mac).
