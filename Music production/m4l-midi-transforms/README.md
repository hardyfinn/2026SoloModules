# Hær MIDI Transforms for Max for Live

Three **MIDI effects** for Ableton Live that take a loop you already have and keep it moving, so it
doesn't go stale over a long track:

- **Mutator** learns your bar and makes **one small change every few bars**. REVERT takes you back.
- **Euclid Morph** slides the hits of your pattern towards an **evenly spread (Euclidean)** version.
  The MORPH knob goes from yours (0%) to fully even (100%).
- **Markov Remix** learns **what tends to follow what** in your loop and makes new bars with the
  same feel. STRICTNESS goes from "almost yours" to "wild".

> **Not tried in Live yet.** The logic is tested outside Live (48 checks, including a simulation of
> the patch's player), but nobody has loaded these devices in Live yet. If something doesn't work,
> see *If it doesn't work* at the end.

## Install

Each device comes with one script file. **Keep the two side by side in the same folder:**

| Device | Script that goes next to it |
|---|---|
| `Mutator.amxd` | `mutator_0db30698.js` |
| `EuclidMorph.amxd` | `euclid_dc2a1db6.js` |
| `MarkovRemix.amxd` | `markov_01fc0eab.js` |

A good place: `User Library/Presets/MIDI Effects/Max MIDI Effect/`. Then drag the device onto a
**MIDI track, before the instrument** (in front of a Drum Rack, Simpler, a synth, or an External
Instrument that goes to the modular).

## How all three work

1. Play a clip into the track as usual. While the device is in **THRU** you hear the clip as it is.
2. Pick a **Length** (1, 2, 4 or 8 bars) and press **LEARN**. The display says *ARMED*; learning
   starts at the **next bar line** and lasts that many bars (*LEARNING 1 / 2 ...*).
3. At the end the device **takes over** (*PLAYING*): from now on it plays its own version of what
   it learned, and the notes coming in are ignored. You can stop the clip, or leave it running.
4. **THRU** hands the track back to the incoming notes. Press THRU again to play the learned
   pattern again.

The learned pattern is **saved with your Live set**. It plays in time with the song wherever you
start playback, and it keeps the **feel** of what you played: notes slightly early or late stay
early or late (only where they start is changed, never the groove inside a 16th).

**The display:** one row per note (a drum lane, e.g. C1 = kick), one column per 16th. A grey outline
is where a note was when you played it; an orange block is what plays now (brighter = louder).
The bottom line says what the device did last.

Only notes are changed. CC, pitch bend, aftertouch and program changes pass straight through.

## Mutator

![Mutator](previews/Mutator.png)

| Control | What it does |
|---|---|
| **Change every** | How often a change happens: every 1, 2, 4, 8 or 16 bars (on the bar line). You always hear your own pattern for the first round. |
| **ROTATE** | A whole lane slides one 16th earlier or later. |
| **DROP** | One note is taken out (it is remembered, ADD may bring it back). |
| **SHIFT** | One note moves one 16th earlier or later. |
| **ADD** | Brings back a dropped note, or adds a quiet "ghost" copy of a hit next to it. |
| **HOLD** | Stops changing and keeps the pattern as it is now. |
| **NOW** | Makes one change straight away. |
| **REVERT** | Back to exactly what you played. |
| **KEEP** | "I like this one": the current pattern becomes the new original, so REVERT comes back here. |

Each change touches only one lane, so it never jumps far. Leave it running and the loop drifts
slowly; switch HOLD on when it lands somewhere good, then KEEP.

## Euclid Morph

![Euclid Morph](previews/EuclidMorph.png)

| Control | What it does |
|---|---|
| **Morph** | 0% = what you played. 100% = each lane keeps its number of hits, spread as evenly as possible over the loop. In between, every hit **slides smoothly** towards its even place, so 30 to 60% gives in-between timings that are neither yours nor straight. |
| **Hits** | Add (+) or remove (-) hits in the even version. New hits fade in, extra ones drop out, when Morph passes 50%. |
| **Turn** | Rotates the even pattern by whole 16ths. At 0 the device picks the rotation that moves your hits least. |
| **Lanes** | *each note*: every drum or pitch is its own lane (drums). *together*: all notes count as one lane, notes starting together move together (melodies, chords, arps). |

Morph, Hits and Turn are normal Live controls: **automate Morph** in the Arrangement for a part
that slowly straightens out (or slowly falls apart, going from 100% down to 0%).

## Markov Remix

![Markov Remix](previews/MarkovRemix.png)

| Control | What it does |
|---|---|
| **Strictness** | 100%: plays exactly what you played. Lower: more often, instead of going on to the next 16th of your loop, it jumps to another place in your loop that came after the same thing (for example "anything that followed a kick"), preferring places in the same spot of the beat. 0% = always jumps. |
| **New remix every** | A fresh remix every 1, 2, 4 or 8 bars, or *never* (keep the one you have). The first round always plays your loop. |
| **NEW** | A fresh remix at the next bar line. |
| **ORIGINAL** | While on, plays what you played (to compare). Off: back to the remix. |
| **remix by: kit / lanes** | *kit*: whole steps of the kit are moved around together (keeps kick-snare-hat combinations). *lanes*: every drum gets its own remix (more new combinations, looser). |

A longer Length gives it more to learn from: 2 or 4 bars with a bit of variation work best.
Every 16th it plays is one that happened in your loop, so it stays in your style.

## Starting points

- **Hats that never repeat exactly:** hats on their own MIDI track, Mutator, Change every 2,
  only SHIFT and ADD on.
- **Kick and snare stay, the rest drifts:** split the kit over two tracks; Mutator or Markov only on
  the percussion track.
- **A build:** Euclid Morph on the percussion, automate Morph 0% to 100% over 16 bars, then
  back to 0% at the drop.
- **Detroit chords that answer back:** a chord clip, Markov Remix with *lanes* off (kit),
  Strictness 70%, New every 4.
- **Chain them:** Euclid Morph then Mutator on the same track. The second one learns what the
  first one plays.
- **To the modular:** put the device before an External Instrument (or CV Tools) that goes out
  through the disting NT, so the modular gets the changing pattern.

All buttons can be mapped to keys (**Cmd/Ctrl+K**) or a controller (**Cmd/Ctrl+M**), and
automated in the Arrangement, so you can write REVERT, NEW or HOLD into the song.

## Limits

- Assumes **4/4**. Plays on a 16th grid (the feel of each note is kept on top of that).
- Up to **8 bars** learned. The display shows up to 16 rows.
- While the device is PLAYING, incoming notes are ignored (that's what THRU is for).
- If you stop Live while LEARNING, learning starts again at the next bar line when you play.

## If it doesn't work

- **Display is empty / Max Console says "error -1 opening file"**: the .js was not found. Put the
  .js next to the .amxd (same folder) and load the device from there. Or: click the device's
  edit button (opens Max), then **Freeze Device** and save. A frozen device carries its script
  inside and works anywhere.
- **Nothing plays after LEARN:** check the device is *before* the instrument and that Live is
  playing (learning waits for a bar line while the transport runs).
- Please send what the Max Console says: it's in Max's Window menu, once the device is open for editing.

## How it is built (for opening in Max)

The patch does the timing, the script does the thinking:

- **Timing chain** (high priority, so tight): `metro 16n @quantize 16n` → `transport` → an `expr`
  that counts 16ths since the start of the song → `if` (only once the device has taken over) →
  `% loop length` → `coll` (what's on each 16th) → `zl iter 4` → `unpack` → the start offset and
  length (in 16ths) times *ms per 16th* → `pipe` → `makenote` → `midiformat` → `midiout`.
- **Input:** `midiin` → `midiparse` → each note gets its song position from a second `transport`,
  goes to the script, and through a `gate` while the device isn't playing (note-offs always pass).
- **Script** (`jsui`, also the display): learns, changes the pattern and rewrites the `coll` a
  16th before the bar line where the change is heard. The learned pattern goes into a `pattr` that
  Live saves with the set.

Source, build script and tests: `source/` in the zip (`python3 build_devices.py`,
`node test/core_test.js`).
