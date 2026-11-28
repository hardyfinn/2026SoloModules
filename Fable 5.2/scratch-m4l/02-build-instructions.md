# DJ Claude Skratch: build instructions for Opus

Design by Fable, 2026-11-23. Read `01-research-and-proposal.md` first for what the device is.
The name is Hardy's: **DJ Claude Skratch**.

Hardy heard the prototype demos on his own sample and said "amazing. use the defaults you
selected." So the sound of `ref/scratch_engine.py` is approved and is the thing to reproduce.
Do not redesign the strokes, the fader timings or the cartridge stage. Where this document and
the reference engine disagree on a number, the engine wins and this document has a mistake.

## 0. Decisions

| Question | Decision |
|---|---|
| Kind of device | Max for Live **instrument** (MIDI in, audio out). No audio input. |
| Words | All sixteen (section 3). |
| Cartridge model | Always on. No dial. |
| Name | DJ Claude Skratch. File `DJ Claude Skratch.amxd`. Short name in code: `skr`. |
| Where | `hardyfinn/2026SoloModules`, new folder `DJClaudeSkratch/`, branch `claude/project-thread-ca7dnn`. Not main unless Hardy says. |
| Live | Suite 12.3 (Max 9, so `v8` is available). Apple silicon Mac. |

## 1. What to reuse from Doubles

`Doubles/` is on main in the same repository. Copy what you need into `DJClaudeSkratch/`; do not
import across folders. Worth taking:

- `tools/build_device.py`: the patch is generated from Python, scripts are embedded in the
  `v8` object, the device is never hand-edited. Keep that. The `.amxd` header differs for an
  instrument: Doubles writes `b'aaaa'` (audio effect). An instrument is `b'iiii'`. **Verify
  against a real instrument .amxd if you can get one**; the Doubles header was verified that way.
  An instrument has no `plugin~`; it has `midiin` or `notein`, and `plugout~`.
- `tools/genexpr.py` and the template approach for the gen~ codebox, with its rule: every
  History is copied to a local at the top of the sample and written back at the end. Also its
  hard-won lessons: no function definitions in the codebox; check generated names against
  globals; scale 0 to 100 dials to 0 to 1 before they reach gen~.
- `src/doubles_strip.js`: the waveform strip with movable marks. Here the marks are cue points.
- The queue pattern: JavaScript plans ahead and writes entries into a `buffer~`; gen~ executes
  them sample-accurately and reports through a status buffer that JavaScript polls every 20 ms.
- How the record is kept with the Live set, the `live.drop` loading, the panel helpers
  (`dial`, `menu`, `toggle`, `button`, `label`), the lint, `tools/make_manual.py`,
  `tools/panel_preview.py`, and `ref/sim.js` with `ref/core_sim.js` (the offline harness that
  runs the real planner against a JavaScript copy of the gen~ core).

Not reused: the two decks, the crossfader, the moves, the Doubles stroke code (mode 7).

## 2. Architecture

The big difference from Doubles: **gen~ decides nothing.** In Doubles the engine works out
whether the hand has time and adds the human error. Here the planner (JavaScript) does all of
that, because it can: one deck, and every movement is known when it is planned. The planner
keeps its own copy of where the record is, turns each word into concrete segments with the
error already in them, and gen~ plays the segments.

```
MIDI notes, panel, Routine, Instinct
        |
   planner (v8)  --- words -> hand segments + fader events, humanised, 1 to 2 beats ahead
        |                 writes skr_q (hand) and skr_f (fader)
   gen~ core     --- clock, executes segments, cartridge stage, fader
        |
   plugout~
```

Buffers (all `---` prefixed, as in Doubles): `skr_rec` (the record as loaded, stereo, for the
strip and for saving), `skr_cut` (the record upsampled 4x and pre-emphasised, stereo, what gen~
reads), `skr_q`, `skr_f`, `skr_stat`.

### 2.1 Clock

All times are in **beats on one running clock** `B`.

- Live playing: `B` is the song position, from `plugphasor~` and a beat counter as in Doubles.
- Live stopped: `B` keeps running at the current tempo from where it was, so the device can be
  played from the keys with the transport stopped. When Live starts, `B` jumps to the song
  position; the planner sees the jump (status buffer), flushes both queues and replans.
- A stroke's length is in **seconds**, fixed when it is planned. Its start is in beats. A tempo
  change mid-stroke is ignored; strokes are short.
- The record does **not** follow tempo. Playing speed is the file's own speed. Only the timing
  of the words follows Live.

### 2.2 Hand segments (`skr_q`)

One entry per segment. Fields:

| Field | Meaning |
|---|---|
| `tb` | Start, in beats |
| `kind` | 0 hold, 1 stroke, 2 play (let go), 3 wiggle |
| `dur` | Length in seconds |
| `p1` | Record position at the end, in seconds. gen~ **sets** the position to this when the segment ends |
| `a` | Stroke: peak scale. Play: target speed (1). Wiggle: amplitude |
| `k` | Stroke: skew exponent. Wiggle: whole cycles. Play: time constant in seconds |
| `e` | Stroke: outer exponent (1, or 0.6 for a drag). Wiggle: wobble depth |
| `ph` | Wiggle: wobble rate and phase packed, or use two fields |
| `gen` | Flush generation, as in Doubles |

Speeds, with `u` running 0 to 1 over the segment (use the sample centre, `(i + 0.5) / m`):

- **Stroke**: `v = a * sin(pi * u^k)^e`. The planner computes `a` so that the stroke covers its
  travel: `a = travel / (dur * mean)`, where `mean` is the average of `sin(pi * u^k)^e` over 0
  to 1, found numerically in JavaScript (256 points is plenty).
- **Play**: `v` moves from its current value to `a` with time constant `k`.
- **Hold**: `v` decays to 0 from its current value with `tau_stop`, plus the tremor term.
- **Wiggle**: as `Deck.wiggle` in the reference.

Setting the position to `p1` at the end of every segment removes rounding drift between the
planner's idea of the record and the engine's. It is silent, because the cartridge output is
proportional to speed and every stroke ends at rest. For a play segment the planner works out
`p1` from the closed form (`a * T + (v0 - a) * k * (1 - exp(-T / k))`).

If a segment's `tb` has already passed when gen~ reaches it, it starts at once and still lasts
`dur`. The planner should never cause that, but it must not break.

### 2.3 Fader events (`skr_f`)

The planner resolves openings and clicks into a flat list of `(tb, gain)` events (gain 0 to
about 1.12). gen~ moves the fader gain to the new value in a straight line over `fade` seconds
(a Param: 0.6 ms plus 1.6 ms times SLOP). The fader is applied **after** the cartridge stage.

### 2.4 The cartridge stage

Proven in `ref/device_form.py`, which prints the coefficients. Run it. Measured there: at
playing speed the output matches the record within 0.11 dB from 20 Hz to 18 kHz, and a
performance matches the reference engine within 0.1 dB to 4 kHz and about 1 dB at the very top.

On load, in JavaScript (in slices on a Task so the interface does not freeze; show "CUTTING"):

1. Upsample each channel 4 times: zero-stuff, then a windowed-sinc filter, 16 taps a phase
   (`upsample4` in `device_form.py`).
2. Filter with PRE: the two first-order sections of the phono curve with numerator and
   denominator swapped, at 4 times the buffer's sample rate. Divide by `G1K`.
3. Write to `skr_cut`.

Longest record: 20 seconds. A longer file is cut off at 20 seconds and the panel says so.
Coefficients depend on sample rate. Compute them in JavaScript from the formula in
`device_form.py` (`section`), for the **buffer's** rate when cutting and for Live's rate in
gen~. If the file's rate and Live's rate differ, resample the file to Live's rate first, or
scale the read increment; say which you did.

Per output sample, in the codebox, a `for` loop of four sub-steps:

```
for j in 0..3:
    p  += v_sub / (4 * SR)                 # v interpolated across the sample
    x   = linear read of skr_cut at p * 4 * SR   (per channel)
    y   = x * v_sub
    y   = PLAY section 1, then PLAY section 2     (phono curve)
    y   = three low-pass biquads (LOWPASS, elliptic, 19 kHz)
out = y after the fourth sub-step, times G1K, times fader, times LEVEL
```

Filter states are per channel. Clamp the read position to the record. Denormals: add 1e-20.

### 2.5 Status buffer

`B`, playing flag, queue heads for `skr_q` and `skr_f`, position `p`, speed `v`, fader gain,
and an alive counter. The strip shows a needle line at `p` and lights while the fader is open.

## 3. The words

The planner is a port of class `Scratch` in `ref/scratch_engine.py`, with class `Deck` reduced
to bookkeeping (it emits segments and tracks `t`, `p`, `v`; it does not make audio). Port the
methods one for one, including `start`, `_err`, `_trav`, `_fwd`, `_back`, `_silent_back`, the
3.0 speed cap (4.5 for the lazer flick), the cue-move rule, and class `Hands` with all its
constants. Keep the same order of random draws where it is easy; it makes checking simpler.

`u` below is the SPEED setting in beats: 8ths 0.5, 16ths 0.25 (default), 16th triplets 1/6,
32nds 0.125. The table gives each word's call at `u` and how long one unit lasts.

| Key | Word | Call (one unit) | Unit lasts |
|---|---|---|---|
| C1 36 | Baby | `baby(b, u)` | 2u |
| C#1 37 | Forward | `forward(b, u, u)` | 2u |
| D1 38 | Stab | `stab(b, u/2, u/2)` | u |
| D#1 39 | Chirp | `chirp(b, u)` | 2u |
| E1 40 | 1-click flare | `flare(b, 2u, 1, 1)` | 4u |
| F1 41 | 2-click flare | `flare(b, 2u, 1, 2)` | 4u |
| F#1 42 | Crab | `crab(b, 2u)` | 4u |
| G1 43 | Tear | `tear(b, u)` | 3u, then rest u |
| G#1 44 | Scribble | `scribble(b, 2u)` | 2u |
| A1 45 | Transform | `transform(b, 8u, u, pat=(1,1,0,1))` | 8u |
| A#1 46 | Drag | `drag(b, 2u)` | 4u |
| B1 47 | Tip | `tip(b, u/2, 2)` | 2u |
| C2 48 | Lazer | `lazer(b, u/2, 1.5u)` | 2u |
| C#2 49 | Release | `release(...)`, see below | while held |
| D2 50 | Echo | `forward(b, 1.1u, 0.9u, amp=0.6^i)` | 2u a repeat |
| D#2 51 | Reverse | `reverse(b + u, u, u)` | 2u |

A stroke shorter than 45 ms cannot be played: use the next slower SPEED for that word
(a 32nd tip at 140 BPM, for example). `flare`, `crab` and `drag` at 8ths become very long;
that is correct, they are slow versions.

C3 to G3 (60 to 67) choose cue points 1 to 8. The move happens in the next gap (`start` does
it). Notes outside these ranges do nothing.

### 3.1 Playing from keys and buttons

- One hand, so **one word at a time**, last note wins.
- A note-on starts its word at the next QUANT point (16ths default; 8ths, beats, or OFF for "as
  soon as possible", which is about 10 ms).
- **While the key is held the word repeats**, one unit at a time. The planner adds the next unit
  when the current one is within a short lookahead (half a unit, at least 60 ms). After note-off
  the current unit finishes. One unit is always played.
- Echo counts repeats while held; level `0.6^i`, stop after 6.
- Release: on note-on the record is let go and plays. On note-off (at least a 16th later) the
  fader cuts and the hand pulls back (`_silent_back`). If the record reaches its end first, it
  cuts there.
- Velocity: stroke length times `0.8 + 0.4 * vel/127`, fader level `0.7 + 0.3 * vel/127`.
- A new note during a unit takes over at the end of that unit, except Release, which is cut at
  the next QUANT point.
- The sixteen panel buttons do the same as the keys (momentary).
- In ROUTINE and INSTINCT, a held key or button takes over for as long as it is held, and the
  mode carries on at the next bar line.

### 3.2 Swing

`Scratch.sec()`: the off 16ths move by FUNK (50% to 66%, default 58%). Applies to every word
start and to transform steps. Triplet SPEED ignores swing.

## 4. Modes

**MODE** menu: HANDS (plays only when played), ROUTINE, INSTINCT.

### 4.1 Routine

Sixteen slots of one beat each, looping, starting on a bar line. Each slot: a cell from the
table in 4.2 (by name, including `rest`), and a cue point (1 to 8, or "same"). A two-beat cell
takes two slots; the slot after it shows as taken. Edited in a pop-up window like Doubles's
Routine window. Default routine: the plan printed for `30-instinct-tight-dj`, first four bars.

### 4.2 Instinct

Port `cells()` and `instinct()` from `ref/make_demos.py` exactly: the 24 cells with their
lengths, weights and difficulty, the weight formula, `bar()`, `vary()`, and the four-bar shape
A, A again (or varied), B, ending. It plans one four-bar phrase at a time, on a four-bar line.

| Dial | Range, default | In the reference |
|---|---|---|
| DENSITY | 0 to 100, 50 | `density` |
| SKILL | 0 to 100, 75 | `skill` |
| MOTIF | 0 to 100, 60 | `motif` |
| RELEASE | 0 to 100, 60 | `release` |
| FUNK | 0 to 100, 50 | swing 50% to 66% (50 gives 58%), and multiplies the weight of `tear-trip` and `transform-trip` by 0.3 to 2 |
| WALK | 0 to 100, 0 | `cuewalk`: chance per phrase of moving to another cue point |

- Cells are written at 16ths. SPEED does not change Instinct or Routine.
- No cell uses Reverse. That is how the approved demos were made; leave it, Reverse is for the keys.
- **Word switches**: sixteen small toggles, one per word, all on by default. A cell is offered
  only if every word it uses is on. If nothing but `rest` is left, play rests.
- **PHRASE**: ON bars (1, 2, 4, 8; default 4) and OFF bars (0, 1, 2, 4, 8; default 0). After
  ON bars of scratching the machine rests for OFF bars. With ON under 4, use the first ON bars
  of the four-bar shape and make the last of them the ending.
- The release ending needs the cue to have enough record after it. If less than one beat of
  record follows the cue point, use the other ending.

## 5. The record

- `live.drop` on the strip: aif, wav, mp3, flac, as Doubles. Mono files play on both channels.
- On load: fill `skr_rec`, find cue points, cut the record (2.4), draw the strip.
- **Cue points**: up to 8. Found as in Doubles's `find_stickers`, tuned for speech: a rise in
  the 10 ms level of at least 9 dB over the level 50 ms before, at least 120 ms apart, placed
  5 ms before the rise. The first cue is always the start of the sound (first sample above
  -40 dB, less 5 ms). Marks can be dragged, added (double-click) and removed, as in Doubles.
- **CUE** numbox, 1 to 8: the current cue point. The keys C3 to G3 and WALK change it too, and
  the numbox follows.
- The record and its cue points are saved with the Live set, as Doubles does it.
- **A built-in record**, so the device makes a sound when first loaded: generate
  `sounds.record()` (a synthesized vowel and a three-part word) at build time and embed it. Do
  **not** put Hardy's sample in the repository; it is a commercial recording. It stays in
  `/mnt/project-files/scratch-m4l/source/` for demos.
- CLEAR returns to the built-in record.

## 6. Panel

Same look as Doubles: dark panel, sections with a title, white text in the hands section, text
labels everywhere. Sections left to right:

1. **RECORD**: the strip (waveform, cue marks, needle), drop area, CUE, CLEAR, a status line.
2. **WORDS**: sixteen momentary buttons in two rows of eight, in the order of the table, each
   with its word switch as a small toggle under it. SPEED menu, QUANT menu.
3. **PLAY**: MODE menu, ROUTINE (opens the window), PHRASE ON and OFF menus.
4. **INSTINCT**: DENSITY, SKILL, MOTIF, RELEASE, FUNK, WALK.
5. **HANDS**: the HANDS macro and TIMING, WEIGHT, SLOP, RISK, exactly as in Doubles (the macro
   drives the four; default 50, "tight DJ").
6. **OUT**: LEVEL.

Every control is a Live parameter with a long name that reads well in automation ("Baby",
"Density", "Word On Chirp"). Short names searchable.

## 7. Checks before it goes to Hardy

Offline, with a harness like Doubles's (`ref/sim.js` running the real planner and a JavaScript
copy of the core):

1. **Machine hands match the reference.** With all four hands dials at 0, each of the sixteen
   words, played once at 96 BPM and 16ths, gives a position curve within 1 ms of record and
   fader edges within 1 ms of `scratch_engine.py` with `hands=0`. Add a small exporter to the
   Python side that dumps `p` and the gain for this.
2. **Speeds.** With hands at 50 and at 100, over 64 bars of Instinct: no audible stroke
   averages more than 3.0 times playing speed (4.5 for the lazer flick), and no fader opening
   overlaps a cue move.
3. **Playing speed is flat.** Release from the start of a noise record: within 0.5 dB of the
   record from 30 Hz to 16 kHz.
4. **A held record is silent**: below -80 dB with the fader open and hands at 0.
5. **Keys.** A held Baby key repeats in whole units and stops after the unit in which it was
   let go. Release plays until note-off. A second key takes over at a unit boundary. Works with
   the transport stopped.
6. **Instinct** at defaults reproduces the shape (A, A, B, ending) and, with every switch off
   except Baby, plays only babies and rests.
7. **Queues never overrun** over 10 minutes of dense Instinct at 160 BPM.
8. The lint passes, the codebox has no function definitions, parameter names are unique.

Render the sixteen words and two Instinct runs from the simulator, on the built-in record and
on Hardy's sample, into `/mnt/project-files/scratch-m4l/build/demos/` (not into the repository
for his sample). Say plainly that they are checked by measurement, not by ear.

## 8. Deliverables

- `DJClaudeSkratch/DJ Claude Skratch.amxd` and `.maxpat`, `src/`, `tools/`, `ref/`, `docs/`.
- Manual PDF: what it is, loading a record and cue points, the sixteen words each with its
  hand chart (`ref/make_demos.py` draws them), the key map, the three modes, the dials, five
  recipes (for example "babies and stabs only, PHRASE 2 on 2 off, answering a vocal").
- README, panel preview image, version 1.0.0, a row in the repository's top README.
- Copies in `/mnt/project-files/scratch-m4l/build/`, with a zip of the device and manual.
- It cannot be tested in Live from here. Tell Hardy that, and ask for the Max Console output
  if anything is wrong, as on Doubles.
- Hardy's creations tracker sheet lists Max for Live devices. Ask before writing to it.

## 9. Known risks

- The `iiii` header and the instrument's MIDI input object are the two things not proven by
  Doubles. If the device loads as an effect or receives no notes, look there first.
- Cutting a 20 second stereo record is about 100 million multiplies in JavaScript. It should
  take around a second in `v8`. If it is much slower, cut in gen~ offline or shorten the filter.
- `v8` timing jitter (20 ms tick) limits how late a key can be played before a QUANT point.
  With QUANT OFF, plan the word about 10 ms ahead and accept the jitter.
