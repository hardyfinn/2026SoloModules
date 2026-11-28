---
title: "Mark Fell's timing and synthesis methods"
date: 2026-11-15
source: "Fable 5.2 · Design Mark Fell-style VCV modules"
topic: Music
tags: [mark-fell, rhythm, fm-synthesis, sequencing]
summary: |
 Mark Fell sets each step as a length in milliseconds with a repeat count instead of using a timeline or BPM, and runs loops of different lengths against each other.
 For sound he tweaks 4-operator FM presets, steps through a list of voices on each note and uses dry drum-machine kits; the research did not confirm Linn drums, only 808/909 and early-80s machines.
---

# Mark Fell's timing and synthesis methods

## Summary
Mark Fell sets each step as a length in milliseconds with a repeat count instead of using a timeline or BPM, and runs loops of different lengths against each other.
For sound he tweaks 4-operator FM presets, steps through a list of voices on each note and uses dry drum-machine kits; the research did not confirm Linn drums, only 808/909 and early-80s machines.

## Question
> new thread. Mark Fell in vcv rack

## Answer
I've written up the research and a proposal for 10 modules: [01-research-and-proposal.md](/mnt/project-files/fell-vcv/01-research-and-proposal.md).

Fell doesn't use a timeline or BPM. He sets each step as a length in milliseconds and gives it a repeat count, then uses one control to stretch the whole pattern. He also runs loops of different lengths against each other so they drift in and out of phase. For sound he takes 4-operator FM presets and tweaks them, steps through a list of voices on each new note, and uses dry drum-machine kits.

The modules fall into two groups:
- **Timing:** DURA (the duration sequencer), GROUP, PRIME, LANES, CYCLE and HOLD.
- **Sound:** FM4, CHORD, KIT and SPECTRA.

The research didn't confirm the Linn drums from your brief. My sources only mention 808/909 and early-80s drum machines.

I need your answers on five choices. The defaults are in bold:
1. **Plugin name:** **Microtime**, or a name of your own. Fell's name and album titles stay off the panels.
2. **How many modules:** **all 10**, or a core 6 first.
3. **FM4 sounds:** **built-in presets plus loading of 4-op .syx banks**, or presets only.
4. **KIT drums:** **synthesised only**, or also loading your own WAVs.
5. **Repo:** please create an empty **hardyfinn/Microtime** and add it to the project.

When you've decided, I'll write the build spec for Opus.

## Sources
- Project file: `/mnt/project-files/fell-vcv/01-research-and-proposal.md`

Related: [[Autechre's Max system and post-2010 eras]], [[David Tudor's Rainforest, Pulsers and Phonemes]]
