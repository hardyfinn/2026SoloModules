# Beat juggling for Ableton Live: research and proposal

Fable, 2026-11-21. Design stage, nothing built. Working name: **Doubles**.

History and names below are checked against the sources at the end. The descriptions of how each move is performed, and all timing figures, are from memory of the technique and are marked "check" where a build would depend on them.

## 1. What beat juggling is

A DJ takes two copies of the same record, one on each deck, and uses the crossfader and both hands to play the pieces of a beat in a new order. It grew out of "cutting", where a short section of a break is kept going by alternating two copies. Steve Dee is named as one of the earliest innovators in the mid-1980s and gave it the name "The Funk". It spread widely from 1990 through the X-Men (later X-Ecutioners), the Invisibl Skratch Piklz and the Beat Junkies, with Cash Money, Jazzy Jeff, Aladdin and Cutmaster Swift before them.

Rob Swift's advice is the useful design brief: every record is different, memorised routines matter less than "turntable instincts", and the point is funk, not speed. So the device should make it quick to try moves on whatever is playing, and should not be a preset machine.

## 2. The moves

| Move | What the DJ does | What you hear |
|---|---|---|
| Loop | Deck A plays a phrase (a beat, half a bar, a bar). At its end the fader cuts to deck B, cued to the same start, while A is spun back. | The phrase repeats for as long as wanted. The basis of everything else. |
| Double (echo) | Deck B runs a fixed distance behind A, usually an eighth or a quarter note. The fader flicks to B after each hit. | Kick-kick, snare-snare. Each hit is answered by its copy. |
| Triple | As double, with a third hit made by pulling one record back for a single hit. | Three of each hit, usually as a fill. |
| Chase / strobe | Both records are advanced by hand one hit at a time, alternating decks. | The beat at about half tempo, each hit stated twice, with a stepped, mechanical feel. |
| Shuffle strobe, broken chase | A chase with some steps skipped or swung. | Syncopated versions of the chase. |
| Pause | A hand stops the record on a hit and releases it late. | A hole in the beat, then the beat displaced. |
| Fill / breakdown | One record is held at a single hit (a kick or a snare) and released and pulled back in rhythm while the other supplies the answer. | A new drum pattern built from two or three hits of the original. |
| Backspin | The record is spun back with the fader open. | The rewind sound, as a pickup into the next bar. |
| Phase | Both decks open, a few milliseconds apart. | Flanging that drifts as the decks drift. |
| Drag / tap | A finger on the platter edge or a tap on the label. | A brief slowing that settles back, used to re-align the decks. |

Body tricks and the carousel are visual and are left out.

## 3. Why Max for Live suits this better than VCV Rack

- Live already holds the beat. A bar or two of any track is played into the device's buffer, with no file handling.
- It knows the tempo, the bar line and the position in the song, so recordings land exactly on the bar and moves are quantised without clock patching.
- Moves can be played from a keyboard or pads, drawn as automation, or recorded as clips, and the result bounces to audio in the arrangement.
- The parts that need a platter (pull-backs, backspins, late releases) need the audio in memory. The buffer provides that.

## 4. The device

A Max for Live **audio effect**. One wide device view.

### 4.1 The record

Hardy's direction (2026-11-21): no live audio processing. The device has a buffer that a short piece of audio is played into first, and the juggling is done on that.

- **RECORD** arms the buffer. Recording starts on the next bar line and runs for a set length (1, 2, 4 or 8 bars), or FREE: starts and stops by hand and the length is trimmed afterwards. The audio comes from whatever feeds the device: a clip on its own track, or another track routed in. What is recorded is "the record", and both decks get a copy.
- While recording, the input is heard. Once the record is pressed the input is ignored and the device is the sound source: it plays the record in a loop, locked to Live's tempo and bar line, and the moves act on it.
- **START / END** trim handles and a BARS setting say how long the piece is in bars, so it stays in time if Live's tempo changes (both decks follow tempo as a turntable would, pitch and speed together, with a KEEP PITCH option).
- **Stickers.** The device finds the hits in the record (kick, snare, hats) and marks them, as a DJ marks the label with stickers. Moves cue to these marks, not to the grid, so a swung or off-grid beat is cut at its real hits. Up to 16 marks, shown on a waveform strip, each can be moved or switched off by hand.
- The record is saved with the Live set, and can be saved in a preset. Dragging an audio file onto the strip loads it in place of recording.
- CLEAR empties the buffer for a new piece.

### 4.2 Two decks and a fader

Each deck is a read position on its copy of the record with a modelled platter. The crossfader is a sharp-curve cut between them. This model is what separates it from a beat-repeat effect:

- **Platter.** A released record does not start at full speed. It takes roughly 30 to 80 ms under a slipmat (check), with a small pitch scoop on every hit that is released from a hold. A pull-back takes real time and has a top speed, so the other deck has to cover while it happens.
- **Two copies are not the same.** Each copy has its own small pitch offset, wear and level. With both open you get phasing, and the decks drift apart until a drag corrects them.
- **Fader.** Cut time about 1 ms (check), with the option of a looser fader that lets a little of the pull-back through.
- **HANDS** runs from MACHINE (perfect cuts, instant cueing, no drift: a clean glitch tool) to HUMAN (finite hand speed, timing scatter of a few milliseconds, late releases, audible slip). Somewhere past the middle sounds like a good DJ. At full it sounds like practice.
- **WEAR** dulls the cue points that get hit most, as cue burn does on a juggled record.

### 4.3 Playing it

Three ways, which can be combined:

1. **Move buttons.** One button per move in section 2, held for as long as the move should last, released back to the straight beat on the next division. Each has its own settings: loop length, double distance (1/16, 1/8, dotted 1/8, 1/4), chase rate, which sticker a fill holds. Every button can be MIDI-mapped, key-mapped or automated in Live.
2. **Routine.** A row of 16 slots, one per bar (or per half bar), each holding a move or "straight". It plays in time with the song, so a 16-bar routine can be written once and left running.
3. **Instinct.** The device chooses moves itself from weighted odds that depend on where it is in the phrase: mostly straight or loops early in a 4- or 8-bar phrase, doubles more likely on the last bar, fills and backspins most likely in the final half bar, and it never repeats the same fill twice running. A DENSITY control sets how busy it is and a FUNK control shifts the odds from safe moves to syncopated ones. The same idea as the weighted note chooser: musical knowledge in the probabilities.

A **FILL PAD** row of 8 pads plays the stickers directly, one deck per hand (odd pads deck A, even pads deck B), for building breakdowns by hand.

### 4.4 Other controls

- STRAIGHT level: how loud the plain looping record is between moves, down to silent, so the device can play moves only.
- DECK B OFFSET fine control for phase effects.
- PITCH per deck, plus or minus 8%, real speed change. With the decks at different pitches, doubles turn into tuned drum answers.
- Crossfader position sent out as a mappable modulation source (for example to open a filter on deck B only).

## 4.5 The human model (added 2026-11-22, after Hardy asked for the device to be more human)

HANDS stays as one macro, and opens into four controls that can be set apart. All of this is in the reference engine (doubles_engine.py, class Hands) and can be heard in demos-baby-cry/.

| Control | What it models |
|---|---|
| TIMING | Releases and fader cuts stray from the beat by a few milliseconds. The error is not fresh each time: a hand that is late tends to stay late for the next few moves. Fast passages get looser and creep early. |
| WEIGHT | How heavy the hand is. The record takes longer to come up to speed, each release is slightly pushed or dragged (a small pitch scoop up or down at the start of the hit), and a stopped record takes longer to stop. |
| SLOP | Untidy cueing and fader work. The record is not pulled back to exactly the sticker, and the further it is pulled the bigger the miss, so a hit can lose its front or bring a sliver of what came before it. The fader opens a touch early and closes a touch late, which lets through the first instant of the pull-back. Levels differ a little from cut to cut. A held record trembles. |
| RISK | How often a move is fluffed outright: a hit dropped 30 to 80 ms out and clipped by the fader. |

Three things happen at every setting above zero, because they are what a DJ does and not errors:

- **Catching up.** When a record is dropped in off the beat and left playing, the DJ hears it and nudges it back over the next beat or so. Measured on the test loop: a drop that lands 11 ms out is within 2 ms four beats later.
- **Hand speed.** A pull-back takes real time. If the move does not leave enough, the hit arrives late.
- **Two copies.** Deck B runs at a very slightly different pitch and is a little duller and quieter.

Instinct mode is also made more human: it tends to say a move twice before changing it, and it gets busier as the routine goes on.

## 5. What it will not do

- It does not process live audio. Nothing can be juggled until a piece has been recorded into the buffer.
- It is not a scratch device. Hand motion is modelled only as far as juggling needs it (pull-back, release, drag). A scratch pad could be added later.
- Vinyl tone (cartridge, groove, surface noise) is left out. In Live that is another device after it.

## 6. Building it, and the risk

A Max for Live device is a Max patch saved as an .amxd file. Opus can write that file directly: the deck, platter and fader engine as one block of gen~ code, the sticker finder and the Instinct chooser in JavaScript, and the panel from Live's own dials and buttons. A reference version of the engine in Python can render demo audio first, so the sound of each move is settled before any Max work.

The risk is larger than for the VCV plugins. Those are compiled and checked on every push. A Max device can only be proved by opening it in Live, and there is no Live here. The first build should be expected to need one or two rounds of fixes from what Hardy sees on opening it, unless a session is run on Hardy's Mac with Live installed.

Needs Live Suite (or Standard with the Max for Live add-on), Live 11 or 12.

## 7. Questions for Hardy

1. **Where the beat comes from.** Settled by Hardy: a buffer that a short piece is played into, no live processing. Still open: is 8 bars long enough as the maximum, and is drag-in of an audio file wanted as well.
2. **How it is played.** Buttons, Routine and Instinct are all proposed. Say if any is unwanted.
3. **Live version**, and whether Max for Live is installed.
4. **Name.** Doubles, or another.
5. **Demos first?** Render each move on a test break in Python before the build spec is written. Recommended.

## Sources

- https://en.wikipedia.org/wiki/Beat_juggling
- https://en.wikipedia.org/wiki/DJ_Steve_Dee
- https://www.djrobswift.com/?p=2089 (Rob Swift, ten principles for a funky beat juggle)
- https://schoolofscratch.com/beat-juggling/ (lesson list: looping, pauses, doubling and echoing, tripling, strobing or chasing, shuffle strobing, broken chase, phasing, marking records)
