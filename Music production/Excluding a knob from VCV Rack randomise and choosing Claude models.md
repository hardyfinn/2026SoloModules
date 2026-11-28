---
title: Excluding a knob from VCV Rack randomise and choosing Claude models
date: 2026-11-22
source: Music production · project chat
topic: AI & Claude
tags: [vcv-rack, claude-models, randomise, tokens]
summary: |
 In VCV Rack 2 one line per knob, randomizeEnabled = false, keeps it out of Ctrl+R; Haiku is cheapest for that edit.
 Sonnet suits panel redesign; Haiku can find most output-level knobs but unclear ones should be checked.
---

# Excluding a knob from VCV Rack randomise and choosing Claude models

## Summary
In VCV Rack 2 one line per knob, randomizeEnabled = false, keeps it out of Ctrl+R; Haiku is cheapest for that edit.
Sonnet suits panel redesign; Haiku can find most output-level knobs but unclear ones should be checked.

## Question
> Whats the most token efficient model to you to exclude one parameter of our modules from vcv rack's ctrl+r randomisation across multiple built modules (ie level control)?

## Answer
Cheapest is Haiku 5.5. VCV Rack 2 has a per-knob flag, so excluding level from Ctrl+R is one added line per knob: `->randomizeEnabled = false;` after its configParam. One thread can search all plugins and edit only the level knobs. The main cost is rebuilding each plugin, so I'd combine it with the MIT licence tidy-up to rebuild once. Which parameters should be excluded (just level?), and for all modules?

## Question 2
> And is sonnet a good choice for panel redesign?

## Answer 2
Yes, Sonnet is a good fit for panel redesign. It's mostly layout and label sizing, which it handles well, and it costs much less than Opus. It also checks its own work by rendering panel previews. I'd keep Opus for new DSP or tricky design where the sound model matters, and Haiku for mechanical edits like the Ctrl+R change.

## Question 3
> Can haiku decide which knob relates to overall output volume, even if it's not called level?

## Answer 3
Mostly yes. Haiku can follow a knob through the code to see whether it ends up scaling the final output, and that finds most master-volume controls even when they're called Gain, Vol or Out. Where a module has several outputs or per-voice levels, it's less certain which one counts as overall. So I'd have Haiku produce a list of the knob it picked for each module and ask you about the unclear ones. Then it makes the edits only after you've glanced over the list.

## Sources
- The conversation itself; no outside sources were cited.
