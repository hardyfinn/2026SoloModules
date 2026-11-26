# Comper Fable

Three modules: **Comper**, a jazz piano comping player, **Walker**, its upright bass player, and **Rider**, its drummer.

A jazz piano comping player for VCV Rack 2. It plays chords the way a pianist accompanies: rootless,
shell, quartal and spread voicings that move by the smallest steps from chord to chord, swung two-bar
rhythm figures that push into the next chord, rolled and weighted hands, and a LISTEN input that leaves
room for a soloist and answers in the gaps. Notes out only: patch it into a piano (Joanna, in this
repository, is the intended partner) or any polyphonic voice.

- 38 HP (2.3.0). Poly V/OCT, GATE, VEL out (up to 16 channels), PEDAL, SCALE, BASS V/OCT and GATE, CHANGE, EOC
- Chords from a built-in 16-step chart (14 progressions, or type one), from ROOT and QUAL CV, or worked out from a poly chord input
- Grooves: swing, bossa, ballad, soul and house sixteenths. 4/4 only
- LOOSE knob from sequencer-tight to drunk, with TIMING, ROLL, WEIGHT and SLOP trims
- 12 presets in Rack's preset menu
- Manual: [docs/Comper-Manual.pdf](docs/Comper-Manual.pdf). Design: [docs/BUILD-INSTRUCTIONS.md](docs/BUILD-INSTRUCTIONS.md)

Designed by Fable (the reference engine `tools/proto/comper_ref.py` defines what it plays), built by Opus.

## What's new in 2.4.0 (Walker)

- **Panel:** Walker is 36 HP (was 28) with full-size labels (3.2 mm controls, 2.9 mm jacks).
- **OCTAVE:** moves the bass up 0, +1 or +2 octaves, on the audio and the V/OCT output. The line itself does not change.
- **LENGTH:** every note shorter (0.3x) or longer (up to 3x, legato). The default (1.4x) is longer than 2.3.0's. A lengthened
  note still ends when the next one speaks, so the bass stays one voice.
- **VARY:** a mix of short and long notes. Up, more notes are held long (mostly on the beat) and clipped short (mostly off it).
  0 keeps every note at the same length. Turning LENGTH or VARY never changes which notes are played or when.
- Old patches load with the new knobs at their defaults.

## What's new in 2.3.0 and 2.3.1

- **Panel:** 38 HP with full-size labels (3.2 mm controls, 2.9 mm jacks). The 32 step knobs are gone: the chart is a strip of 16 steps
  along the bottom of the screen. Tap a step, then set it with **EDIT ROOT** and **QUAL** in the CHART block. The playing step has a dot.
  Old patches keep their charts (the step settings are still stored, just not on the panel).
- **Progressions on the screen** (2.3.1): the top line of the screen is a progression bar. Tap the arrows to step through all 44
  progressions, or tap the name for a list by genre. A `*` means the chart has been changed since it was loaded.
- **PRESET knob** (2.3.1, BAND section): turn it to load one of the 12 factory presets (the knob's tooltip names them).
- **OCTAVE:** -3 to +3 octaves on the chord outputs (the bass outputs stay put).
- **LENGTH:** short to legato. Notes now have a pianist's lengths: the top note rings longest, inner voices lift first, accents sit
  longer, ghost notes are clipped, each groove has its own touch, and the touch drifts slowly so a phrase stays legato or detached.
  SLOP and LOOSE set how much it varies. The default is longer than 2.2.0's.
- **AUTRE:** the OUT knob and its CV input are renamed AUTRE (same job).
- **Grooves:** a sixth groove, **Reggae** (straight eighths, chords chopped on the offbeat and on 2 and 4). Walker plays its soul line
  and Rider its soul pattern under it.
- **Chords:** plain major and minor triads (type `C`, `Am`, or use the QUAL knob's last two settings). In a typed chart a bare letter is now
  a triad (it was a 6 chord before).
- **Progressions:** 30 more, grouped by genre in the right-click menu: House (11), Gospel (2), Blues (2), Disco (4), Soul (4), Funk (3),
  Reggae (4), on top of the 14 jazz ones.
- Licence is now MIT.

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

## Rider

A jazz drummer for Comper and Walker. Put the three in a row (any order) and they play as a trio with no cables:
Rider keeps time on the ride, comps on snare and bass drum, catches the pianist's pushes (it is told Comper's rhythm
figure for each bar just before it starts), keeps clear of Walker's off-beat notes, and sets up the ends of phrases
and the top of each chorus. Swing, bossa, ballad (brushes), soul and house. It plays a modelled jazz kit fitted to
recordings (partials, a noise wash in 26 bands, the stick click), stereo out, plus six triggers and their velocities.
A BAND knob lets its ENERGY lead Comper and Walker. It also runs alone from a clock.

- 30 HP. 12 presets, paired with Comper's and Walker's
- Design: [docs/RIDER-BUILD-INSTRUCTIONS.md](docs/RIDER-BUILD-INSTRUCTIONS.md); the player is `tools/proto/rider_ref.py`, the kit `tools/proto/rider_kit_ref.py`
- Rider chapter in the manual; build notes in [docs/DECISIONS.md](docs/DECISIONS.md)

## Build

    make RACK_DIR=path/to/Rack-SDK dist

Offline checks (the C++ engines must reproduce 99 Comper takes, 97 Walker lines and 174 Rider drum parts from the
reference engines note for note, the upright must match the reference sound in 62 cases and the kit in 69), MIDI and WAV renders of every preset, no Rack needed:

    make test

Rider panel: `python3 res/make_rider_panel.py`. Rider demos: `python3 tools/make_rider_demos_cpp.py OUTDIR`.
Walker panel: `python3 res/make_walker_panel.py`. Demos: `python3 tools/make_walker_demos.py OUTDIR` (needs the Joanna folder next to this one).

Panel: `python3 res/make_panel.py` (needs `pip install fonttools`). Presets: `python3 tools/proto/export_presets.py`.
Golden cases: `python3 tools/proto/make_reference.py && python3 tools/golden_to_h.py`.

MIT (see LICENSE.txt). Changed from GPL-3.0-or-later in 2.3.0: the code is all original.
