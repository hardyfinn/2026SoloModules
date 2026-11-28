# Juke

Footwork / juke drum machine for VCV Rack 2 (36 HP), built around 160 BPM and the
3-3-2 kick feel. Nine synthesised voices (kick, clap, snare, closed and open hat, hi and
lo tom, cowbell, vocal chop), each with a trigger out and an audio out, plus a stereo mix.

![panel](docs/panel-preview.png)

## Playing it

- **Grid**: click a step to cycle off, hit, ghost. The grid shows the bar being played.
- **PRESET** (and its CV, 0-10 V across the list) loads one of 14 patterns: the guide's
  Basic, Intermediate, Roll / fill and 3-3-2 vs 4, then Half-time heavy, Juke drive, Tom
  runner, Chop stutter, Cowbell hook, Ghost shuffle, Broken 3-3-2, Clap roll build, Sparse
  intro and Blank. Only a change of preset loads it, so your edits stay put.
- **GEN** writes a new bar in the style. DENSITY sets how busy it is, FEEL picks
  half-time (clap on 9), double-time (claps on 5 and 13) or either. The screen shows the
  seed.
- **MUTATE** makes small changes that keep the style: kicks nudged a step, hits and
  ghosts swapped, percussion rotated, stutters. VARY sets how many changes.
- **FILL** turns the second half of this bar (or the next one, if you're past step 9)
  into a roll: snare, clap, toms or chop.
- **EVOLVE** plays phrases (4 bars, or 8 / 16 from the menu): the base bar, variations,
  then a fill. The base drifts a little every phrase.
- **Mute** buttons on each row silence that lane's trigger and sound.
- **Clock**: runs on its own at TEMPO (160 BPM default) while RUN is lit. Patch 16ths
  into CLOCK to override it. CLK out sends 16ths either way. SWING delays the off 16ths
  (50-75%).
- **Metric shift** (right-hand column): MOD picks a new pulse against the clock, from 1:2
  (half-time) through 2:3, 3:4, 1:1, 4:3, 3:2 to 2:1. Press SHIFT, or hold the GATE input
  high, and from the next beat the pattern's 16ths run at that ratio. At 4:3 the dotted 8th
  becomes the beat, so 160 feels like 213; at 3:2 it plays 16th triplets (240). It stays
  locked to the clock every 16th. While shifted the bar loops instead of moving on, and
  on release it comes back on the clock's next bar line, starting from step 1, so it stays
  in step with the rest of the patch. The menu can make it come back on the next beat
  instead, which moves the bar line. ON is high while shifted, BEAT ticks every 4 pattern
  steps (the pulse you feel), CLK follows the shifted steps, and the CV under MOD moves
  through the ratios (+-5 V). The menu option "Evolve shifts the bar before each fill"
  does it automatically each phrase.
- **ACCENT** out: a CV of how busy each step is, for modulating anything else.
- Ghost-note triggers are 5 V by default (menu: off, 5 V or 10 V).

The generator is the same code as the browser prototype, so a seed with the same
density and feel gives the same bar in both.

Build: `make dist RACK_DIR=...`. Engine checks and a demo WAV without Rack: `make test`.
Panel: `python3 res/make_panel.py` (needs fonttools). MIT licence.
