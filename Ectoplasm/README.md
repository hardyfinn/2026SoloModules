# Ectoplasm

A break slicer and mangler for VCV Rack 2: a Hær edition of the **Ectocore**.
24 HP. Version 2.0.0.

The sound engine is the real, open-source Ectocore firmware by **Zack Scholl /
Infinite Digits** (<https://github.com/schollz/_core>, GPL-3.0-only), compiled for
the desktop the same way as Infinite Digits' own Rack plugin. Ectoplasm changes
the panel and how you reach the controls, not the sound.

Infinite Digits also publish a free official Rack plugin (module "EZEPTOCORE",
with an Ectocore panel option). Use that one if you want the original 8 HP
hardware layout.

## What Ectoplasm adds

* **Every hidden function on its own knob.** On the hardware these need you to hold
  TAP or MODE while turning another knob. Here they have their own knobs:
  * TAP band: DRIVE (volume, then saturation, then fuzz), FILTER (DJ filter:
    low-pass left, high-pass right), GATE (slice gating), RETRIG (stutter
    probability), PITCH.
  * MODE band: AMIGA, SATURATE, SMEAR, JITTER, CHAOS.
  * Each of these acts like the hardware's shifted knob: it changes the engine
    when you move it (or its CV moves). Holding TAP or MODE and turning the main
    knobs still works too.
* **BANK** and **TEMPO** knobs, and a **PLAY** button (the hardware's TAP + MULT).
* **14 CV inputs:** CLOCK, RESET, PLAY, AMEN, BREAK, SAMPLE, BANK, GRIM, TEMPO,
  DRIVE, FILTER, GATE, RETRIG and PITCH.
* **A display** showing the bank, sample, slice, BPM, rune and trigger mode,
  with the sample's file name underneath.
* **Plain WAV folders.** You don't need the sample manager (see below).

## Controls (same as the Ectocore)

* **BREAK**: how often the current rune's effects fire. The 16-light ring is the
  firmware's own LED wheel.
* **GRIMOIRE**: which of the 7 runes (effect sets) BREAK uses. Edit the runes in
  the right-click menu under *Grimoire*.
* **AMEN**: fully left plays straight. In the middle, it loops 1 to 32 random
  slices. Fully right is generative, with new slices and fills every two bars.
* **JUMP**: in the middle, no jumps. Turning it right jumps and returns in time;
  turning it left jumps and carries on from there.
* **SAMPLE** / **BANK**: choose the sample in the current bank, and the bank.
* **MODE**: sets what TRIG outputs: Kick, Snare, Transient or Random (shown by
  the four lights and on the display). **MULT**: tap to multiply the clock out,
  hold to divide it. **TAP**: tap tempo.
* **CLOCK** expects 2 PPQN. **RESET** makes the next clock pulse beat 1.

## Loading samples

Right-click the module and choose **Choose sample folder…**.

* **Sample-manager exports** (the folders with `bank1`, `bank2`… and `.wav.info`
  files, made by the Infinite Digits sample manager or ectocore.rocks) load
  exactly as they would on the hardware, including slices, transient markers
  and settings.
* **Plain WAV folders** need no preparation:
  * A folder of WAVs fills bank 1, then bank 2 and so on, 16 files per bank.
    Alternatively, a folder of sub-folders makes each sub-folder one bank.
    Files load in name order.
  * Each file is cut into 16 equal slices.
  * The BPM comes from the file name, for example `amen 172bpm.wav` or
    `break_bpm140.wav`. Without one, it's guessed from the loop length,
    assuming a power-of-two number of beats.
  * Files shorter than one second, with no BPM in the name, play as one-shots.
  * WAVs can be any rate, 8/16/24/32-bit or float, mono or stereo. Nothing is
    written to your folder.
  * The Kick, Snare and Transient trigger modes need the sample manager's
    markers. With plain WAVs, use Random.

No breaks are bundled.

## Building

Like the original, the engine is generated from the firmware sources at build
time, by `engine/lib/core_engine/prepare.py` using Clang. This needs `python3`
(standard library only) and `clang` (Xcode's on macOS). Then run
`make dist RACK_DIR=…`.

### Offline test (no Rack)

```sh
python3 test/make_break.py /tmp/breaks
python3 engine/lib/core_engine/prepare.py --out /tmp/gen --clang clang
clang -O2 -std=c11 -fwrapv -w -c /tmp/gen/engine.c -o /tmp/engine.o
clang -O2 -std=c11 -c engine/lib/core_engine/card_format.c -o /tmp/card.o
clang -O2 -std=c11 -c engine/lib/pcg_basic.c -o /tmp/pcg.o
clang++ -O2 -std=c++17 test/render.cpp src/Storage.cpp /tmp/*.o -o /tmp/render -lpthread
/tmp/render /tmp/breaks /tmp/out.wav
```

## Licence and credits

Ectoplasm is GPL-3.0-only (see `LICENSE.txt`), because it is built from GPL-3
code:

* `engine/` is Zack Scholl's `_core` firmware (`lib/` and `main.c`). Copyright
  2026 Zack Scholl, GPL-3.0-only. It is copied from upstream commit
  `c817b92` (2026-11-09, firmware v8.1.0), with these changes, each marked "Ectoplasm" in the
  file:
  * `lib/ectocore.h`: a bank-request hook for the BANK knob.
  * `lib/core_engine/prepare.py`: uses the committed
    `crossfade4_441.pregenerated.h` (so the build needs no NumPy), and includes
    `haer_api.inc`.
  * `lib/core_engine/core_engine.h`: declarations for `haer_api.inc`.
  * new `lib/core_engine/haer_api.inc`: the dedicated controls.
  * removed: `lib/test`, `lib/sdio` (hardware only).
* `src/Storage.*` and `src/State.hpp` are from `_core/rack/src` (Zack Scholl,
  GPL-3.0-only). Storage was modified to load plain WAV folders and report
  sample names.
* `src/Ectoplasm.cpp` is based on `_core/rack/src/Ectocore.cpp`, with a new panel
  and controls by Haer.
* `engine/lib/pcg_basic.*`: PCG random generator, Copyright 2014 Melissa O'Neill,
  Apache-2.0 (`licenses/Apache-2.0.txt`).
* The panel, layout and Hær logo are Haer's own. No Infinite Digits or Toadstool
  Tech artwork is used. "Ectocore" is named only to credit the original.
