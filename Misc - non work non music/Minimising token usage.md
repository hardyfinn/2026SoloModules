---
title: Minimising token usage
date: 2026-11-26
source: Misc - non work non music · project chat (19 and 26 Nov 2026)
topic: AI & Claude
tags: [claude, token-usage, threads, projects, efficiency]
summary: |
  Keep threads small and single-goal, since every turn re-reads the whole thread; avoid big pastes, use lighter models and lower effort for simple work. Project memory is small and doesn't need clearing.
  For one-off questions unrelated to a project, a fresh standalone chat is cheapest, because project chats carry the project's context.
---

# Minimising token usage

## Summary
Keep threads small and single-goal, because every turn re-reads the whole conversation so far; project memory adds little and doesn't need clearing.
A fresh standalone chat is the cheapest place for an unrelated one-off question.

## Question
> Whats the most efficient way to ask a one off question? I wonder if adding it to this project chat uses tokens for content here that is not needed.

## Answer
Short answer: yes, a new chat outside this project is the cheapest way to ask a one-off question. Anything you write here starts from this project's context (instructions, memory, and recent chat), so unrelated questions carry extra baggage. A fresh standalone chat starts clean and only uses tokens for what you ask. Asking here is still fine when the question builds on this project's files or past threads. Within a project, a new thread per topic keeps each one small, rather than reusing one long thread.

## Question 2 (26 Nov 2026)
> I'm looking to ensure I don't use too many tokens on my Fable use. Should I clear memory or make sure new threads are kept small? Please tell me more about minimizing token usage.

## Answer 2
Short answer: keep threads small and focused; clearing memory matters much less.

**Why threads matter most:** every message in a thread re-reads the whole conversation so far, so long threads cost more with each turn. Cost grows faster than length.

**What helps, most to least impact:**
1. **One goal per thread.** Start a new thread for a new topic instead of continuing an old one.
2. **Avoid big reads.** Pasting huge files, logs or PDFs, or asking for whole-repo searches, adds a lot to every later turn. Point to specific files or sections.
3. **Use a lighter model for simple work.** Fable is best for hard problems. Lookups, drafts and small edits run fine on Sonnet or Haiku at a fraction of the cost (see models/choosing-claude-models.md in the project files).
4. **Lower effort for easy tasks.** Higher effort means more thinking tokens. I can set it per thread if you ask.
5. **Be specific.** Clear asks with the needed context avoid back-and-forth rounds, which each re-read everything.
6. **Resolve finished threads** so you don't return to them and extend them accidentally.

**Project memory:** it's small and loaded into each session, so it adds only a little. Keep it short and prune stale lines, but there's no need to wipe it. Clearing it would just mean I forget your preferences.

If you like, I can set a default cheaper model for new threads in this project and you pick Fable only when a task needs it.

## Sources
- No links were given in the answer.

Related: [[Choosing between Claude models]], [[Claude models for building VCV Rack plugins]]
