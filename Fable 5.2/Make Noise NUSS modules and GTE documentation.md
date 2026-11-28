---
title: "Make Noise NUSS modules and GTE documentation"
date: 2026-11-15
source: "Fable 5.2 · Design MultiMod and PoliMATHS for VCV"
topic: Music
tags: [make-noise, nuss, emulation, vcv-rack]
summary: |
 Five of the six NUSS modules are documented well enough for behavioural VCV versions; only the XOH mixer is not worth doing, and there are no schematics or published DSP.
 GTE (Gestural Time Extractor) is also documented enough: it turns CV movement into 8 one-at-a-time gates, though its manual gives few numbers.
---

# Make Noise NUSS modules and GTE documentation

## Summary
Five of the six NUSS modules are documented well enough for behavioural VCV versions; only the XOH mixer is not worth doing, and there are no schematics or published DSP.
GTE (Gestural Time Extractor) is also documented enough: it turns CV movement into 8 one-at-a-time gates, though its manual gives few numbers.

## Question
> New thread - what make noise NUSS modules are well documented enough to make vcv rack versions?

## Answer
Five of the six NUSS modules are documented well enough to make VCV versions. Only the little XOH mixer isn't worth doing, because Rack's own mixers already cover it.

NUSS is Make Noise's New Universal Skiff System from January 2026. Each module has an official PDF manual that goes through every control one by one. There are no schematics and no published DSP for any of them, so each version would copy how the module behaves, not its actual circuit. That's the same approach as the Tudor modules.

- **MultiMod:** easiest to get very close to the original. It's digital, the manual gives its voltage ranges, shapes and clock behaviour, and there's nothing like it in VCV.
- **PoliMATHS (8-channel MATHS):** easy to copy, since every control is listed. I'd have to estimate the envelope curves, using the original MATHS and videos.
- **QPAS (stereo quad-peak filter):** the manual explains how it works in detail, but its analog tone can only be approximated. It's still very distinctive.
- **QXG (4-channel stereo low pass gate):** the panel is simple, but the response curves aren't published, so I'd tune its tone by ear.
- **MultiWAVE (8 voices, dual wavetable):** fully documented but the biggest job. We'd have to supply our own wavetables, since Make Noise's can't be copied.
- **XOH and the USB MIDI inlet:** skip these. Rack's mixers and its MIDI-CV module do the same jobs.

There's no official Make Noise plugin for VCV. Clones of the older MATHS and plenty of low pass gates exist, but none of them have the NUSS 8-channel trick, where one trigger picks which of 8 channels fires. In Rack that maps neatly onto polyphonic cables.

I'd start with **MultiMod, PoliMATHS and QXG** as a first round, then do QPAS and MultiWAVE. On trademarks, Make Noise is still selling these, so we'd give them our own names, panels and wavetables and call them "inspired by".

Tell me which modules you want and I'll write the proposal. Details and sources are in [01-feasibility.md](/mnt/project-files/nuss-vcv/01-feasibility.md).

## Question 2
> is there sufficient documentation to model the make noise GTE module?

## Answer 2
Yes, there's enough to make a faithful VCV version of how GTE behaves. It's a simple module, so the gaps in its documentation matter less than for PoliMATHS.

GTE (Gestural Time Extractor) is an 8 HP analog NUSS companion module. It watches how a CV moves and turns that movement into gates:
- 8 channel gate outputs, with only one high at a time.
- An odd output and an even output.
- A pulse stream out that fires a short trigger every time the channel changes.
- **Space** sets how many volts it takes to sweep through all 8 channels, which controls how busy the gates are.
- An optional clock input makes channel changes happen only on clock edges.
- With the knobs at set positions, it turns a NUSS Channel Index signal into 8 separate gates.

The [official manual](https://www.makenoise-manuals.com/gte/gte-manual.pdf) is short and gives few numbers. It doesn't state the input voltage range, the Space range, exactly where each channel's band starts and ends, the gate level, the pulse width or what the LEDs do. Those would be sensible guesses (for example 10 V gates and 1 ms pulses), and they don't change how it plays. Nothing like it is in the VCV Library.

It would fit well as a third Eightfold module, called something like **EGX**. It speaks the same 0.5 V Channel Index as EFG and ETS. That means you could patch ETS's loudest-copy output into it and get 8 gates, or turn any LFO's motion into rhythms. Do you want me to add it to the spec before the Opus build?

## Sources
- Project file: `/mnt/project-files/nuss-vcv/01-feasibility.md`
- [official manual](https://www.makenoise-manuals.com/gte/gte-manual.pdf)

Related: [[Destiny+ products that can be emulated in VCV Rack]], [[Is there enough information to model WMD Fracture]]
