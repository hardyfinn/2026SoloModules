---
title: Modelling a steel pan and a hang drum
date: 2026-11-20
source: Music production · project chat
topic: Music
tags: [physical-modelling, steel-pan, hang-drum, synthesis]
summary: |
 How well a steel pan and a hang drum can be modelled in synthesis.
 Both are tuned metal shells with deliberate overtones, so modal synthesis gets close.
---

# Modelling a steel pan and a hang drum

## Summary
How well a steel pan and a hang drum can be modelled in synthesis.
Both are tuned metal shells with deliberate overtones, so modal synthesis gets close.

## Question
> I'm interested in two other bell modules. Well, kind of bells. How well can you model a steel pan drum and how well a hang drum?

## Answer
Both can be modelled well. The hang is the easier one to make convincing.

**Hang drum: very good.** It has few notes, each tuned with a fundamental, an octave and a compound fifth, sitting on a shell that acts as a Helmholtz resonator. The "gu" port gives that deep, breathy bass. Modal synthesis captures this closely. With the touch modelled too (finger slap, palm muting, thumb rolls, and the ding versus the tone fields), it would be hard to tell from a recording on its own, though a little cleaner than the real thing.

**Steel pan: good, but harder.** Each note area is tuned to its own fundamental, octave and fifth, and the areas bleed into each other, so hitting one makes its neighbours sing faintly. That sympathetic shimmer, the bright stick attack and the slight pitch glide at onset are what make a pan sound like a pan. A modal model with coupling between notes gets close, especially for tenor and double seconds. The rubber-tipped stick and fast rolls need care. A real steelband's ensemble sound would be a step further.

**Shared ideas for both:**
- Gate and pitch inputs, so either one can sit after Peal or a sequencer.
- Strike position, force and mute controls.
- A tuning editor or scale presets, including Hang-style scales such as D Kurd and Integral, and a pan layout in fourths and fifths.
- A "neighbours" amount for the steel pan's sympathetic ringing.

[…]

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Hard physical modelling targets in synthesis]]
