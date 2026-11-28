---
title: "Destiny+ products that can be emulated in VCV Rack"
date: 2026-11-15
source: "Fable 5.2 · Assess Destiny+ modules for VCV emulation"
topic: Music
tags: [destiny-plus, emulation, vcv-rack, feasibility]
summary: |
 Two Destiny+ products have enough documentation for inspired-by VCV versions: Programma 900 2.0 (full manual) and entanglement space (all 64 algorithms named with knob assignments).
 None can be circuit-accurate, because the instruments are digital and the maker (kirkis) has published no firmware, schematics or DSP.
---

# Destiny+ products that can be emulated in VCV Rack

## Summary
Two Destiny+ products have enough documentation for inspired-by VCV versions: Programma 900 2.0 (full manual) and entanglement space (all 64 algorithms named with knob assignments).
None can be circuit-accurate, because the instruments are digital and the maker (kirkis) has published no firmware, schematics or DSP.

## Question
> New thread - is there enough information on any of the Destiny+ product line to make any vcv rack emulations?

## Answer
Yes, for two products. Neither can be circuit-accurate, though. Destiny+ instruments are all digital, and the maker hasn't published any firmware, schematics or DSP equations, so anything we make would be "inspired by", built from the manuals and demo videos. (The maker is a designer called kirkis, not Mike Hodnick.)

- **Programma 900 2.0 (worth doing):** it has a full manual. That covers the six additive voices (sliders for odd and even harmonics, partial count, tilt and tuning), harmonic lock, morph banks, 32 CV buffers that record and loop, and parameter lists for all nine effects (reverbs, 16-pole phasers, a flanger, granular). It would work as a voice module, a CV buffer module and an FX module.
- **entanglement space (worth doing second):** the maker's site names all 64 algorithms with each one's knob assignments. I'd make a four-core effects module with a chosen set of effect types rather than all 64.
- **Double Q:** the product is really its camera hand tracking, which can't be emulated. Its 16-lane gesture looper could become a small extra if you want one.
- **model-Q-2, Czochralski cells, 16 psyche, arc\*C, easel cards, rosetta:** too little is documented. Anything we built would be mostly our own invention.

These are current products from a small maker, so we'd use our own names rather than theirs. The per-product details and sources are in the attached [01-feasibility.md](/mnt/project-files/destinyplus-vcv/01-feasibility.md). Tell me which ones you'd like and I'll write the module proposal.

## Sources
- Project file: `/mnt/project-files/destinyplus-vcv/01-feasibility.md`

Related: [[Make Noise NUSS modules and GTE documentation]], [[Is there enough information to model WMD Fracture]]
