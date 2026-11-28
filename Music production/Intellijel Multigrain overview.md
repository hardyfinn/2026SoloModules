---
title: Intellijel Multigrain overview
date: 2026-11-02
source: Music production · Intellijel Multigrain
topic: Music
tags: [eurorack, granular, multigrain, sampling]
summary: |
 The Multigrain holds 8 Sounds per Preset, each with two Scenes that the MORPH fader blends.
 Covers the grain controls and how to clock and modulate it.
---

# Intellijel Multigrain overview

## Summary
The Multigrain holds 8 Sounds per Preset, each with two Scenes that the MORPH fader blends.
Covers the grain controls and how to clock and modulate it.

## Question
> new task - intellijel multigrain

## Answer
Here's the Multigrain at a glance, then a quick question about what you want me to build.

**How it's organised:** a Preset holds 8 Sounds (buttons 1–8). Each Sound is a sample, or a live input buffer, with two Scenes (A and B). The MORPH fader and MORPH CV blend between the two Scenes, so one fader move can take a Sound from a tight percussive stutter to a slow smeared pad.

**The grain controls:** START, WRAP and SCAN pick where grains read from and how that window moves. RATE, SIZE and PITCH set how often grains fire, how long they are and their pitch, and two link buttons tie RATE to SIZE and SIZE to PITCH. SHAPE (7 envelopes), TONE (low-pass/high-pass filter) and BLUR (reverb send) do the rest. X, Y and Z are three assignable CV inputs, and RAND adds internal random modulation, all stored per Scene.

**Where it fits your rig:**
• **Clock:** send a DROID or Pam's clock to SYNC. RATE then becomes a clock divider or multiplier. Patch a bar-length pulse into GATE to reset the phase.
• **Marbles X outputs** into X/Y/Z on START and PITCH give Autechre-style drift. A slow Marbles or DROID voltage into MORPH turns a loop into a passage that develops over time, which helps with the loop fatigue you mentioned.
• **SELECT** (0–5V picks Sound 1–8) from a K-Accumulator or Metropolix gives you sequenced sound switching.
• **Live mode** on Ableton or NT output means you can grain your own Hapax chord stabs live.

[…]

## Sources
- [Intellijel product page](https://intellijel.com/shop/eurorack/multigrain/)
- [Perfect Circuit overview](https://www.perfectcircuit.com/signal/intellijel-multigrain-overview)
- [Manual v1.2](https://www.analoguehaven.com/intellijel-designs/multigrain/manual.pdf)

Related: [[Make Noise Morphagene overview]]
