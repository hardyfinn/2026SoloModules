---
title: "Twenty VCV module ideas for house and techno"
date: 2026-11-21
source: "Fable 5.2 · Suggest next 20 VCV modules"
topic: Music
tags: [module-ideas, house-music, techno, vcv-rack, generative-music]
summary: |
 Twenty module ideas in four groups: weighted generators, groove and timing, character processors in the Vinylizer family, and voices.
 The suggested starting point is a weighted rhythm generator, a groove module and a vintage sampler, which together give a drum pattern that sounds right before any tweaking.
---

# Twenty VCV module ideas for house and techno

## Summary
Twenty module ideas in four groups: weighted generators, groove and timing, character processors in the Vinylizer family, and voices.
The suggested starting point is a weighted rhythm generator, a groove module and a vintage sampler, which together give a drum pattern that sounds right before any tweaking.

## Question
> New thread, next modules. I am looking for 20 suggestions of modules we could build in VCV Rack. I have been impressed by the weighted randomization module we built, which assigns higher probability of important notes in the key triggering more often than less important notes, i.e. the root will occur more than the sixths. I'm also impressed by the vinyl simulation that was built. My core interests in music are around house music in 1980s, 90s, techno in the 80s and 90s, acid house in the 80s and 90s, plus hip-hop, reggae, funk and soul. The vinylizer is particularly good as it works across these genres, but any suggestions for models or families of models would be most welcome. Ask me questions.

## Answer
Here are 20 suggestions, grouped by what they do. I leaned on the two things you said impressed you: modules that put musical knowledge into the probabilities, and character processors that work across all your genres. I have left out anything you already have (303, cymbals, FM4, KIT, vinyl and so on).

**Weighted generators (same idea as your note randomiser)**
1. **Weighted rhythm generator.** The same principle applied to time. Strong positions in the bar fire more often than weak ones, with weighting tables per style: four on the floor, backbeat, reggae one drop, funk sixteenths, clave.
2. **Weighted bassline generator.** Follows a chord input and picks root, fifth, octave and approach notes by importance, with rhythm styles for house, funk and reggae.
3. **Chord progression walker.** Moves between chords by weighted likelihood (soul and deep house movements such as ii-V, minor 7th and 9th shifts), and outputs a poly chord plus the root.
4. **Motif and variation generator.** Makes a short riff, then repeats it with small weighted changes, since house and techno riffs are mostly repetition with a twist.
5. **Fill and ghost note generator.** Sits after a drum pattern and adds fills at the end of phrases and ghost notes in between, more likely at bar 4, 8 and 16.
6. **Weighted break chopper.** Load a breakbeat, and it re-orders the slices while keeping kicks and snares where they belong most of the time.

**Groove and timing**
7. **Groove module.** Swing and micro-timing in the manner of the MPC60, SP-1200, TR-909 shuffle and a slightly late human drummer, applied to any clock or gate, with matching accent patterns.

**Character processors (in the family of Vinylizer)**
8. **Vintage sampler.** Run any sound through 12-bit sampler behaviour: the gritty pitch-down aliasing of the SP-1200 and the smooth filter of the S950.
9. **Tape.** A companion to Vinylizer covering cassette four-track, reel-to-reel and well-used master tape.
10. **Dub echo.** Tape echo with spring reverb in the feedback path, filter in the loop and safe runaway feedback, in the manner of the Space Echo.
11. **DJ isolator and filter.** Three-band rotary isolator with full kills, plus a DJ-style high and low pass sweep.
12. **Pump compressor.** Bus compression with the cheap, obvious pumping of 90s house, with a sidechain input and a ducking CV output.
13. **Overdriven desk.** A small mixer whose channels saturate like a hot Mackie, with mute buttons and two sends for dub-style mixing.
14. **80s and 90s reverb.** Early digital reverb with gated and reverse modes, plus plate and spring.
15. **Chorus and ensemble.** Juno chorus I and II, Dimension-style widening and string ensemble in one module.

**Voices**
16. **909 drum family.** Kick, snare, clap, rim and toms modelled from the circuits, to sit alongside your cymbal module.
17. **House keys.** The organ bass, the house piano and the FM "solid bass" in one playable poly voice.
18. **Electric piano and clavinet.** Physical models of Rhodes, Wurlitzer and Clavinet for soul, funk and deep house chords.
19. **Organ and rotary speaker.** Drawbar organ with a Leslie model, where the Leslie also works alone as an effect. Good for reggae bubble and soul.
20. **Rave and Detroit voice.** Hoover, Reese bass, chord-memory stabs and string machine pads.

If you asked me where to start, I would pick 1, 7 and 8, because together they give you a drum pattern that sounds right before you have touched anything.

My questions:
- Do you make whole tracks inside Rack, or do you use Rack to generate material that you finish elsewhere?
- Are you happy for modules to load your own samples (breaks, drum hits), or do you prefer everything synthesised?
- Do you play parts from a keyboard, or is it nearly all sequenced and generated?
- Which is missing most in your patches right now: generators, effects or voices?
- Are there particular records or machines you would like something to sound like?

## Sources
- None cited in the answer.

Related: [[Modelling a Mackie CR-1604 channel from recordings]], [[Synth presets on classic house records]]
