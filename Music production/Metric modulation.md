---
title: Metric modulation
date: 2026-11-23
source: Music production · Metric modulation
topic: Music
tags: [rhythm, tempo, metric-modulation, music-theory]
summary: |
 Metric modulation changes tempo by making a note value in the old tempo equal a different value in the new one.
 Includes worked ratios and ways to do it in a DAW or modular.
---

# Metric modulation

## Summary
Metric modulation changes tempo by making a note value in the old tempo equal a different value in the new one.
Includes worked ratios and ways to do it in a DAW or modular.

## Question
> New thread - metric modulation

## Answer
Metric modulation means a note value inside the current pulse becomes the new beat. For example, "dotted 8th = new quarter" takes 120 BPM to 160, and the groove carries over so the change feels inevitable rather than like a jump. It's a strong arrangement tool for loop fatigue, because the patch keeps its patterns but the floor moves under them. Here are three ways I could build it for your rig.

**1. VCV module, working name "Pivot" (my recommendation)**
- It takes Live's clock in and puts out a modulated clock that your sequencers follow.
- A row of up to 8 pivots, each shown as "old note = new note" (3:2, 4:3, 5:4, dotted, triplet and so on). A pivot fires on a trigger, after N bars, or by CV.
- Each change either snaps on the downbeat or glides over a set number of bars, which gives you an accelerando into the new tempo.
- A HOME button and a "land in phase" mode, so a chain of pivots finishes back on Live's grid and never drifts against the arrangement.
- Outputs: the new clock, reset, a pivot trigger and a ratio CV. Two clocks overlap during the change for a short polymetric blur, which is very Autechre.

**2. Max for Live device "Pivot"**
- It changes Live's own tempo by exact ratios at bar lines, using the same chain, glide and home controls.
- With Live as master clock through the disting NT, VCV and the modular follow automatically, so the whole set modulates together.

**3. DROID patch**
- It switches clock multiply and divide ratios on a button press or a CV pattern. This works on hardware only, with no glide, and takes about 20 lines of .ini that I'd generate for you.

Options 1 and 2 also work well together: the module handles changes inside a patch, and the device handles big section changes.

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Visual music theory lessons]]
