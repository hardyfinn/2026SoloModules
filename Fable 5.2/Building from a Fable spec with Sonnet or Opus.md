---
title: "Building from a Fable spec with Sonnet or Opus"
date: 2026-11-14
source: "Fable 5.2 · Build Serge modules for VCV Rack"
topic: AI & Claude
tags: [claude-models, sonnet, opus, build-specs, workflow]
summary: |
 Sonnet could build from a detailed Fable spec, but hard DSP, circuit judgement and hand-drawn panels are where it would need more fix-up rounds, so Opus was recommended.
 No new thread is needed to build with Opus: the same thread can switch models and keeps the research context.
---

# Building from a Fable spec with Sonnet or Opus

## Summary
Sonnet could build from a detailed Fable spec, but hard DSP, circuit judgement and hand-drawn panels are where it would need more fix-up rounds, so Opus was recommended.
No new thread is needed to build with Opus: the same thread can switch models and keeps the research context.

## Question
> Do you think Sonnet could write from this design spec?

## Answer
Yes, Sonnet could build from it, but I'd still give this one to Opus.

The spec is [02-build-instructions.md](/mnt/project-files/serge-vcv/02-build-instructions.md). It is written so the builder rarely has to invent anything: each of the 19 modules has an exact control list, a behavioural model with formulas and constants, and tests it must pass. That suits Sonnet well for the simple modules, which are about half the set.

Where I'd expect Sonnet to need more fix-up rounds:
- **The hard DSP.** The nonlinear filters (VCFQ, VCFS, Resonant EQ), the band-limited NTO and the oversampling are easy to get subtly wrong in ways that pass a compile but sound off.
- **Circuit judgement.** The spec ends with a ledger of constants I estimated, and asks the builder to check them against schematics where it can. That is the step that decides how faithful the result is.
- **Paperface panels.** 19 hand-drawn SVG panels with symbols instead of words take visual judgement.

If you want to use Sonnet, the spec's build order already gates each stage on its tests, so it would work. A sensible split is Sonnet for the plugin and Opus for a review pass on the five DSP-heavy modules.

## Question 2
> I'd like to build this using Opus. Do i need to open a new thread for that?

## Answer 2
No, you don't need a new thread. I can switch this thread to Opus and it will build from the spec in the Serge repo.

Both routes work, so here is the difference:
- **Switch this thread.** Opus keeps everything from the research here, including the sources behind the estimated constants. This is what I'd do.
- **New thread.** A clean start that reads only the spec. Ask for it in the project chat and say Opus. The Serge repo is already saved to the project, so a new thread will have it.

If you want to stay here, reply "switch to Opus and build" and I'll do that. The build will go to the `claude/project-thread-rmwlim` branch unless you'd like it on `main`.

## Sources
- Project file: `/mnt/project-files/serge-vcv/02-build-instructions.md`

Related: [[Reducing Fable usage in a Claude project]], [[Claude models for building VCV Rack plugins]]
