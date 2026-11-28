---
title: "Autechre's Max system and post-2010 eras"
date: 2026-11-13
source: "Fable 5.2 · Design Autechre-style VCV Rack modules"
topic: Music
tags: [autechre, max-msp, generative-music, sequencing]
summary: |
 Autechre work from a self-built hub of Max patches (with gen and C code) that they describe as procedural, not random, and since about 2016 driven by curves rather than discrete events.
 Most of the music comes from long live jams edited afterwards; hearing FM and physical modelling in it is the fans' reading, not something the band has confirmed.
---

# Autechre's Max system and post-2010 eras

## Summary
Autechre work from a self-built hub of Max patches (with gen and C code) that they describe as procedural, not random, and since about 2016 driven by curves rather than discrete events.
Most of the music comes from long live jams edited afterwards; hearing FM and physical modelling in it is the fans' reading, not something the band has confirmed.

## Question
> Im interested in creating a system of modules in vcv rack which produce music like that of autechre since 2010, who utilize their own max patches. Research the tools they designed and suggest modules that you could design in response. Do not build these, but the end result is instructions for the opus model to build. Ask me questions.

## Answer
The first pass of research is done, and I have some questions for you before I design the modules. The notes are attached. Each point is marked as either something the band said or something I inferred.

**What the sources say:**
- Their Max setup is a "hub" of patches they built themselves, with gen and C code, and almost no hardware. Since about 2010 they have kept growing one "system" for the albums and the live shows.
- They say the work is "procedural", not random. Oversteps was "markov chain stuff... with conditionals forcing scales and harmonies". Booth also said "the line between sequencing and synthesis is pretty much gone".
- Since about 2016 the sequencing is "a lot of curves rather than discrete events". Each sequencer embodies a single idea, and they pass patches back and forth between them.
- Most of the music comes from long live jams with the system that they edit afterwards. Neither of them has published specs for the synthesis or reverbs. Hearing FM and physical modelling in it is the fans' reading, not something the band has confirmed.

**Questions:**
1. **Which era matters most?** Melodic Oversteps/Exai, rhythm-heavy elseq/NTS, or the slower, sound-first SIGN/PLUS and recent live sets? Name a few tracks if you can.
2. **How generative versus hands-on?** Should the rack play itself while you steer a few big controls, or do you want to perform it more directly with knobs and gates?
3. **How many modules?** A tight set of 4–6, or a larger family of 10 or more?
4. **Sound engines too, or just control?** Should I design custom voices (FM, struck/physical percussion, sample cutting, reverb), or only sequencing and timing modules that drive existing VCV voices?
5. **Can it use existing modules?** Should it lean on VCV Fundamental and other free plugins, or work on its own?
6. **Your experience:** How comfortable are you with VCV Rack and with C++? This sets how much detail the build instructions need, such as SDK setup and testing.
7. **Where should the plugin live?** A new GitHub repo, a local folder, or undecided for now?

Until you reply I'll draft with these defaults: all eras, semi-generative with macro controls, about 6–8 modules, custom voices included, works alone but plays well with Fundamental, and full SDK detail in the instructions.

## Question 2
> 2. Eras. Quaristice, el seq,  exai. I enjoy untilted a lot though it is earlier. Spl9 is excellent.

## Answer 2
Thanks, that shifts the focus toward drum machines and sample cutting. I've updated the proposal to match:

- **New module, MUTATE**, for the Untilted and Quaristice side. It's a step drum sequencer where each step can also change the sound, like Elektron's parameter locks, and each track can have its own length. Every few bars it rewrites its own pattern by rotating, ratcheting, thinning or copying between tracks. A drift knob lets the pattern wander and a home button brings it back. (Untilted being Machinedrum-heavy is a fan claim. The Quaristice hardware jams are documented.)
- **SPLICE moves into the core set** because of spl9. That track is built on chopped, treated samples.
- **elseq** keeps the bending clock (WARP) and the curve-driven rhythm (CONTOUR) at the centre.

The core set is now WARP, MUTATE, CONTOUR, STRIKE, FMX, SPLICE and STEER. The rest are extended or experimental modules.

To write the build instructions I still need to know: how generative or hands-on the system should be, how many modules you want, your VCV and C++ experience, and where the plugin will live.

## Sources
- None cited in the answer.

Related: [[Mark Fell's timing and synthesis methods]], [[SuperCollider synthesis projects of 2026]]
