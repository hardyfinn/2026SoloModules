---
title: "VCV Rack tags and module search"
date: 2026-11-14
source: "Fable 5.2 · Build Serge modules for VCV Rack"
topic: Tech & Work
tags: [vcv-rack, plugin-development, module-browser, tags]
summary: |
 VCV Rack only accepts tags from its own fixed list and drops any others, so a short name like NTO cannot be a tag.
 The workaround is to end each module's name with its short name, for example "New Timbral Oscillator (NTO)", because Rack's browser searches names.
---

# VCV Rack tags and module search

## Summary
VCV Rack only accepts tags from its own fixed list and drops any others, so a short name like NTO cannot be a tag.
The workaround is to end each module's name with its short name, for example "New Timbral Oscillator (NTO)", because Rack's browser searches names.

## Question
> For the next build, please add the common short serge names of the modules to the vcv rack tags, so that i can search for eg. NTO and get new timbral oscillator.

## Answer
The manual now has 12 more patch recipes (pages 7 and 8), including pinged percussion, a bouncing ball, two-slope chaos, an FM bell, a sync sweep, PWM from the comparator, a canon from the ASR and talking formants on RESEQ.

On the search request: Rack only allows tags from its own fixed list and drops any others, so "NTO" can't be a tag. Instead, each module's name now ends with its short name, such as "New Timbral Oscillator (NTO)". Rack's browser searches names, so typing NTO will find it. This is on main, and [the new build](https://github.com/hardyfinn/Serge/actions/runs/43638964236) is running now.

## Sources
- [the new build](https://github.com/hardyfinn/Serge/actions/runs/43638964236)

Related: [[Serge modules already in VCV Rack]], [[Installing a VCV Rack plugin file on a Mac]]
