# Tape: one VCV Rack module for cheap 80s and 90s recording

Fable, 2026-11-23. Design stage only. Nothing is built yet.

## The short version

Vinylizer models what happened to a track at the pressing plant and on the turntable. This module models what happened before that: the bedroom or budget studio where the track was recorded and mixed. I propose one wide module that holds the whole of that path as bypassable sections on one panel:

**DESK → NOISE REDUCTION → MULTITRACK deck → BUS → MASTER deck**

with a MACHINE selector for each deck (cassette four-track, hi-fi cassette deck, boombox, budget reel-to-reel) and one **AGE** knob that takes the whole rig from freshly serviced to never serviced.

It is built the same way Vinylizer is: a matched record and playback pair, with every loss and every distortion happening in between, computed from real tape wavelengths in micrometres. A prototype of that model exists and 16 sound sketches from it are in `sketches/`. Start with `00-gallery.wav`.

One caution up front. I cannot hear the sketches. I checked them by measurement only (frequency response, distortion, level and treble balance), so your ears are the first real test. Tell me what is wrong and I will fit it.

## 1. What cheap recording actually was

The music you named was mostly not made on studio tape. Three setups cover nearly all of it.

**Two cassette decks.** No multitrack at all. Larry Heard on "Can You Feel It": "I had two cassette decks and I recorded a pass on one and then recorded to the other" (Red Bull Music Academy, 2017), and "I did one take, one pass, on one tape, then ran it back to the other one" (Vice). So the drums and bass on that record went through cassette twice and the parts played on top went through once. His own verdict: "Not exactly state-of-the-art, but effective."

**The cassette four-track.** Tascam 244 (1982), Porta One (1984), 424 (1990s), Fostex X-15, Yamaha MT series. Four tracks on a normal cassette, a small mixer built in, dbx or Dolby noise reduction. When four tracks were not enough you bounced three onto the fourth and carried on, losing quality each time. The mix then went to a second machine, usually a hi-fi cassette deck, later DAT.

**Budget reel-to-reel.** Fostex A-8 and R8 (eight tracks on quarter-inch tape), Tascam 38 and 388, mixed down to a quarter-inch two-track such as a Revox B77 or Tascam 32. This is the medium-fi end: clearly tape, much less damage.

Published figures for the real machines (sources at the end):

| Machine | Speed | Response | Noise, no NR | Wow and flutter | Noise reduction |
|---|---|---|---|---|---|
| Tascam 244 | 9.5 cm/s | 40 Hz to 14 kHz ±3 dB | not given | 0.04% RMS | dbx II |
| Tascam Porta One | 4.76 cm/s | not given | not given | not given | dbx |
| Tascam 424 MKII, normal speed | 4.8 cm/s | 40 Hz to 10 kHz ±3 dB | 55 dB | 0.06% | dbx II |
| Tascam 424 MKII, high speed | 9.5 cm/s | 40 Hz to 16 kHz ±3 dB | 55 dB | 0.05% | dbx II |

A 424 at normal speed is 3 dB down at 10 kHz when new and aligned, after one pass. That is the sound: not hiss and wobble first, but a soft top, treble that squashes on loud passages, thick low mids, and noise reduction that never quite tracks.

## 2. Why tape sounds the way it does

Seven things, in rough order of how much they matter on cheap machines.

1. **Treble saturates long before bass.** A cassette moves at 4.76 cm/s, so a 10 kHz wave is 4.8 micrometres long on the tape. Wavelengths that short are lost in the thickness of the coating, in the gap of the head and in any space between head and tape. The recorder makes up for it by boosting treble hard before it hits the tape. So the tape sees hats and sibilance 12 dB or more hotter than the bass, and they hit the ceiling first. Loud treble is compressed and dulled; quiet treble passes. This is the same principle as Vinylizer's record curve, and it is the heart of the model.
2. **Everything shares one ceiling.** A loud kick uses up the tape's headroom, so the hats on the same track duck under it for a moment. Tape "glue" is mostly this.
3. **Noise reduction mistracks.** dbx squeezes the signal 2:1 going on and expands it 1:2 coming off. Any error the tape adds in between is doubled: a 2 dB treble loss becomes 4 dB, a dropout becomes a hole, and hiss swells up behind each bass note and fades ("breathing"). Dolby B does a milder version to the treble only, and goes dull whenever the playback level is not what it expects. The Porta One review heard it: "the slight breathing as the noise faded up and down with the volume of the bass was audible, but not obtrusive."
4. **Generations.** Every bounce and every dub runs the whole chain again. The Porta One review: "second and third generation bounces are quite acceptable", after which "the sound quality begins to deteriorate".
5. **Azimuth.** The head is never exactly square to the tape. That costs treble on each track, and it also delays one stereo channel against the other by tens of microseconds, wandering as the tape weaves. In mono, the top end phases in and out. This is the swimmy treble of a cassette master.
6. **Speed.** Wow from the capstan and pinch roller, flutter from the motor and belts, a fine roughness from tape scraping over the heads.
7. **Dropouts and noise.** A speck of dirt lifts the tape off the head for a few milliseconds. Treble goes first because short wavelengths cannot reach across the space. Hiss, a little hum, and noise that rides on the signal itself (modulation noise, the "grain").

## 3. The proposed module

One module, about 50 HP like Vinylizer, text labels, stereo in and out. Each section has a bypass button and a light.

### DESK
The channel strip of a cassette multitracker. TRIM drives an op-amp stage that stays clean and then clips hard, which is the Portastudio overdrive sound. LOW and HIGH shelves at 100 Hz and 10 kHz, ±10 dB, as on the Porta One.

### NOISE REDUCTION
TYPE: OFF, DOLBY B, DOLBY C, dbx. ENCODE and DECODE have separate switches, because the misuses were part of the craft: record with Dolby and play back without it for a bright, compressed top; dbx without decode for a crushed, airless sound. A CAL trim sets the level mismatch between record and playback.

### MULTITRACK deck
- MACHINE: FOUR-TRACK (4.76 cm/s), FOUR-TRACK HI (9.5 cm/s), EIGHT-TRACK REEL (38 cm/s).
- REC LEVEL with VU meters and a peak light. This is the main performance control: low is clean and hissy, high is thick and squashed.
- TAPE: I (normal), II (chrome), IV (metal). Changes headroom, treble headroom and hiss.
- BIAS: under-biased is bright and gritty on quiet sounds, over-biased is smooth and dull.
- BOUNCE: 1 to 4 generations. The whole deck is run that many times in a row.

### TRANSPORT and HEAD (shared by the multitrack deck)
WOW, FLUTTER, AZIMUTH, DROPOUT, HISS, and WEAR (condition of the tape itself: shed oxide, less headroom, more dropouts).

### BUS
A budget VCA compressor of the kind every cheap studio had, with THRESHOLD, RATIO and a gain reduction light. This is the "minimal mastering" you mentioned: a compressor across the mix on the way to the master deck, and nothing else. Off by default.

### MASTER deck
A second, complete tape machine for the mixdown, with fewer controls: MACHINE (CASSETTE DECK, BOOMBOX, QUARTER-INCH TWO-TRACK, DAT), REC LEVEL, TAPE, and its own Dolby B switch. DAT is the clean 90s option: 16-bit with a steep filter and nothing else.

### AGE
One knob for the state of the whole rig. It scales, together: head wear and dirt, azimuth error and tape weave, worn pinch roller and belts (more wow), level calibration drift between record and playback (which makes the noise reduction mistrack more), channel imbalance, crosstalk, dropout rate, hiss, hum, and a magnetised head. At zero both machines are freshly serviced. I recommend a default around a quarter.

### Inputs and outputs
Stereo in and out, CV for REC LEVEL, AGE, WOW, BOUNCE and the two deck levels, a MIX control and output level. A **TRACKS** poly input and output on the multitrack deck: each channel of a poly cable becomes one track on the same tape, sharing one transport, leaking into its neighbours, with the outer tracks slightly worse, as on a real four-track.

### Left out on purpose
Varispeed and pitch control (a live effect cannot play faster than its input, the same limit as Vinylizer), tape stop and rewind effects, and tape echo. High-end studio machines are out by your brief.

## 4. The sketches

All in `sketches/`, from `tape_proto.py`, `songs.py` and `make_sketches.py` in this folder. Three dry loops (deep house, hip hop with 12-bit drums, techno with an acid line) were synthesised as separate stems so they could be recorded track by track. Every file is level-matched to its dry version.

| File | What it is |
|---|---|
| 00-gallery | Four bars each: house dry, four-track, four-track in the red; hip hop dry, four-track; techno dry, reel |
| 01, 06, 11 | The three dry loops |
| 02 | House. Drums, bass and keys on a four-track with dbx at sensible levels, mixed to a chrome cassette with Dolby B |
| 03 | The same with the desk trims and tape levels in the red |
| 04 | House by the two-cassette-deck method: drums and bass through tape twice, keys once, normal tape, no noise reduction |
| 05 | Bouncing: dry, then 1, 2, 3 and 4 generations, four bars each |
| 07 | Hip hop on a high-speed four-track, mixed to a normal cassette |
| 08 | Hip hop dubbed twice on a boombox |
| 09 | Noise reduction used right, then wrong three ways: Dolby record with no decode, Dolby decode only, dbx with no decode |
| 10 | AGE at 0, a third, two thirds and full, four bars each |
| 12 | Techno on the budget eight-track reel, mixed to quarter-inch. The medium-fi end |
| 13 | Techno, hot, on a four-track to normal cassette |
| 14 | Drums and bass only on worn tape: without noise reduction, then with dbx, to hear the hiss change and breathe |
| 15 | Worn tape and dirty heads |
| 16 | The full chain: four-track, budget compressor, cassette master |

What the prototype's four-track measures, new, one pass, no noise reduction:

| Level | 100 Hz | 1 kHz | 5 kHz | 10 kHz | 14 kHz |
|---|---|---|---|---|---|
| -20 dB | +1.3 | -0.1 | -0.4 | -1.0 | -4.7 |
| 0 dB (meter zero) | +1.0 | -0.5 | -1.4 | -3.7 | -8.2 |
| +6 dB | +0.1 | -1.5 | -3.6 | -7.8 | -12.7 |

So treble response depends on level, as it should. At high speed the same machine is 1.8 dB down at 14 kHz, and the eight-track reel 1.5 dB.

## 5. What is solid and what is not

Solid: the published figures in the table above, the equalisation standards (120 microseconds for normal tape, 70 for chrome and metal), 2:1 dbx with treble pre-emphasis and a band-limited detector, Dolby B giving up to 10 dB and Dolby C up to 20 dB, and the wavelength loss formulas, which are textbook.

Estimates I will check before the build spec:
- How much treble headroom each tape type really has. I tuned the model so 10 kHz saturates about 12 dB before 315 Hz on chrome at cassette speed. Published tape data sheets give this figure and I have not yet read one.
- The Fostex and Tascam reel machines. Their review pages would not load, so those numbers are from memory.
- Dolby B and C are simplified sliding filters with the right amount of boost at the right levels, not the licensed circuit. Dolby C in particular is crude in the prototype.
- Head bump frequencies, crosstalk levels and all of the AGE scalings are my judgement.
- The tape curve in the prototype has no memory. The build should add a small hysteresis term so quiet signals under-biased sound right.

## 6. Questions

1. **Two decks in one module?** I recommend yes: multitrack deck and master deck on one panel, as described, because the sound you are after is two passes through tape. The alternative is a narrower one-deck module that you use twice in a patch.
2. **The TRACKS poly input.** I recommend yes. It is what makes a patch behave like a four-track session instead of a stereo effect.
3. **Hiss.** Vinylizer had no crackle. Here I recommend a HISS knob that defaults to a realistic low level and turns fully off, because the breathing of noise reduction is part of the sound and needs some hiss to act on.
4. **Name and home.** I suggest **Tapeizer**, in a plugin called Tapeizer Fable, folder `TapeizerFable` in 2026SoloModules on its own branch, Apple silicon only, with a PDF manual and patch recipes. Other names: Ferric, Fourtrack.
5. **Machines.** Is there a particular machine you owned, or one behind records you love, that should be on the selector?

Say "go" to accept the recommendations and I will write the build spec for Opus.

## Sources

- Tascam 244 review, Electronics & Music Maker, August 1982: https://www.muzines.co.uk/articles/tascam-244/4323
- Tascam Porta One review, Home & Studio Recording, December 1984: https://www.muzines.co.uk/articles/tascam-porta-one-ministudio/4197
- Tascam 424 MKII specifications: https://homerecording.com/tas424specs.html
- Larry Heard on his first house tracks: https://daily.redbullmusicacademy.com/2017/02/larry-heard-fingers-inc and https://www.vice.com/en/article/larry-heard-can-you-feel-it-making-of
- dbx noise reduction: https://en.wikipedia.org/wiki/Dbx_(noise_reduction)
- Tape noise reduction, Sound On Sound: https://www.soundonsound.com/techniques/tape-noise-reduction
- Cassette tape types: https://en.wikipedia.org/wiki/Compact_Cassette_tape_types_and_formulations
