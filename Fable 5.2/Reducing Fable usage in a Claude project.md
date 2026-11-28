---
title: "Reducing Fable usage in a Claude project"
date: 2026-11-26
source: "Fable 5.2 · project chat"
topic: AI & Claude
tags: [claude-projects, token-usage, fable, opus, workflow]
summary: |
 Clearing the project chat saves little, because the heavy usage is in the threads (design, sketch rendering and long builds).
 What cuts Fable usage: keep Fable for design only, give sharper notes to avoid rework rounds, and start new threads instead of extending long ones.
---

# Reducing Fable usage in a Claude project

## Summary
Clearing the project chat saves little, because the heavy usage is in the threads (design, sketch rendering and long builds).
What cuts Fable usage: keep Fable for design only, give sharper notes to avoid rework rounds, and start new threads instead of extending long ones.

## Question
> Does this project chat use lots of tokens? Should I be clearing it? I'm keen to minimize Fable usage.

## Answer
This chat is the small part, so clearing it wouldn't save much. I can't see your usage figures, but the chat mostly does short reads and one-line posts, while the heavy use is in the threads: design work, rendering sketches, and long builds.

What would actually cut Fable usage:
- **Keep Fable for design only.** The build step already goes to Opus, and you can set the project's default model to Opus so new threads only use Fable when you ask for the design pass.
- **Fewer rework rounds.** Each new round of sketches is a full Fable design pass, so sharper notes on the first round save the most.
- **Start a new thread instead of extending a long one.** A very long thread re-reads its whole history on each turn, so follow-up work on an old module is cheaper in a new thread.

If you'd like, I can set the default model for new threads to Opus now.

## Sources
- None cited in the answer.

Related: [[Building from a Fable spec with Sonnet or Opus]], [[Minimising token usage]]
