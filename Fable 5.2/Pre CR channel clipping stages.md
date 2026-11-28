---
title: "Pre CR channel clipping stages"
date: 2026-11-24
source: "Fable 5.2 · Mackie Emulation"
topic: Music
tags: [pre-cr, clipping, gain-staging, eurorack]
summary: |
 The Pre CR's three stages all clip at about ±10.5 V at their own output, but GAIN, the EQ and LEVEL decide which stage reaches that voltage first.
 With LEVEL at 5 and the EQ flat the preamp and output stage clip almost together; the stages only separate when LEVEL or the EQ move away from the middle. The figures come from simulation.
---

# Pre CR channel clipping stages

## Summary
The Pre CR's three stages all clip at about ±10.5 V at their own output, but GAIN, the EQ and LEVEL decide which stage reaches that voltage first.
With LEVEL at 5 and the EQ flat the preamp and output stage clip almost together; the stages only separate when LEVEL or the EQ move away from the middle. The figures come from simulation.

## Question
> You say the circuit clips hard in three stages, one after another. Can I ask, are these all triggered at the same threshold, so all kick in immediately?

## Answer
No. Each stage clips at the same voltage, about ±10.5 V at its own output, but the knobs decide which stage reaches that voltage first.

- **GAIN** sets how hard the preamp is driven. It is the only stage GAIN affects directly.
- **HI and LOW** sit after the preamp. Flat, the EQ stage passes the signal at the same size, so it only clips if you boost. A full LOW boost makes the EQ stage clip about 15 dB before the preamp does, on bass content only.
- **LEVEL** sets the gain of the output stage. Below about 4.5 it turns the signal down, so the output stage never clips and you hear the preamp alone. At 5 it adds just under 1 dB, so the output stage clips a hair before the preamp and the two are nearly simultaneous. At 10 it adds 23 dB, so the output stage clips long before the preamp gets anywhere near.

So in the renders I'm making for you, with LEVEL at 5 and EQ flat, the clipping is essentially one event shared between the preamp and the output stage. The stages only separate when you move LEVEL or the EQ away from the middle, and that is where the more complex tones come from: a preamp square wave, reshaped by the EQ, then clipped again.

These figures are from the simulation, so the exact crossover point on the LEVEL knob is something your recordings will confirm.

## Sources
- None cited in the answer.

Related: [[Modelling a Mackie CR-1604 channel from recordings]]
