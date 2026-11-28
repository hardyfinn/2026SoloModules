---
title: "David Tudor's Rainforest, Pulsers and Phonemes"
date: 2026-11-15
source: "Fable 5.2 · Design David Tudor-style VCV modules"
topic: Music
tags: [david-tudor, rainforest, live-electronics, feedback]
summary: |
 Rainforest sends sound through cone-less transducers into real objects, Pulsers is a Gordon Mumma modulator Tudor altered that runs on phase-shifted feedback with no input, and Phonemes pairs a long-attack percussion generator with a chopping vocoder.
 The Tudor piece is called Pulsers, not Pulsars.
---

# David Tudor's Rainforest, Pulsers and Phonemes

## Summary
Rainforest sends sound through cone-less transducers into real objects, Pulsers is a Gordon Mumma modulator Tudor altered that runs on phase-shifted feedback with no input, and Phonemes pairs a long-attack percussion generator with a chopping vocoder.
The Tudor piece is called Pulsers, not Pulsars.

## Question
> new thread - developing vcv rack modules in the style of David Tudor's rainforest, pulsars and phonemes

## Answer
I've written up how the three pieces worked and a proposal for 12 modules, in [01-research-and-proposal.md](/mnt/project-files/tudor-vcv/01-research-and-proposal.md). (The Tudor piece is called *Pulsers*.)

**How the pieces worked:**
- **Rainforest:** sound goes through cone-less transducers into real objects, such as a bedspring, a wine barrel or a copper still. You hear each object in the room, and a pickup on it is also amplified through a speaker.
- **Pulsers:** a modulator by Gordon Mumma that Tudor altered. It has no input; "phase-shifted feedback is the trigger source", and its timing can drift until it becomes unstable.
- **Phonemes:** a percussion generator whose attacks can stretch to several seconds, plus a vocoder that chops sound into small pieces.

**The 12 modules, grouped by piece:**
- **Rainforest:** OBJECT, a modelled object with two transducer inputs and separate room and pickup outputs. CRITTER makes animal-like source sounds. SEEK finds an object's resonances and plays them back into it. WANDER is a listener walking among the objects.
- **Pulsers:** PULSER makes rhythm from feedback with no input. SHIFT, TUNED and SIDEBAND are copies of Tudor's own homemade parts. LOOP is a feedback matrix that keeps a patch on the edge of oscillating instead of dying out or blowing up.
- **Phonemes:** SWELL lengthens short sounds, PHONEME is the chopping vocoder, and XGATE cross-gates sounds and spreads them over 4 to 6 output channels.

The document also lists existing VCV modules worth using alongside these, and why.

**Four choices are yours. My default is in brackets:**
1. Plugin name [*Chocorua*, the place where Rainforest IV started].
2. Whether OBJECT can load your own contact-mic recording of a real object and turn into it [yes].
3. All 12 modules, or a first round of 6 [all 12].
4. A new GitHub repo for the build, e.g. `hardyfinn/Chocorua`. Please create it when you're ready.

Tell me what you'd change, or just say go, and I'll write the build spec for Opus on these defaults.

## Sources
- Project file: `/mnt/project-files/tudor-vcv/01-research-and-proposal.md`

Related: [[Mark Fell's timing and synthesis methods]], [[Autechre's Max system and post-2010 eras]]
