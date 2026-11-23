# Doubles

A Max for Live audio effect for beat juggling. You record a short piece (1 to 16 bars) into the device, and it plays it back on two modelled turntables with a crossfader. The moves are loop, double, triple, chase, shuffle strobe, pause, fill, spin (backspin), phase and drag. The hands that play them are modelled too: TIMING, WEIGHT, SLOP and RISK, with HANDS moving all four at once.

Built for Ableton Live Suite 12.3 (Max 9), Mac.

## Install

1. `Doubles.amxd` is self-contained: both scripts are stored inside it (Max 9 `v8` and `v8ui`, which come with Live 12.2 and later). Put it in your User Library, for example `Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/Doubles/`.
2. Drag `Doubles.amxd` onto an audio track.
3. Open the Max Console (right-click the device title bar, then Open in Max, then Window > Max Console). If it shows errors, copy the text and send it back. That is the fastest way to fix anything.
4. Once it works, you can open it in Max and press Freeze, which makes one self-contained file.

Recordings are saved as `doubles-YYYYMMDD-HHMMSS.wav` in the same folder as the device. The Live set remembers which file it used.

### If the .amxd will not load

`Doubles.maxpat` is the same patch as plain Max JSON. Drag Live's empty **Max Audio Effect** onto a track and click its Edit button. In Max, select all and delete, then open `Doubles.maxpat` in Max, select all, copy, and paste into the device. Save it as `Doubles.amxd`.

## Checklist for the first test

1. Drop it on a drum track. Audio passes through unchanged.
2. Set LENGTH to 2 BARS, press RECORD and start Live. It arms, records two bars from the next bar line, then the strip shows the waveform with ticks.
3. The loop now plays from the device in time with the song, and the track's own clip can be stopped.
4. Turn LOOP on: the current beat repeats. Turn it off: it rejoins the song in the right place.
5. Try each move button.
6. HANDS to 0: the cuts become exact. HANDS to 100: loose and late. Back to 50.
7. Set MODE to INSTINCT: moves appear by themselves, getting busier towards the end of each 16 bars.
8. Open ROUTINE..., set a few slots, and set MODE to ROUTINE.
9. Save the set, close it and reopen it: the record is still there.
10. The Max Console shows no errors.

## Files

| Path | What |
|---|---|
| `Doubles.amxd` | The device (generated). |
| `doubles.js`, `doubles_strip.js` | Copies of the scripts embedded in the device, for reading. |
| `Doubles.maxpat` | The same patch, for the fallback above. |
| `src/doubles_core.gen.tmpl` | The audio engine (gen~ code), as a template for the two decks. |
| `src/doubles.js` | Planner, routine, instinct, sticker finder, record handling. |
| `tools/build_device.py` | Builds the device: `python3 tools/build_device.py`. |
| `tools/genexpr.py` | Expands the gen~ template, and translates it to JavaScript for the tests. |
| `ref/sim.js` | Runs the whole device offline (engine and planner together) on a loop. |
| `ref/check.py`, `ref/compare.py` | Test checks. |
| `ref/doubles_engine.py` | Fable's original reference engine. |
| `docs/Doubles-Manual.pdf` | Manual with recipes. |

## What was tested, and what was not

Tested offline by running the gen~ code (translated to JavaScript) together with `doubles.js`, sample by sample, on an 8-bar loop at 107 BPM:

- Recording starts on the bar line and captures exactly the set length.
- With HANDS at 0 and COPY at 0, every juggled slot is the right part of the record (36 of 36 slots; timing error under 0.5 ms).
- At HANDS 50 (tight DJ), the hits land on average 3 to 4 ms from the machine timing, which is within the design target.
- There is no NaN, and the output is soft-limited.

Not tested, because Live cannot run here: the device loading in Live, the panel, the Live API transport hookup, `plugphasor~`, saving and reloading the record with the set, and file drop. Expect a round of fixes from the Max Console output.

## 1.1.0: baby scratches

Built from `docs/03-baby-scratch-build-instructions.md` (design by Fable). New controls: SCRATCH and OVER in HANDS, BABY in MOVES; QUANT moved under the move menus. No more racing to catch up after a late return (corrections are capped at 3 %). Offline checks: SCRATCH 0 gives no scratches; LOOP at one beat with SCRATCH 100 scratched 12 of 12 returns; BABY with OVER on SILENCE scratched 18 of 18 presses, each ending within 5 ms of the grid with the other record fully cut; biggest drift correction 3.0 %.

## Differences from the build spec

- Held moves (LOOP, DOUBLE, CHASE, SHUFFLE, FILL, PHASE) are on/off buttons, not hold-to-play. Turning one on switches the others off. That replaces LATCH and works the same from a MIDI controller.
- TRIPLE, PAUSE, SPIN, DRAG and BABY are one-shots. Each pad plays its sticker until the next hit, or for one beat.
- The other deck, when unused, runs muted in time with the song, so it is ready to catch. A real DJ does this, and it keeps pull-backs short.
- When the hand is late, the fader waits for the release instead of opening onto a record that hasn't moved yet.
