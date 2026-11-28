---
title: "Making a synth module feel analogue"
date: 2026-11-23
source: "Fable 5.2 · Design Verbos Sawtooth Stack module"
topic: Music
tags: [analog-modelling, saturation, drift, synth-design]
summary: |
 The less precise, more alive feel of analogue hardware comes from several small stacked errors: tracking errors, pitch jitter, imperfect waveshapes, saturation, loose envelopes, coupling, warm-up and a noise floor.
 All of these can sit behind one AGE knob, which transfers to other synthesis modules as a small shared library each module draws from.
---

# Making a synth module feel analogue

## Summary
The less precise, more alive feel of analogue hardware comes from several small stacked errors: tracking errors, pitch jitter, imperfect waveshapes, saturation, loose envelopes, coupling, warm-up and a noise floor.
All of these can sit behind one AGE knob, which transfers to other synthesis modules as a small shared library each module draws from.

## Question
> The gallery sounds great. The real module feels a little less precise and better for it. Do you think the drift control will help? Im interested in making this sounds feel more alive. Would some saturation also help?  What other suggestions do you have for making this feel more analogue?

## Answer
Yes, drift helps, but on its own it only covers one of the things you're hearing. Slow pitch wander makes unison phase and breathe. The "less precise" feel of the hardware comes from several small errors stacked together, and I'd model these, roughly in order of how much they matter:

1. **Tracking errors, not just drift.** Each of the five oscillators follows V/OCT and SPREAD slightly differently, so the detune changes from note to note and the octave stack is never quite octaves. The sketches have a little of this. The hardware likely has more, and it is the main reason a real swarm never repeats.
2. **Pitch jitter.** A faint fast noise on each oscillator's pitch, separate from slow drift. It roughens the edges of the saw and stops the beating from sounding like a clean chorus.
3. **Imperfect waveshapes.** A triangle-core sine is a bent triangle, so it carries a few percent of odd harmonics and a small glitch at the turnaround. The saw has a slightly rounded reset. Each of the five shapers would be a little different. At the moment the sketches use five mathematically perfect copies.
4. **Saturation, in two places.** Yes, this helps. A soft asymmetric stage on each oscillator before the mix, then the VCA and output stage pushed by the sum of five. The second one matters most: when all five peaks line up in unison the mix compresses, so the beating changes tone as well as volume.
5. **Loose envelopes and VCA.** Slightly curved segments instead of ruler-straight ones, a little timing scatter between notes, and a trace of bleed through the closed VCA.
6. **Coupling.** Oscillators on one board share a power rail and briefly pull toward each other when they are almost in tune. A small amount gives the "locking and letting go" feel near unison.
7. **Warm-up and noise floor.** A very slow common pitch sag over minutes, and a low hiss. Subtle, but they stop silence from being digital silence.

I'd put all of this behind one **AGE** knob on the panel (from factory-fresh to tired), replacing the right-click drift option, with a separate **DRIVE** knob for the saturation since you'll want to play that one.

I can render the gallery again as a side by side, clean against aged at two settings, so you can choose the default by ear. Shall I do that before the spec?

## Question 2
> This "age" control is fascinating and something id like to apply to our other synthesis modules.

## Answer 2
It should transfer well. AGE is not really about sawtooth stacks: it is a set of small error sources (component mismatch, slow drift, jitter, imperfect shapes, saturation, loose envelopes, a noise floor) that any analog-modelled module has in some form. The sensible way to reuse it is a small shared library in the plugin, say `age.hpp`, that each module pulls the pieces it needs from, with one AGE knob per panel. Cymbolism, House Keys, Birdie and Cicada would each use a different subset.

I'd build it here first, as a concrete module, and factor it out once we know which parts earn their place. Then it is a short job per module rather than a redesign.

Re-rendering the comparison now: a couple of the first takes clipped, which would have flattered the heavier settings.

## Sources
- None cited in the answer.

Related: [[The attack of a plucked double bass]]
