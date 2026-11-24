# Comper Fable

Two modules: **Comper**, a jazz piano comping player, and **Walker**, its upright bass player.

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

## Walker

An upright bass player for Comper. Put it next to Comper and it follows with no cables: walking, two-feel, ballad,
bossa, soul and house lines that land on each chord and lean into the next, with human hands that sit ahead of the
beat, played on a physical model of an upright bass fitted to recordings (a real finger pluck, a ringing body,
fingerboard growl). Audio out, and the notes as V/OCT, GATE and VEL. It can also follow chord cables from any
sequencer (it learns the tune on the first pass), or be played as an upright bass voice from GATE IN.

- 28 HP. 12 presets, paired with Comper's
- When Walker sits next to Comper, Comper stops playing roots (menu option to keep them)
- Design: [docs/WALKER-BUILD-INSTRUCTIONS.md](docs/WALKER-BUILD-INSTRUCTIONS.md); the line is `tools/proto/walker_ref.py`, the sound `tools/proto/upright_ref.py`
- Walker chapter in the manual; build notes in [docs/DECISIONS.md](docs/DECISIONS.md)

## Build

    make RACK_DIR=path/to/Rack-SDK dist

Offline checks (the C++ engines must reproduce 99 Comper takes and 97 Walker lines from the reference engines note
for note, and the upright must match the reference sound in 62 cases), MIDI and WAV renders of every preset, no Rack needed:

    make test

Walker panel: `python3 res/make_walker_panel.py`. Demos: `python3 tools/make_walker_demos.py OUTDIR` (needs the Joanna folder next to this one).

Panel: `python3 res/make_panel.py` (needs `pip install fonttools`). Presets: `python3 tools/proto/export_presets.py`.
Golden cases: `python3 tools/proto/make_reference.py && python3 tools/golden_to_h.py`.

GPL-3.0-or-later (see LICENSE.txt).
