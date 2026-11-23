# Comper Fable

A jazz piano comping player for VCV Rack 2. It plays chords the way a pianist accompanies: rootless,
shell, quartal and spread voicings that move by the smallest steps from chord to chord, swung two-bar
rhythm figures that push into the next chord, rolled and weighted hands, and a LISTEN input that leaves
room for a soloist and answers in the gaps. Notes out only: patch it into a piano (Joanna, in this
repository, is the intended partner) or any polyphonic voice.

- 36 HP. Poly V/OCT, GATE, VEL out (up to 16 channels), PEDAL, SCALE, BASS V/OCT and GATE, CHANGE, EOC
- Chords from a built-in 16-step chart (14 progressions, or type one), from ROOT and QUAL CV, or worked out from a poly chord input
- Grooves: swing, bossa, ballad, soul and house sixteenths. 4/4 only
- LOOSE knob from sequencer-tight to drunk, with TIMING, ROLL, WEIGHT and SLOP trims
- 12 presets in Rack's preset menu
- Manual: [docs/Comper-Manual.pdf](docs/Comper-Manual.pdf). Design: [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md)

Designed by Fable (the reference engine `tools/proto/comper_ref.py` defines what it plays), built by Opus.

## Build

    make RACK_DIR=path/to/Rack-SDK dist

Offline checks (the C++ engine must reproduce 99 golden takes from the reference engine note for note) and
MIDI renders of every preset, no Rack needed:

    make test

Panel: `python3 res/make_panel.py` (needs `pip install fonttools`). Presets: `python3 tools/proto/export_presets.py`.
Golden cases: `python3 tools/proto/make_reference.py && python3 tools/golden_to_h.py`.

GPL-3.0-or-later (see LICENSE.txt).
