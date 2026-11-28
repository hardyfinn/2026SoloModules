# Bass player for Comper: research and proposal

Designed on Fable, 2026-11-23. A walking bass player with its own upright bass sound, following Comper's chart. Status: proposal with a working prototype (line engine and upright model) and 14 audio sketches. Nothing is built.

## What it is

The bass in the Comper demos was a throwaway: a General MIDI bass playing a 25-line script. This replaces it with a real second player. It reads the same chord chart and bar position as Comper, decides its own line, and plays it on a modelled upright bass. It also sends the notes out, so the line can drive any other voice.

Two halves, as with a real bassist: **the line** (which note, when) and **the sound** (the instrument and the hands).

## The line

A walking line is not random chord tones. It is a small set of rules that every player follows and that the ear expects:

1. **Arrive on the root.** When a chord arrives, the bass is on its root. If the same chord simply continues into another bar, the bass may land on the 3rd or 5th instead.
2. **Lean into the arrival.** The note before a chord arrives is chosen backwards from where the line is going: a half step below or above the coming root, a scale step, or its fifth. This is what makes a line sound like it knows the tune, and it means the player has to see one chord ahead.
3. **Fill the middle by the smallest sensible path.** The notes in between are searched, not rolled: every route through chord tones, scale tones and (with OUT up) chromatic notes is scored, and the cheapest wins. Steps are cheap, thirds are fine, repeats and big leaps are expensive, changing direction costs a little.
4. **Have a shape.** The line rises and falls over several bars around a slowly moving centre, so it travels the neck instead of circling one octave.
5. **Decorate with the right hand.** Skips (a short note on the swung upbeat, often a muted "dead" note), a leaning half step into the next beat, and the occasional triplet run filling a third.

Measured on the prototype, 24 bars of blues at the default settings: the root is on beat 1 in 24 of 24 bars; 60% of moves are steps, 26% thirds, 14% larger; no repeated notes; 74% of notes are chord tones; range A1 to D3; approach notes split between scale steps (11), half steps (13) and fifths (4). `sketches/bass-lines.png` shows three versions of the same blues.

Six feels, normally following Comper's GROOVE switch: **WALK** (four to the bar), **TWO** (half notes, root and fifth, with pickups), **BALLAD** (long notes, vibrato, whole notes when it is sparse), **BOSSA** (root, pickup, fifth, pickup), **SOUL** and **HOUSE** (sixteenth-note bar figures that repeat, with ghost notes). I trust walk, two and ballad most. Bossa is the standard pattern and should be right. Soul and house are three hand-written figures each and are the parts I trust least.

## The hands

The same idea as LOOSE on Comper and TIMING/WEIGHT/SLOP on Doubles, with two things that only a bass has:

- **On top of the beat.** A jazz bassist plays slightly ahead of the ride cymbal and well ahead of the piano. The prototype puts the bass about 10 ms early by default (AHEAD), while Comper sits a few ms late. That gap is a large part of why a rhythm section feels like it is moving.
- **Intonation.** No frets. Every note is a few cents off (about 4.5 cents of scatter at LOOSE 1), high notes run slightly sharp, and after a leap the note is found from below: it starts 20 to 60 cents flat and settles within a few hundredths of a second. Long notes get a slow vibrato.

Plus weight (2 and 4 slightly heavier, phrases drifting in loudness), slop (a note that fails to speak now and then), and notes cut short before a leap because the hand has to travel.

## The sound

I did not want to guess at an upright, so I measured one. The model is fitted to the Karoryfer Meatbass pizzicato samples (a real double bass, public domain): about 50 recorded plucks across the range at three loudnesses, plus recordings of knocks on the body.

What the measurements say, and what the model therefore does:

- **The lowest notes have almost no fundamental.** At C1 and Eb1 the fundamental is 17 to 25 dB below the second harmonic. The body cannot radiate below about 45 Hz; you hear the low E from its overtones. The model has that steep cut.
- **The body is strongest between 50 and 80 Hz, with a hollow around 115 to 165 Hz,** and narrow resonances at 65, 104, 150, 200, 226, 268, 300 and 440 Hz. Notes that land on a resonance are louder and die sooner, because the body drinks the string's energy. This is the built-in unevenness of a real bass.
- **The pluck is two sounds.** About 60% of the level is gone in the first tenth of a second, then the rest rings on slowly. Before I added this the model sounded like a sustained tone with a click; the level-against-time error dropped from 6.2 to 4.4 dB when I did.
- **Brightness goes with how hard, not how high.** The fit preferred a rolloff counted in harmonics (about 4 harmonics for a soft pluck, 9 for a hard one) at every pitch.
- **High partials die fast, and faster the higher up the neck you play.**

On top of the measured core, things the samples cannot give: **GROWL** (the string buzzing against the fingerboard on its way down, the "mwah" of a low action), **string and position choice** (the model knows which string a note is on, open strings ring longer and brighter, high positions are duller), **MIC or PICKUP** (body sound against the direct string), and **AGE** (dead strings, uneven partials, looser pitch).

How close is it? Across 19 measured notes the model's first six partials are within about 6 dB on average (it was 8 dB before fitting), and the level over time in three bands is within 4.4 dB. Most of what is left is the real instrument's note-to-note irregularity, which no smooth model has. Sketch 01 plays six real plucks each followed by the model on the same note, so you can judge directly. I have checked all of this by measurement and have not heard it.

## The pluck, second pass (after your note on the attack)

You were right on all three counts, and the first model had none of them. Zooming into the first 300 ms of the recordings:

- **The burst.** At the instant of release, everything above 1 kHz is 15 to 30 dB louder than it is a tenth of a second later. A string let go by a finger is a sharp corner, so for a moment every harmonic is present, falling off at about 6 dB per octave, and the high ones die within a few hundredths of a second. The first model filtered the pluck once and left it; its start was 12 dB too dull on average. The new one has the full corner and lets each harmonic die at its own rate (error now 7.6 dB, most of what is left is fine grain I am not chasing).
- **The drop, and you were right about 200 ms.** Half of the strong partials in the recordings fall and then come back: down by a median of 5 dB (some by 15 to 24 dB) around 80 to 150 ms, recovered by about 200 to 250 ms. One correction to the picture: it happens after the finger lets go, not before. The string moves in two directions at once, one drives the bridge hard and dies fast, and the two are tuned a few hertz apart, most strongly near the body's resonances at 90 to 160 Hz. They start together, cancel, and come back. The model now has both motions, and its partials dip and recover about as often and as deeply as the real ones.
- **A pitch component.** The first cycle of the note is long: flat by something like one to three semitones for one cycle, settling within two or three. My measurement of this is rough (the method itself scatters by a semitone on a signal this short), so the size is uncertain, but the direction is consistent in 11 of 14 recordings. The model starts each note about two semitones flat for one cycle.
- **Before the pluck.** The samples are trimmed at the release, so they cannot show the finger arriving. In a line it matters: the finger lands on the string about 50 ms before it plucks, and if that string is still sounding the old note it stops it dead, with a soft tick. The model now does this when two notes follow each other on the same string, and lets the old note ring over when they are on different strings.

Sketch 01 is now three sounds per note: the real pluck, the first model, the new model.

## The pluck, third pass (after "still closer to the first model")

Averaged level in each band over time was already close after the second pass, so I looked for what that measure cannot see, and found it: **in the real note a lot of the sound is not on the note's harmonics at all.** In the first 150 ms the energy between the harmonics is only 12 to 15 dB below the energy on them (low band) and 8 dB below (above 900 Hz). In the model it was 20 to 22 and 13 to 19 dB below, and 32 to 35 dB below later on. The model was too clean: a perfectly periodic buzz where the real thing has wood and wire in it. Three causes, all now modelled:

- **The body rings.** The first model shaped the tone with eight gentle resonances that did not ring. A real body is knocked by every pluck and sounds at its own pitches for a tenth of a second or more. The body is now 40 ringing resonances read from the eight knock recordings (65, 102, 146, 202, 222 Hz and up to 2 kHz, each with its measured ring time).
- **High partials are not in tune with the note.** A bass string is stiff, so its upper partials run progressively sharp (measured: the 20th partial about a sixth of a harmonic sharp, the 40th nearly half). That turns the burst from a buzz into a snap, because the high partials no longer line up cycle after cycle.
- **The burst dies in three stages, not one:** a snap gone in a hundredth of a second, a main part over about a tenth, and a faint ring that lasts. The highs above 3 kHz were 14 to 30 dB too quiet after the first 60 ms and are now within about 5 dB up to 6 kHz.

Between-harmonics error: 10 dB before, 3 dB now. Band-against-time difference: 3.0 dB before, 2.0 dB now. One measure got worse: individual harmonic levels are now within about 7.5 dB instead of 6, because the ringing body colours each note differently and I have not refitted the smooth curve under it. That is a job for the build spec.

**Fourth pass (after "should it die away faster?").** Yes. Measured in finer time steps, the body ring in the third pass was 6 to 7 dB too strong from 80 to 300 ms: I had taken each resonance's ring time from a bare knock, and in a played note the ring fades about three times faster. It is now stronger at the pluck and shorter. What is left under the note later on is not the knock but the other three strings humming along quietly, which the model now has too. Between-harmonics error: 3 dB before, 1.8 dB now.

There is also a single **ATTACK** amount on the whole pluck (burst, finger noise, body knock). Sketch 14 plays the same four bars with it at none, as measured, and exaggerated, so you can say where it should sit. It should be a panel knob.

## Sketches

In `sketches/`. Trios are Comper's own notes through Joanna, the new bass, and quiet General MIDI drums.

| Sketch | What it shows |
|---|---|
| 01 real, first model, new model | Six notes, three sounds each: the real bass, the first model, the new model with the reworked pluck |
| 02 bass alone | 24 bars of walking blues, nothing else |
| 03 trio medium swing | Comper preset 1 with the bass |
| 04 trio fast bebop | 208 bpm |
| 05 duo ballad | Piano and bass only, long notes and vibrato |
| 06 trio bossa | |
| 07 trio blues | Busier, with skips |
| 08 trio modal | ROAM up: the line travels further |
| 09 trio soul, 10 trio house | The least trusted feels |
| 11 activity | Two-feel, then walking, then busy |
| 12 loose | Machine, then player, then too loose |
| 13 tone | Gut and soft, steel and ringing, GROWL full, PICKUP only, AGE full |
| 14 pluck | The whole pluck at none, as measured, exaggerated |

## Proposed module

Working name **Walker**, a second module inside the Comper Fable plugin, about 26 HP.

**Following Comper.** Put it next to Comper and it follows with no cables: chart, bar position, tempo, swing, groove, and the chord that is coming next. Being in the same plugin is what makes that link simple. It also has cable inputs (ROOT, QUALITY, CLOCK, RESET) so it can follow something else; without the link it cannot see ahead, so in cable mode it learns the progression on the first pass and plays simpler approach notes until it has.

**Player.** FEEL (follow Comper, or one of the six), ACTIVITY, REGISTER, ROAM, OUT, REPEAT, SEED with NEW, and a LAY OUT gate input so the bass can drop out and come back in on a root.

**Hands.** LOOSE as on Comper, with trims TIMING, WEIGHT, SLOP, PITCH (intonation) and AHEAD.

**Sound.** ATTACK (the whole pluck, from none to exaggerated), PLUCK (where on the string), BRIGHT (gut and soft fingers to steel and hard), SUSTAIN, GROWL, BODY, MIC/PICKUP, THUMP, AGE, LEVEL.

**Outputs.** Audio, and V/OCT, GATE and VEL of the line. A switch chooses whether V/OCT carries the slides and vibrato or clean pitches.

**Presets.** 12, paired with Comper's twelve so preset 3 of one fits preset 3 of the other.

## Open questions

1. **Same plugin or its own?** I recommend a second module in Comper Fable, because the cable-free link needs both modules to share code. Alternative: its own plugin folder (your usual pattern), following Comper by cables only.
2. **Cable inputs in version 1?** I recommend yes, with the learn-on-first-pass behaviour, so it is useful without Comper.
3. **Should Comper react to the bass?** I recommend one small thing: when Walker is attached, Comper's ROOTS setting is ignored (a pianist stops playing roots when a bassist arrives). Nothing more in version 1.
4. **Bowed bass?** I recommend leaving arco out. It is a different model and jazz comping rarely needs it.
5. **Name.** Walker is my suggestion. Say if you prefer another.

A drummer (ride, hi-hat, brushes) would be the obvious third player and could follow the same link. I have not designed one.

Next step on your word: the build spec for Opus.
