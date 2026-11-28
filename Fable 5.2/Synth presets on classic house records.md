---
title: "Synth presets on classic house records"
date: 2026-11-21
source: "Fable 5.2 · Design house keys voice"
topic: Music
tags: [house-music, detroit-techno, dx100, synth-presets]
summary: |
 Solid Bass and Jazz Organ are factory voices B01 and A13 on the Yamaha DX100, and 4-operator Yamaha FM is the preset sound of Chicago and Detroit from 1987 to 1995.
 Only the DX100 sounds are true presets; Saunderson, Derrick May and Mr. Fingers otherwise programmed their own patches on machines like the Juno-106, Jupiter-6, Casio CZ, Ensoniq Mirage and SQ-80.
---

# Synth presets on classic house records

## Summary
Solid Bass and Jazz Organ are factory voices B01 and A13 on the Yamaha DX100, and 4-operator Yamaha FM is the preset sound of Chicago and Detroit from 1987 to 1995.
Only the DX100 sounds are true presets; Saunderson, Derrick May and Mr. Fingers otherwise programmed their own patches on machines like the Juno-106, Jupiter-6, Casio CZ, Ensoniq Mirage and SQ-80.

## Question
> Lots of classic house records from the late 80s and early 90s use presets, for example, solid bass and jazz organ. Please search for presets that were used commonly. I am particularly interested in Chicago and Detroit records from 87 to 95.

## Answer
Both of your examples are the same machine. Solid Bass and Jazz Organ are factory voices B01 and A13 on the Yamaha DX100, and the search says 4-operator Yamaha FM is the preset sound of Chicago and Detroit from 1987 to 1995.

**What I found**
- **DX100 (also DX21, DX27, FB-01):** Solid Bass, Jazz Organ, Club Organ, Ham n Eggs, 6 Tease, the electric pianos, Strings and Voices. Kevin Saunderson says the "Big Fun" bass is a DX100 and that Derrick May "used it a lot".
- **TX81Z:** LatelyBass, a beefier variation of Solid Bass. MK says it was the reason everyone bought one.
- **Korg M1 (from 1989):** Organ 2 ("Show Me Love", "Gypsy Woman") and Piano 16' ("Vogue", "Ride on Time", "Finally").
- **Roland JX-8P:** Marshall Jefferson's keyboard for nearly everything, including the "Soundtrack" strings. The "Move Your Body" piano is a Prophet 2000 sampler.
- **Juno-60 and Jupiter-6:** Larry Heard's "Can You Feel It" and "Mystery of Love", programmed rather than presets.

One gap: I confirmed Jazz Organ as a preset but found no reliable credit tying it to a named record. The gear forums where that is discussed blocked me.

**Why this is good news for realism:** the FM presets can be exact. A DX100 voice is about 70 numbers on a known chip, so the module can play the real factory voice data through a model of the chip and its gritty converter. The M1 sounds are Korg's samples, so those I would rebuild by synthesis fitted to recordings.

The full list, sources and a first module proposal are in [01-research-and-proposal.md](/mnt/project-files/house-keys-vcv/01-research-and-proposal.md).

**Four questions before I write the build spec**
1. Should the module cover FM plus the M1 organ and piano, with analog strings and pads as a later second module? That is my recommendation.
2. For the M1 sounds, is synthesis only right, or should it also load your own samples?
3. Which records are your reference for Jazz Organ and Solid Bass? I will fit the models to those.
4. Is HOUSE KEYS the name?

## Question 2
> I'm not interested in the M1 sounds at all, so remove those. I am interested in anything from Derek May and Kevin Saunderson, Mr. Fingers too.

## Answer 2
The M1 is out and the first 100 sounds are listed in [02-revised-proposal-100-sounds.md](/mnt/project-files/house-keys-vcv/02-revised-proposal-100-sounds.md), built around what the three of them actually owned.

**What the interviews say**
- **Kevin Saunderson:** "My bass lines really came from the Yamaha DX-100", including "Big Fun". He used the Juno-106 "a lot", made the Reese bass on a Casio CZ-5000, and built the "Good Life" stab from a sampled chord with a string layer. He also had a Mirage, a JX-8P and a Poly-800.
- **Derrick May:** shared that gear, and the DX100 above all. Attack Magazine puts the "Strings of Life" strings on a Mirage, which matches what you said.
- **Mr. Fingers:** a Jupiter-6 for "Mystery of Love" and "Washing Machine", and a Juno-60 for "Can You Feel It", recorded straight to cassette.

One finding matters for the design. Only the DX100 sounds are true presets. Saunderson said in 1988 that presets make you sound like everybody else, and Larry Heard altered his patches and never saved them. So the module models their machines and ships patches programmed the way they used them.

**The plan: five engines, 100 sounds**
- **FM (30):** the DX100 factory voices, including Solid Bass and Jazz Organ, on a model of the real chip.
- **POLY (24):** Juno, Jupiter and JX sounds, for Mr. Fingers's bass and chords and Saunderson's Juno-106.
- **PD (12):** the Casio CZ, for the Reese bass and "Good Life".
- **WAVE (22):** the SQ-80.
- **SAMPLER (12):** the Mirage chain, for the "Strings of Life" strings and piano.

It also has a STAB mode, where one key plays a whole chord like a sampled stab, and a TAPE stage for the cassette sound.

I cannot listen to records here, so sounds named after a record are built from the documented machine and how it was used. If you upload short clips of the sounds you care about most, I can fit those by measurement instead.

Two things would help before I write the build spec: whether all five engines stay, and the module's name. I will use HOUSE KEYS unless you prefer another.

## Question 3
> The nsonic mirage is a sampler Derek May used. I also like the nsonic SQ80 sounds. Can we recreate some of those?
>
> I'd be happy to deal with circa 100 sounds in the initial module, which I then review and reduce.

## Answer 3
Yes to both, and 100 sounds is a good target.

**SQ-80:** very recreatable. Each voice is three digital wave oscillators into an analog resonant filter, and a patch is a list of numbers, so the factory patches can be played as they were. The one limit is the wave memory, which is Ensoniq's. I would rebuild the waves by synthesis. That will be very close for the synthetic waves and a little less exact for the short sampled attacks.

**Mirage:** the samples on its disks are Ensoniq's too, but much of its character is the machine itself. It is 8-bit, runs at low sample rates with no smoothing, and feeds an analog filter. I would model a piano and strings and play them through a model of that chain.

I am now building the 100-sound list around Derrick May, Kevin Saunderson and Mr. Fingers, with the M1 removed.

## Sources
- Project file: `/mnt/project-files/house-keys-vcv/01-research-and-proposal.md`
- Project file: `/mnt/project-files/house-keys-vcv/02-revised-proposal-100-sounds.md`

Related: [[Twenty VCV module ideas for house and techno]]
