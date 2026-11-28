---
title: Orchestrator models and module families
date: 2026-11-20
source: Misc - non work non music · project chat
topic: AI & Claude
tags: [claude, orchestration, subagents, vcv-rack, workflow]
summary: |
  An orchestrator is a model that splits a big task, hands parts to worker models (subagents), checks results and combines them; it saves cost and keeps contexts small, but a vague brief misleads every worker.
  For a family of VCV modules in one plugin: one shared spec from Opus/Fable, one Sonnet worker per module, shared helpers first, then a consistency review; worth it from 3–4 modules up.
---

# Orchestrator models and module families

## Summary
An orchestrator is a model that splits a big task, hands the parts to worker models, checks their results and combines them.
For a family of VCV modules, a shared spec plus one worker per module works well from about three or four modules up.

## Question
> I've heard the term orchestrator when talking about models providing direction to other models. Can you tell me more about this?

## Answer
An orchestrator is a model (or program) that breaks a big task into parts, hands those parts to other models, and then combines their results. It's the manager in a team of AI workers.

- **What it does:** plans the work, picks which worker handles each step, passes along the context each one needs, checks the results, and decides what happens next.
- **Workers:** often called subagents. They can be smaller, cheaper models, or the same model with a narrow job like "search the docs" or "write this module". They can run in parallel and report back.
- **Why it's used:** a strong model plans while cheaper ones do the repetitive steps, which saves cost. Each worker also gets a small, focused context, so it makes fewer mistakes than one model juggling everything.
- **Trade-offs:** more moving parts, and the orchestrator's brief is a single point of failure. If the instructions are vague or the plan is wrong, the workers faithfully do the wrong thing. It isn't worth it for small tasks.

This is the same idea as your earlier question about Opus writing a plan for Sonnet to build VCV modules, which is a simple two-tier version. This project works that way too: I coordinate, and threads do the deep work.

## Question 2
> So would this be a good way to handle building a family of modules in the same plugin?

## Answer 2
Yes, a family of modules in one plugin is a good fit, because the modules share a lot. A workable setup:

- **Orchestrator (Opus or Fable):** writes one short shared spec covering the plugin's conventions: panel style and colours, port and param naming, voltage standards, shared DSP helpers, file layout and plugin.json. It also lists each module with its params, ports and acceptance checks, and designs the tricky DSP.
- **Workers (Sonnet, Haiku for panels):** each builds one module against that spec. Independent modules can run in parallel, so you'd start one thread per module.
- **Shared code first:** build the common helpers (e.g. a shared filter, envelope or UI widget) before the modules that use them, so every module doesn't reinvent them and drift apart.
- **Review step:** have the orchestrator check each module against the spec for consistency, and check anything audio-related by ear.

The main risk is inconsistency between modules, which a good shared spec mostly prevents. It's worth it from about three or four modules up. For one or two simple modules, Sonnet alone is quicker and cheaper.

## Sources
- No links were given in the answer.

Related: [[Claude models for building VCV Rack plugins]]
