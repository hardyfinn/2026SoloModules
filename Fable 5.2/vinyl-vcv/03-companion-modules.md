# Vinylizer Fable: companion modules

Fable, 2026-11-21. Design stage, nothing built. The facts about pressing plants, masters and cheap decks below are from memory and have not been checked against sources. Opus should look up the figures marked "check" before building.

## What the main module leaves out

Vinylizer Fable models a good record, well cut, on a decent deck. The 80s and 90s also had:

1. **Bad pressings.** Recycled vinyl, worn stampers, patches that didn't fill, thin discs, dust, scratches.
2. **The master before the lathe.** Analogue tape, early digital (1630, DAT), CD masters cut to vinyl, and the digital delay line inside many lathes.
3. **The deck as an instrument.** Pitch fader, wrong speed, start and brake, back-cueing, skips and stuck grooves. A live effect can't do these, because it can't play audio faster than it arrives. A module that holds the record in memory can.
4. **The room.** Bass from the speakers shaking the stylus, footfall, motor rumble, mains hum.
5. **Cheap playback.** The ceramic cartridge and autochanger of an 80s music centre.

Five modules cover these. Three attach to Vinylizer Fable as expanders, because their effect has to happen inside the groove or at the cartridge. Two are separate modules that patch in front of it.

The rule from the first design still holds: nothing is pasted on top of the audio. Noise and defects are put into the groove as physical shapes and then read by the same stylus, cartridge and phono stage as the music, so the chain shapes them the way it shapes everything else.

---

## 1. PRESSING (expander, about 14 HP)

The pressing plant and the life of the disc. This is the "pretty poor quality pressing" module.

**How it works.** It adds shapes to the groove wall inside Vinylizer Fable, before the stylus reads it. Surface noise is random roughness of the wall, mostly vertical. The stylus tip filters it by its size (a fine line tip reads more of it than a spherical one), the playback curve turns it into the familiar low "rush", and because it is mostly vertical it largely cancels when the output is summed to mono. All of that falls out of the existing model without being drawn in by hand.

| Control | What it does |
|---|---|
| STOCK | The material. VIRGIN (quiet), REGRIND (recycled vinyl with paper and old-label contamination: more rush, plus occasional gritty patches), STYRENE (injection-moulded 7" singles: bright when new, wears very fast under a heavy spherical tip), FLEXI (thin floppy disc: low cut level, weak bass, heavy wow), PICTURE (picture disc: a constant coarse hiss from the thin clear layer over the print). Each sets roughness, hardness and wear rate. Hardness feeds the main module's contact-patch treble loss. |
| NOISE | Amount of wall roughness. Part of it is steady, part rises where the groove is heavily modulated, so the noise "breathes" with loud passages the way a rough pressing does. |
| NON-FILL | Patches where the vinyl didn't flow fully into the stamper. Gives a short tearing "zzzt" at the same point of each turn for a few turns, then moves on. Worse on loud, wide grooves. A classic fault of rushed pressing cycles (check where on the side it is most common). |
| STAMPER | How many discs this stamper has already pressed (0 to about 3000, check). A tired stamper rounds off the shortest groove shapes (treble loss that is worse near the label), raises distortion on loud treble and raises the noise. Position 0 is a test pressing. |
| WEIGHT | Disc weight, 90 to 180 g. Thin 80s discs warp more and ring: the disc itself resonates under the stylus and smears the bass slightly. Sets the size of the warp in the main module's ARM section and adds the ring. |
| DUST | Fluff collects on the stylus as the side plays: a slowly growing treble loss, then mistracking. CLEAN button and trigger input blow it off. Also sets the rate of small bumps in the groove (dust particles), each a real displacement read by the stylus, so their tone depends on the tip, cartridge and phono stage chosen. |
| SCRATCH | A radial scratch: one tick per turn at a fixed angle, with the thump of the arm resonance behind it. ADD trigger makes a new one at the current needle position. A deep one can throw the stylus (see SKIP out). |
| CUE BURN | Wear concentrated where a DJ back-cued: the first seconds of the side and any point marked with the MARK button. Duller and noisier there only. |

Jacks: CV for NOISE, DUST and STAMPER. Outputs: **ROTATION** (one trigger per turn, to sync anything to the record) and **SKIP** (a gate when the stylus leaves the groove, from the main module's mistracking and from deep scratches).

Presets: Test pressing, 1983 budget reissue (regrind, 95 g, late stamper), Chart 7" late in the run, Flexi from a magazine, White label 12", Charity-shop LP (dust, scratches, cue burn off).

## 2. DECK (separate module, about 16 HP, patched before Vinylizer Fable)

The turntable as something you play. It records its input into memory as "the side" and plays it back with a physical platter, which makes everything speed-related possible.

| Control | What it does |
|---|---|
| RECORD / LIVE | RECORD captures the input as the side (up to about 25 minutes). LIVE keeps a short rolling buffer so the deck effects still work on a live patch, within the last few seconds. |
| START / STOP | The motor. Direct drive reaches speed in under a second (check: about 0.7 s for a Technics SL-1200); belt takes about two seconds and overshoots slightly. BRAKE sets the stop time. Gate input. |
| POWER | Switches the motor off while playing: the long, sagging spin-down. |
| PITCH | Plus or minus 8% (16% option), a real speed change: pitch and tempo together. CV input. |
| 33 / 45 | Play a 33 at 45 or the reverse. |
| NUDGE | Push or drag the platter edge: brief speed-up or slow-down that settles back, for beat-matching feel. |
| SCRATCH | CV input that moves the record by hand (position), with a TOUCH gate. The platter keeps turning under the slipmat and the record catches up when released. |
| DROP | Needle drop: choose the point on the side and lower the arm. A soft thump, then a moment of lead-in groove before the music. |
| SKIP in | From PRESSING's SKIP out. The stylus jumps one groove: forward (a bit of music is lost) or back. STICK makes it jump back every turn, the stuck-record loop of exactly one revolution (1.8 s at 33, 1.3 s at 45), until NUDGE frees it. |
| RUN-OUT | At the end of the side the stylus falls into the locked run-out groove and loops it until lifted. |

Outputs: audio, **NEEDLE** CV (patch to Vinylizer Fable's NEEDLE input so inner-groove distortion follows the playing position), ROTATION.

Why it goes before Vinylizer Fable: the cartridge resonance and stylus tracing must act on the pitched-up audio, as they do on a real deck. The cutting stages then see the pitched audio too, which is not strictly right, but the record and playback curves cancel as a pair, so the error is small. When DECK is in use, switch Vinylizer's own PLATTER wow to taste: DECK supplies start, stop and pitch, PLATTER still supplies eccentric wobble and flutter.

## 3. MASTER (separate module, about 12 HP, patched before Vinylizer Fable)

What the cutting engineer was handed. A large part of the difference between a 1982 record and a 1996 one is here, not on the lathe.

| Control | What it does |
|---|---|
| SOURCE | HALF-INCH TAPE (30 ips: clean, extended), QUARTER-INCH TAPE (15 ips: head bump in the bass, softer top, more compression), 1630 (the 80s CD mastering format: 16-bit, 44.1 kHz, steep filters), DAT (90s: 16-bit, 44.1 or 48 kHz, the usual source for dance 12"s), CD MASTER (the already limited CD master sent to the lathe). |
| TAPE | Drive into the tape: treble saturates first, bass thickens. With PRINT, loud passages print through adjacent tape layers and give a faint echo before and after (a second source of pre-echo, with a different delay from the groove kind, about one turn of the tape reel). |
| NR | Noise reduction decoding error (Dolby A or SR alignment off by a dB or two, check): treble that pumps slightly with level. Very 80s. |
| CONVERT | Quality of the digital converters: early ones with steep filters that ring near 20 kHz and mild level-dependent distortion, to later clean ones. BITS switches 14 / 16. |
| EMPHASIS | Early digital masters sometimes carried a treble pre-emphasis flag. Decoded wrongly, the transfer came out bright and thin (or dull). Three positions: correct, missed (bright), doubled (dull). |
| DELAY LINE | Many lathes needed the audio a moment early to set groove spacing. With no preview tape head, the cutting signal itself went through a digital delay, so plenty of "analogue" 80s LPs passed through an early 16-bit converter. ON adds that pass. |
| LOUD | CD-style peak limiting before the cut, 0 to 10 dB. Flat-topped waves make the lathe's HF limiter and the playback chain work much harder, the sound of a mid-90s CD master cut to vinyl. |

Tape hiss is included as part of the source (it is on the master, not pasted over the output), and it goes through the whole record chain afterwards.

## 4. BOOTH (expander, about 10 HP)

The room the deck is standing in. It shakes the cartridge inside Vinylizer Fable, so everything it adds goes through the cartridge, arm resonance and phono stage.

| Control | What it does |
|---|---|
| RETURN in | Patch the mix (what the speakers are playing) back in. |
| FEEDBACK | How much speaker sound reaches the deck. Bass blooms and, turned up, the system howls at the arm or plinth resonance. DISTANCE sets the delay from speaker to deck. |
| ISOLATION | What the deck stands on, from a concrete block to a wobbly table. Sets a low resonance and how much footfall and feedback get through. |
| FOOTFALL | Trigger input and amount: a thump through the floor that excites the arm resonance. Enough of it and the stylus skips (feeds SKIP). |
| RUMBLE | Bearing and motor noise: low, mostly vertical, steady components at the turn rate and motor frequency plus a random part. Mostly vertical means it also thins out in mono. |
| HUM | Mains hum picked up by the cartridge coil (50 Hz and harmonics, 60 Hz option). More with the high-inductance DJ cartridge, and on decks with the transformer under the platter it rises as the arm moves toward the label, so it follows NEEDLE. |
| LID | The dust cover as a resonator when closed: a hollow colour on feedback and footfall. |

## 5. MUSIC CENTRE (expander, about 10 HP)

The cheap 80s way to play a record. Switched in, it replaces Vinylizer Fable's CARTRIDGE and PHONO stages.

| Control | What it does |
|---|---|
| CERAMIC | A ceramic cartridge responds to how far the groove moves, not how fast, so it needs no phono stage and only roughly follows the playback curve: a few dB wrong in broad swells, with a peak around 8 to 10 kHz (check). Tracks at 4 to 6 g with a blunt tip (so the main module's contact loss and wear go up), and has about 15 dB of stereo separation. |
| LOAD | The input it feeds. Too low an impedance and the bass disappears, a common fault. |
| CHANGER | An autochanger deck: idler-wheel rumble and wow, plus STACK, records stacked on the spindle slipping on each other for a slow extra wow. |
| TONE / LOUDNESS | Bass and treble controls and the LOUDNESS button every music centre had. |
| AMP | A small power amp that clips softly when pushed, with a little mains ripple on loud bass. |

---

## Changes to the main module

- Expander link, so PRESSING, BOOTH and MUSIC CENTRE can reach the groove and the cartridge. No panel change.
- Nothing else. SKIP and ROTATION outputs live on PRESSING and DECK.

All five would live in the same plugin as Vinylizer Fable, so they appear together in the browser and the expanders work.

## Suggested order

1. **PRESSING**: it is what was asked for and it changes the sound most.
2. **DECK**: adds everything a live effect can't do, and turns the set into an instrument.
3. **MASTER**: the era switch, 1982 versus 1996.
4. **BOOTH**, then **MUSIC CENTRE**.

## Considered and left out

- A cassette-copy-of-the-record module: true to the era, but it is a tape module, not a vinyl one.
- A DJ mixer (EQ, crossfader, overdriven outputs): useful, but Rack already has mixers, and Vinylizer's PHONO has the DJ input stage.
- A jukebox: its sound is styrene singles, heavy tracking and a ceramic or cheap magnetic cartridge, all covered by PRESSING plus MUSIC CENTRE.

## Questions for Hardy

1. Which of the five to build, and in what order?
2. PRESSING includes dust ticks and scratches as physical bumps read by the stylus. The first brief said no pops and crackles. Keep them in this module, or leave them out and keep only noise, non-fill, stamper wear and the rest?
3. Are expanders acceptable now? The first choice was one wide module with no expander, but three of these need to reach inside it.
