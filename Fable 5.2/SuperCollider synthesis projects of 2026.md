---
title: "SuperCollider synthesis projects of 2026"
date: 2026-11-15
source: "Fable 5.2 · project chat"
topic: Music
tags: [supercollider, fm-synthesis, chaos, vcv-rack, module-ideas]
summary: |
 Six 2026 SuperCollider projects from the scsynth forum suit VCV modules, led by FMMatrix (a free-routing FM/PM operator network) and Stochastica (chaos and randomness).
 Caution: FMMatrix and Stochastica are self-described as vibe coded with unverified maths, and several have no stated licence, so build from the techniques rather than port code.
---

# SuperCollider synthesis projects of 2026

## Summary
Six 2026 SuperCollider projects from the scsynth forum suit VCV modules, led by FMMatrix (a free-routing FM/PM operator network) and Stochastica (chaos and randomness).
Caution: FMMatrix and Stochastica are self-described as vibe coded with unverified maths, and several have no stated licence, so build from the techniques rather than port code.

## Question
> what are some interesting 2026 supercollider synthesis projects? With an eye to making more vcv modules

## Answer
Six 2026 SuperCollider projects stand out as module material, all from the scsynth forum this year.

- **[FMMatrix](https://scsynth.org/t/fmmatrix-a-new-set-of-frequency-and-phase-modulation-ugens/13686)** (October) is the strongest. It is a free-routing FM/PM operator network where every operator can modulate every other, with same-sample feedback, a fractional delay per connection, wavetable operators, external audio as a modulator, and fold/wrap/tanh shaping on each modulation sum. That would make a matrix FM voice well beyond Fallen's FM4.
- **[Stochastica](https://scsynth.org/t/stochastica-a-new-suite-of-ugens-for-chaos-and-randomness/13657)** (September) is a chaos and randomness suite: Rulkov and Izhikevich neuron models, Chua, Mackey-Glass, coupled map lattices, and Hawkes and semi-Markov event generators. It would suit a family of small CV and trigger sources, and fits the Autechre and Tudor direction.
- **[HearSay](https://scsynth.org/t/hearsay-fm7-fm6x-nome6-patch-generator-and-editor/13740)** (October) brings two new FM operators: Fmx6, with non-sine waveforms and a filter per operator, and Nome6, which swaps sines for Jacobi elliptic functions that morph smoothly from sine towards square.
- **[TR-808 cymbal model](https://scsynth.org/t/tr808-cymbal-hardware-accurate-modeling/13607)** (September) derives the six Schmitt oscillators and the filter stages from the service manual, covering cymbal and both hats. It is a natural drum companion to Silverbox.
- **[SignalBox](https://scsynth.org/t/signalbox-quark-waveshaping-methods/13365)** (June) adds quadrature and normalised waveshaping, which gives FM-like timbres where you specify the resulting spectrum directly.
- **[GrainDelay and GrainUtils](https://scsynth.org/t/graindelay-a-sub-sample-accurate-granular-feedback-delay/12177)** (late 2025, still active) is a 32-grain feedback delay with sub-sample accurate scheduling and freeze.

Two cautions. Stochastica and the same author's FMMatrix are self-described as vibe coded with unverified maths, and several have no stated licence, so we would build from the published techniques rather than port code. I only read the forum posts, not the repos.

My pick would be a matrix FM voice plus a chaos set. Say which ones appeal and I'll start a design.

## Sources
- [FMMatrix](https://scsynth.org/t/fmmatrix-a-new-set-of-frequency-and-phase-modulation-ugens/13686)
- [Stochastica](https://scsynth.org/t/stochastica-a-new-suite-of-ugens-for-chaos-and-randomness/13657)
- [HearSay](https://scsynth.org/t/hearsay-fm7-fm6x-nome6-patch-generator-and-editor/13740)
- [TR-808 cymbal model](https://scsynth.org/t/tr808-cymbal-hardware-accurate-modeling/13607)
- [SignalBox](https://scsynth.org/t/signalbox-quark-waveshaping-methods/13365)
- [GrainDelay and GrainUtils](https://scsynth.org/t/graindelay-a-sub-sample-accurate-granular-feedback-delay/12177)

Related: [[Autechre's Max system and post-2010 eras]], [[David Tudor's Rainforest, Pulsers and Phonemes]]
