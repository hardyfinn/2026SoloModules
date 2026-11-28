---
title: Kick drum tuning of the 606, 707, 808 and 909
date: 2026-11-28
source: Music production · Kick drum tuning
topic: Music
tags: [kick-drum, tuning, roland, keys]
summary: |
 The 808 and 909 kicks both settle near 49.5 Hz (a sharp G1), the 606 near 60 Hz, and the 707 is a fixed sample.
 No study shows house or techno keys cluster to suit the kick; EDM leans about 80% minor for other reasons.
---

# Kick drum tuning of the 606, 707, 808 and 909

## Summary
The 808 and 909 kicks both settle near 49.5 Hz (a sharp G1), the 606 near 60 Hz, and the 707 is a fixed sample.
No study shows house or techno keys cluster to suit the kick; EDM leans about 80% minor for other reasons.

## Question
> New thread, tuning the bass drum in house and techno. I'm interested to know the standard tuning for 606 and 707 bass drums. I think the tuning on the 808 is also fixed. I wonder if this has had an effect on the keys that house and techno is written in. We might develop a module that helps to tune the kick drum to suit the melody and the key of the track.

## Answer
Your hunch about fixed pitch is right. The link to song keys isn't proven, though. Here's the write-up: Kick drum tuning and track keys.

- **808:** the body sits at about 49.5 Hz, a slightly sharp G1. TONE and DECAY don't change the pitch, and there's no stock trimmer for it.
- **909:** it lands on the same 49.5 Hz whatever TUNE is set to. I measured this from a TR-909 sample pack. TUNE only sets how high the click starts, from about 67 Hz up to 164 Hz.
- **606:** about 60 Hz, between A#1 and B1. That figure comes from circuit analysis, not a recording.
- **707:** a fixed 8-bit sample that can't be tuned. It glides from about 83 Hz down to 47–50 Hz in a quarter of a second. I measured that from TR-707 bass drum recordings. [Later corrected: the last full cycle is about 58.5 Hz (A#1) and the whole hit lasts about 75 ms; see the write-up below.]
- **Keys:** nobody has shown that house or techno tracks favour keys that suit the kick. What has been measured is about 80% minor keys, with A minor most common in one chart count. Short kicks below 60 Hz barely read as a pitch anyway. Tuning really matters with long, boomy kicks.

[…]

### Full write-up

#### Short answer

The 808 and the 909 kicks both settle on about 49.5 Hz, a slightly sharp G1, and neither has a stock knob that moves that body pitch. The 606 kick rings at about 60 Hz (between A#1 and B1, from circuit analysis) and the 707 kick is a fixed 8-bit sample that falls from about 160 Hz to about 58.5 Hz (A#1) in a 75 ms hit, with no tuning at all. So your hunch is right that the classic Roland kicks are effectively fixed in pitch.

The second part is folklore so far. I found no study showing house or techno tracks bunch up in G, C or D to suit the kick. What is measured is a strong lean towards minor keys (about 80% in one house/EDM corpus) and A minor topping a Beatport chart count. Both have simpler explanations than kick pitch. A real, documented habit runs the other way in hip hop: producers transpose the whole song so a long 808 sits in tune and loud.

#### Measured pitches

Every classic Roland kick lands between about 47 and 60 Hz once the click is over, and only the 909 has a stock TUNE knob, which moves the attack sweep rather than the body.

| Machine | How the sound is made | Body pitch after the attack | Nearest note | Stock pitch control | How we know |
| --- | --- | --- | --- | --- | --- |
| TR-606 | Analogue, two bridged-T filters rung by a pulse | about 60 Hz (plus a 130 Hz click filter) | between A#1 and B1 | None (only level and the accent) | Circuit analysis by D. Schmidt, no measured recording |
| TR-707 | 8-bit sample, fixed playback clock | falls 160 > 118 > 72 Hz in 30 ms, last full cycle about 58.5 Hz; the whole hit is only about 75 ms | about A#1 (the BD2-style recordings end near F#1-G1); too short to read as a note | None ("the sounds cannot be tuned"); pitch only by aftermarket mod | Measurement of 8 TR-707 BD recordings, plus Wikipedia and the Circuitbenders mod guide |
| TR-808 | Analogue, bridged-T filter rung by a pulse, long decay | about 49.5 Hz | G1 about +17 cents | No pitch knob. TONE and DECAY change brightness and length, not pitch. No stock trimmer for BD pitch either | Deruty 2025 (37 Trisample samples, median 49.48 Hz) and Werner, Abel and Smith 2014 (circuit model 49.5 Hz; Roland's chart says about 56 Hz) |
| TR-909 | Analogue VCO with a pitch envelope | about 49.5 Hz at every TUNE setting | G1 about +15 cents | TUNE: sets how high the attack starts (about 67 Hz at 0, 94 Hz at noon, 164 Hz at full), the body still lands at 49.5 Hz | Measurement of a TR-909 sample pack recorded from one unit |

Three honest caveats:

- Unit-to-unit spread is real. The 808 used ±20% capacitors, which alone could move the pitch by several semitones from machine to machine. Roland's own chart says about 56 Hz, while the measured samples and model say 49.5 Hz.
- The 808 has a small "pitch sigh": it starts a touch sharp and falls about a semitone in the first 0.4 s. The 909 and 707 fall much further, but that happens inside the first 80 ms where the ear hears a click, not a note.
- Below about 60 Hz people are poor at hearing pitch, and short kicks give the ear little time. So a 909 or 707 kick that is "off" against the key is much less noticeable than a long 808 boom.

#### Did fixed kicks shape house and techno keys?

Not provably. The idea is plausible, but nobody has tested it, and the key data we have points to other causes.

**What is measured**

- A study of about 2,000 hand-annotated EDM excerpts (mostly house) found about 80% are in minor modes, with a lot of Phrygian, and many tracks use 6 or fewer pitch classes rather than full scales (Faraldo, Jordà and Herrera 2017). The authors put this down to looping and layering, not to drums.
- A count of one Beatport Top 100 found A minor the most common key and 65% minor (EDMProd 2015). That is 100 tracks, so it is a hint, not a result.
- Deruty (2025) shows the 808 sits at G1 and argues that moving it far down loses up to about 12 dB of perceived level on near-field speakers. That gives a real reason to keep a long 808 near where it is and move the song instead, which is the hip hop habit (Scott Storch's advice).

**What would have to be true for the theory to hold**

If kick pitch drove key choice, tracks built on 808 or 909 kicks would lean towards keys where G is a chord tone of the tonic: G, C, E♭, E minor or G minor. A minor (the reported favourite) is not one of those, though G is its flat seventh, which sits fine in minor and Phrygian grooves. Nobody has split key counts by kick source, so the test has not been run.

**Likelier influences on house and techno keys**

- Minor and modal loops on a few notes, which hide clashes with a drum.
- Short 909-style kicks below 60 Hz, where the ear barely hears pitch, so a mismatch rarely bothers anyone.
- Samplers (S950, SP-1200, MPC) and later DAWs let producers pitch the kick to the bass anyway, so a fixed machine pitch was never a hard limit.
- Hardware habits: preset sounds, white-key playing (A minor) and chord memory voicings.

**Where tuning clearly does matter**

Long kicks and sub-heavy tracks: a boomy 808, a 909 with DECAY up, or a kick that doubles as the bassline. That is where a kick that is a semitone off the bass beats against it, and where a tuning module pays off.

#### Sources

- [Deruty 2025, Harmonic and Transposition Constraints Arising from the Use of the Roland TR-808 Bass Drum](https://arxiv.org/html/2502.07524v1)
- [Deruty, Why adjusting the song's tonality to the kick drum sample may be a good idea](https://medium.com/@derutycsl/why-adjusting-the-songs-tonality-to-the-kick-drum-sample-is-a-good-idea-3905c6a68e1c)
- [Werner, Abel and Smith, DAFx-14, TR-808 bass drum circuit model](https://dafx14.fau.de/papers/dafx14_kurt_james_werner_a_physically_informed,_ci.pdf)
- [D. Schmidt, TR-606 bass drum circuit notes](https://users.monash.edu/~dschmidt/tr606/bd.html)
- [Roland TR-707, Wikipedia](https://en.wikipedia.org/wiki/Roland_TR-707)
- [Circuitbenders TR7X7 pitch mod kit guide](https://www.circuitbenders.co.uk/tutorials/circuitbenders%20TR7X7%20pitch%20mod%20kit.pdf)
- [Faraldo, Jordà and Herrera, A Study of Tonality in Electronic Dance Music (EUROMAC 2017)](https://creaa.unistra.fr/websites/gream/Activites/Euromac_2017_-_Postprint_-_Extended_abstract_-_FARALDO_Angel_-_HERRERA_Parfecto_-_JORDA_Sergi.pdf)
- [EDMProd, What I Learned from Analyzing the Top 100 Tracks on Beatport](https://www.edmprod.com/beatport-analysis/)
- [Ableton, Drop It: The Kick in Electronic Music](https://www.ableton.com/en/blog/drop-it-kick-electronic-music/)
- Own pitch measurements of a TR-909 sample pack (bass drum at TUNE 0/50/100) and of TR-707 BD recordings.

## Sources
- The conversation itself; no outside sources were cited.
