---
title: "How vinyl cutting and playback shape the record sound"
date: 2026-11-21
source: "Fable 5.2 · Design 90s vinyl sound module"
topic: Music
tags: [vinyl, riaa, mastering, phono-preamp, lo-fi]
summary: |
 The vinyl sound comes from a chain of ten processes from cutting room to phono preamp, such as elliptic bass mono, the cutting de-esser, the RIAA curve, stylus tracing, cartridge resonance and the phono stage.
 The record and playback curves run as a matched pair, and the nonlinear stages in between act on boosted treble that is later pushed down, which is where the softness and warmth come from.
---

# How vinyl cutting and playback shape the record sound

## Summary
The vinyl sound comes from a chain of ten processes from cutting room to phono preamp, such as elliptic bass mono, the cutting de-esser, the RIAA curve, stylus tracing, cartridge resonance and the phono stage.
The record and playback curves run as a matched pair, and the nonlinear stages in between act on boosted treble that is later pushed down, which is where the softness and warmth come from.

## Question
> New thread, vinyl. I'm fascinated by the sound of vinyl, having spent all my money on records in the 80s and 90s. Please make a module that recreates the sounds of vinyl. I'm not talking about adding pops and crackles over the sound. I'm interested in how the cutting process changes the frequency response of the material. I'm interested in how the phono amp works. Please create ideas for 10 processes in one module that help me get that sound of 90s vinyl.

## Answer
The design for the vinyl module is ready: [01-research-and-proposal.md](/mnt/project-files/vinyl-vcv/01-research-and-proposal.md). It has ten processes that follow the real chain from cutting room to phono preamp, and none of them is crackle.

**Cutting**
1. **ELLIPTIC** folds the bass to mono below about 150 Hz, the way engineers did so the cutter wouldn't jump out of the groove.
2. **HF LIMIT** is the cutting room de-esser, which kept hats and sibilance from burning out the cutter head.
3. **CUT** applies the RIAA record curve and the colour of the cutter head. Its SIDE LENGTH control makes long sides quieter and smaller, like a real 25-minute LP side.

**Medium**
4. **MEDIUM** chooses lacquer, DMM or acetate dub plate, with pre-echo and a wear control for plates.

**Playback**
5. **TRACING** models how the stylus shape fails on short wavelengths, including inner groove distortion.
6. **TRACKING** covers tonearm angle error, skating, and breakup on hot peaks.
7. **CARTRIDGE** is a moving magnet coil and load capacitance resonance, plus channel crosstalk and azimuth.
8. **ARM** gives the 10 Hz arm resonance and warp wobble.
9. **PLATTER** gives you 33 or 45 rpm, off-centre wow, flutter, and a 1200-style pitch fader.
10. **PHONO** applies the playback curve through a tube, op-amp or 90s DJ mixer stage, with part tolerance and overload.

The key idea is that the record and playback curves always run as a matched pair. Everything nonlinear in between works on boosted treble that later gets pushed back down, and that is where the vinyl softness and warmth come from.

There are also two extra controls. **NEEDLE** sets where the stylus is on the side, and can sweep slowly so a patch gets duller towards the label. **FORMAT** is a preset for 7", 12" club single, LP or dub plate.

I need five choices from you, all listed at the end of the doc: the name (Lathe, Groove, Platter or Pressing), one wide module or a main module plus an expander, whether to keep the auto needle sweep, whether to keep all ten processes, and where it lives (a new repo or a folder in 2026SoloModules).

## Question 2
> Switch to Fable and think about other modules you would add here that recreate the vinyl sound, especially that of the 80s and 90s, including some pretty poor quality pressing.

## Answer 2
I've proposed five companion modules: [03-companion-modules.md](/mnt/project-files/vinyl-vcv/03-companion-modules.md). I need three choices from you before Opus builds anything.

- **PRESSING** is the poor-pressing module. It covers recycled vinyl, styrene singles, flexi discs and picture discs, patches that didn't fill, a tired stamper, thin 80s discs that ring, fluff building up on the stylus, scratches and cue burn. Defects are cut into the groove and read by the same stylus and phono stage as the music, so nothing is pasted on top.
- **DECK** holds the record in memory, so it can do what a live effect can't: a real pitch fader, 33 played at 45, start and brake, power-off spin-down, scratching, needle drops, skips and the stuck-record loop.
- **MASTER** is what the cutting engineer was handed: analogue tape, 1630 or DAT, a loud CD master, wrongly decoded noise reduction or digital emphasis, and the digital delay line inside 80s lathes.
- **BOOTH** is the room: speaker feedback into the stylus, footfall, motor rumble, and mains hum that rises toward the label.
- **MUSIC CENTRE** is the cheap 80s system, with a ceramic cartridge, an autochanger, and tone and LOUDNESS controls.

The details are from memory, and the doc marks the figures Opus should look up first.

**Your choices:**
1. Which modules to build, and in what order. I suggest PRESSING, then DECK, then MASTER.
2. Whether PRESSING keeps dust ticks and scratches. Your first brief ruled out pops and crackles. Here they'd be physical bumps the stylus reads, with their own knobs you can leave at zero.
3. Whether expanders are fine now. PRESSING, BOOTH and MUSIC CENTRE have to reach inside Vinylizer Fable, so they would attach to its side.

## Sources
- Project file: `/mnt/project-files/vinyl-vcv/01-research-and-proposal.md`
- Project file: `/mnt/project-files/vinyl-vcv/03-companion-modules.md`

Related: [[VCV Rack expanders versus built-in controls]]
