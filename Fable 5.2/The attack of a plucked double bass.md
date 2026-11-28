---
title: "The attack of a plucked double bass"
date: 2026-11-23
source: "Fable 5.2 · Design jazz trio players"
topic: Music
tags: [double-bass, physical-modelling, acoustics, transients]
summary: |
 A real double bass pluck starts with a bright burst, then strong partials dip around 80 to 150 ms and recover by 200 to 250 ms, and the note starts flat and settles within a few cycles.
 Much of the real sound sits between the harmonics (body knock and other strings humming), and the body ring in a played note fades about three times faster than in a bare knock.
---

# The attack of a plucked double bass

## Summary
A real double bass pluck starts with a bright burst, then strong partials dip around 80 to 150 ms and recover by 200 to 250 ms, and the note starts flat and settles within a few cycles.
Much of the real sound sits between the harmonics (body knock and other strings humming), and the body ring in a played note fades about three times faster than in a bare knock.

## Question
> Before building, Please try again with the attack phase of the sound. I wonder if I can distinguish an amplitude change when the finger plucks the string. So it starts with a loud amplitude and then there is a sudden drop before the finger lets the string go. Maybe analyze looking for this. And similarly, that finger pluck carries with it a burst of energy that is missing from the smoother synthesized sounds.
>
> It might even be 200 milliseconds.
>
> Maybe there's even a pitch component to that pluck.

## Answer
You heard right on all three, and the first model had none of them. The pluck is reworked and all 13 sketches are re-rendered.

- **Burst:** at release, everything above 1 kHz is 15 to 30 dB louder than a tenth of a second later. The first model was 12 dB too dull there. The new one starts with every harmonic and lets the high ones die in a few hundredths of a second.
- **Drop, and 200 ms is right:** half the strong partials fall by a median of 5 dB (some by 15 to 24 dB) around 80 to 150 ms and recover by 200 to 250 ms. One correction: this happens after the finger lets go. The string moves in two directions tuned a few hertz apart, which cancel and come back. The model now has both.
- **Pitch:** the first cycle is long, so the note starts flat and settles within two or three cycles. I measured one to three semitones, but that measurement is rough. The model uses two.
- **Before the pluck:** the samples are trimmed at the release, so this part is not measured. I added the finger landing about 50 ms early and stopping the old note when it is on the same string.

Sketch 01 now plays each note three ways: real, first model, new model. Still checked by measurement only, not by ear. The [proposal](/mnt/project-files/jazz-bass-vcv/01-research-and-proposal.md) has a new section on this.

## Question 2
> I think the effect you have found could be emphasised more. In sketch 01, new model is somewhat better but still closer to first model than to the real instrument

## Answer 2
You were right again, and I found what my measurements were missing. The model was too clean. All sketches are re-rendered.

In the real note a lot of the sound is not on the note's harmonics at all. The model had 5 to 18 dB less of that in-between sound, so it buzzed where the real bass knocks and snaps. Three fixes:

- **The body now rings.** It is 40 resonances taken from the knock recordings, each ringing for its measured time, hit by every pluck.
- **High partials run sharp,** as a stiff string's do. The burst no longer lines up cycle after cycle, which turns a buzz into a snap.
- **The burst dies in three stages** and its highs linger. Above 3 kHz the model was 14 to 30 dB too quiet after the first 60 ms.

The in-between gap went from 10 dB to 3 dB. One measure slipped: individual harmonic levels are within about 7.5 dB, was 6. I'd fix that in the build spec.

Sketch 01 is real, first model, new model. New sketch 14 plays the pluck at none, as measured and exaggerated, and I'd make that an ATTACK knob. Still not checked by ear, so tell me where 14 should sit.

## Question 3
> Better, but not quite right. Could it be that the added effect should die away faster than it does in the latest model?

## Answer 3
Yes, it should die faster, and the sketches are re-rendered with that.

Measured in finer time steps, the body ring was 6 to 7 dB too strong from 80 to 300 ms. I had taken each resonance's ring time from a bare knock, and in a played note it fades about three times faster. It is now stronger at the pluck and much shorter.

What remains under the note later on in the real recordings is the other three strings humming along quietly, so I added that. The burst's main part is also a little shorter.

The between-harmonics error is 1.8 dB, down from 3. Sketch 01 is still real, first model, new model, and 14 is the pluck at none, measured and exaggerated. Still not checked by ear.

## Sources
- Project file: `/mnt/project-files/jazz-bass-vcv/01-research-and-proposal.md`

Related: [[Making a synth module feel analogue]]
