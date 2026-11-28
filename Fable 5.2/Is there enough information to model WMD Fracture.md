---
title: "Is there enough information to model WMD Fracture"
date: 2026-11-21
source: "Fable 5.2 · Assess WMD Fracture for VCV emulation"
topic: Music
tags: [wmd-fracture, clap, particle-synthesis, emulation]
summary: |
 There is enough to build a module that behaves like WMD Fracture: its manual describes a pseudorandom particle oscillator firing a 22-voice player of micro-samples, and explains every control.
 Its 23 surface recordings are unpublished and should not be copied, so the particles would be modelled instead, for example from published hand-clap research.
---

# Is there enough information to model WMD Fracture

## Summary
There is enough to build a module that behaves like WMD Fracture: its manual describes a pseudorandom particle oscillator firing a 22-voice player of micro-samples, and explains every control.
Its 23 surface recordings are unpublished and should not be copied, so the particles would be modelled instead, for example from published hand-clap research.

## Question
> New thread - wmd fracture emulation. Please research and let me know if you find enough information to make a similar vcv module.

## Answer
Yes, there is enough to build a module that behaves like Fracture. The one thing we cannot have is its sound set, so it would share the behaviour and range but not the exact timbre.

**What is known.** WMD's own manual page describes the engine plainly: a pseudorandom "particle oscillator" fires a 22-voice player of micro-samples, with the rate and the chance of each particle shaped over the burst, and sample, pitch, level and pan randomised per particle. All six knobs (Surface, Density, Decay, Freq, Spread, Reverb), all three positions of both switches (Tail and Punch), and the Trig, Tick, accent, Infinite and Pitch inputs are explained. It is a digital design, so a behavioural model is the same kind of thing as the original.

**What would be guessed.**
- The 23 "surfaces" are WMD's own recordings (claps, snaps, sticks, ping-pong balls, switches). They are unpublished and should not be copied.
- The ranges for density and decay, the amount of spread, the filter's sharpness and the reverb are not given in numbers, so they would be tuned by ear against demos.
- I found no teardown, firmware notes or designer interview, and no existing VCV version. I have not listened to the demos myself.

**What I would build.** One module of about 12 hp under its own name (working name "Clapfall") with the same six controls, the two switches labelled in words, and stereo out. For the particles I recommend modelling them instead of using samples. Hand claps are well measured in published research, so each particle can be a slightly different hand shape and each clapper can drift in time, which is what makes a crowd sound real. I would add 808 and 909 style particles so the classic claps come from the same engine, and a slot for loading your own folder of short samples.

The full note is in [01-feasibility.md](/mnt/project-files/fracture-vcv/01-feasibility.md). If you want to go ahead, tell me whether modelled particles and the name suit you and I will write the design.

## Sources
- Project file: `/mnt/project-files/fracture-vcv/01-feasibility.md`

Related: [[Destiny+ products that can be emulated in VCV Rack]], [[Make Noise NUSS modules and GTE documentation]]
