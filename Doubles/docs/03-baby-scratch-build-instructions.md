# Doubles 1.1: baby scratches. Build instructions for Opus

Design by Fable, 2026-11-23. Follows `02-build-instructions.md`; read that first.

Hardy's request: "add in baby scratches to the possible actions. This would get closer to
reality, where a quick baby scratch ahead of the beat is common."

## 0. State of the code when you start

The engine and the planner are **already written and tested offline** as part of the design.
They are in `src/doubles_core.gen.tmpl`, `src/doubles.js` and `tools/genexpr.py`, and they are
the source of the demos Hardy heard (`/mnt/project-files/beat-juggling/demos-scratch/`).
Do not redesign them. What is left is the panel, the wiring, the manual and the device build:

1. Add the two controls to the panel (section 4).
2. Rebuild the device (`python3 tools/build_device.py`), which also copies the scripts.
3. Run the checks in section 5.
4. Update the manual, the README and the panel preview; bump the version to 1.1.0.

The committed `Doubles.amxd`, `Doubles.maxpat` and the root copies of the scripts are still the
1.0 build without scratches until you do step 2.

## 1. What a baby scratch is here

One push of the record forward through a drum hit and one pull back to its cue point, with the
fader open the whole time. No fader cuts: that is what makes it a baby scratch.

It is used in two places.

- **Into a cue (the ornament).** When a deck is brought back in on a cued play, the hand may
  flick one scratch on that hit just before letting the record go on the beat. The scratch is
  heard over the tail of the other record, as it is with a crossfader in the middle.
- **On its own (the BABY button).** The record that is not playing scratches the hit that is
  about to arrive, ending on the next beat, and goes quiet again. The live record carries on.

## 2. Engine (gen~), as written

### 2.1 Queue fields

Two of the three spare queue fields are now used. An entry is
`bs, be, p0, kind, cm, rate, gm, gd, g0, amp, dist, gen, str, scn, scd, seq`.

| Field | Meaning |
|---|---|
| `scn` (13) | Pairs of strokes asked for. 0 means no scratch. |
| `scd` (14) | Length of one stroke, in beats. |
| `dist` (10) | For a scratched entry: how much record one forward stroke covers, in record samples. (Still the spin distance for kind 2.) |
| `kind` 3 | New: a scratch on its own. `[bs, be)` is the scratch; `p0` is the cue point. |

### 2.2 Timing

For a cued play (kind 0, cm 0) the engine already works out, when it takes the entry, how long
the hand needs to reach the cue (`need`) and when it will let go (`tr`, samples from now, a few
milliseconds before `bs` so the platter is up to speed on the beat).

A scratch of `np` pairs with strokes of `ns = scd * spb` samples lasts `2 * np * ns` and
**ends at the release**. It is only played if the hand has the time:

```
np = min(scn, floor((tr - need - 0.02 * SR) / (2 * ns)))
```

If `np` is 0 the play is cued in the ordinary way. So the planner asks, and the hands decide.
A late hand never scratches. This is deliberate: it is what a DJ does.

For kind 3 the same code runs with the release moment replaced by the end of the play (`be`).

### 2.3 The motion (mode 7)

The hand reaches the cue (mode 1), waits (mode 2), and when `trrem <= sctot` the fader comes
up (`popen = B`) and mode 7 starts. Strokes alternate forward, back.

- **Forward stroke.** Travel `sctr * (1 + gauss * 0.25 * SLOP)`, never less than 40% of
  `sctr`. The turn-round moment moves by `gauss * reljit * 0.5` (TIMING), at most a fifth of a
  stroke, and the back stroke gives the time back, so a pair always lasts `2 * ns`.
- **Back stroke.** Travel is whatever returns the record to the cue, plus a landing error of
  `gauss * min(cuebase + cueper * distance, 30 ms)` (SLOP). After the last back stroke that
  error is the cue error of the play that follows, and the existing catch-up nudge corrects it.
- **Velocity shape.** `v = a * sin(pi * u)` over the stroke, `u` from 0 to 1. For a push,
  `u` is first raised to the power 0.75, so the record is quickest early in the stroke, as it
  is when a hand starts it. A pull is even. `a = travel / rrate / (c * length)` with
  `c = 0.6560` for a push and `0.6366` for a pull (the mean heights of the two shapes), so each
  stroke covers exactly its travel. Velocity is zero at both ends, so there are no clicks.
- **End.** Kind 0: the record is let go (`mode 3`, `vb = 0`), exactly as after an ordinary
  cue. Kind 3: the hand holds the record still (`mode 0`) and the fader closes at `be`.

A flush that removes the play also stops the scratch and closes the fader. A stop or a jump
clears it.

### 2.4 Two engine bugs fixed on the way (already in the 1.0 device on the branch)

- `rate$` expanded to `rate0` for deck A, which was also the name of the record-rate constant.
  Deck A therefore ran at the square of the tempo ratio whenever Live's tempo was not the tempo
  the record was made at, and lost its own rate in PHASE. The constant is now `rrate`.
  **When adding per-deck names, check that name + "0" or "1" is not already a global.**
- TIMING, WEIGHT, SLOP, RISK and COPY arrive from their dials as 0 to 100 and were passed to
  gen~ unscaled, where the parameters clamp at 1. Every setting above 1% played as fully loose.
  They are now divided by 100 in `anything()`.

## 3. Planner (JS), as written

### 3.1 Patterns

`[pairs, stroke in beats, weight]`:

| Name | Pattern | Lasts | Weight |
|---|---|---|---|
| Quick | 1 pair of 32nd strokes | a 16th | 5 |
| Slow | 1 pair of 16th strokes | an 8th | 3 |
| Double | 2 pairs of 32nd strokes | an 8th | 2 |
| Triplet | 1 pair of 16th-triplet strokes | an 8th triplet | 2 x FUNK |

A pattern is only offered if its stroke lasts between 50 and 190 ms at the current tempo (a
hand cannot go faster, and slower is a drag, not a scratch) and if it fits in the time the deck
is free before the play, less the time to reach the cue. One is then picked by weight.

Forward travel: `min(stroke seconds * (0.75 to 1.05), 95 ms)` of record. So the hand moves at
about playing speed and never goes further than one drum hit.

### 3.2 The ornament

`ornament()` is called from `addPlay()` for every cued play with no gate pattern (kind 0, cm 0,
gm 0). It asks for a scratch with probability

```
SCRATCH * (1 on a beat, 0.7 on an off-beat 8th, 0.35 elsewhere)
```

provided the deck has been free for at least a 16th. This covers every mode: hands-on moves,
Routine and Instinct all make cued plays, so all of them get scratches.

### 3.3 BABY

`babyPress()`, from the message `baby 1`:

- End of the scratch: the next beat or half beat (QUANT, but never finer than a half beat) that
  leaves time for a pattern. Cue point: `snapSamples(rbS(end))`, the hit that is about to play.
- If one deck has nothing planned from now on, a kind 3 play is added to it and sent.
- If both decks are busy (a move is running), `S.forceScratch` is set instead, and the next cued
  play gets a scratch for certain.

`liveDeck` is not changed by a kind 3 play.

## 4. Panel (to build)

| Control | Type | Range, default | Message | Where |
|---|---|---|---|---|
| **SCRATCH** | `live.dial`, percent | 0 to 100, default 30 | `scratch` | HANDS section, as a seventh dial |
| **BABY** | `live.text`, momentary | | `baby` | MOVES, in the one-shot row after DRAG |

- HANDS has two rows of three dials. Make the second row four (SLOP, RISK, COPY, SCRATCH) and
  widen the section and the device by one dial (44 px). Move PLAY and OUT right to match.
- SCRATCH is **not** driven by the HANDS macro. It is a habit, not a looseness.
- Text in HANDS stays white.
- Change the note under MOVES to "ON/OFF: LOOP TO PHASE.  ONE-SHOT: TRIPLE TO BABY."
- Long names: "Scratch" and "Baby". Both are automatable and MIDI-mappable like the rest.
- Routine window: no change. Routine slots get scratches through SCRATCH.

## 5. Checks before it goes to Hardy

Offline, with `ref/sim.js` (scripts are plain JSON, see `ref/out/*.json`; `ctl` takes
`scratch`, events take `{"beat": b, "press": "baby"}`):

1. `scratch: 0`: no entry has `scn`, and no deck ever enters mode 7.
2. LOOP at 1 beat with `scratch: 1`: every return is scratched (12 of 12 in the design run),
   the level on the beat is unchanged within 5%, and the count of late returns does not rise.
3. BABY over straight play: 18 presses gave 18 scratches, each ending within 15 ms of a beat
   or half beat, and the live deck's position is untouched.
4. Routine and Instinct at `scratch: 0.3`: 6 scratches in 17 bars in each design run.
5. Peak hand speed stays under 2.0 (it was 1.5 to 1.9 in the design runs).

In the device: the lint in `build_device.py` passes, the codebox has no function definitions,
and the parameter count is 65.

## 6. Manual

- MOVES: add BABY. "The record that is not playing scratches the next hit, ending on the beat."
- HANDS: add SCRATCH. "How often a baby scratch is flicked in just before a record comes back.
  0 is never. 30 is an occasional habit. 100 is every time there is room. A hand that is late
  never scratches, so loose hands scratch less than the dial says."
- One new recipe: "Loop with pick-ups: LOOP at 1 beat, SCRATCH at 100."
- Version history: 1.1.0, baby scratches; the two fixes in section 2.4.
