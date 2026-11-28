---
title: Using one project instead of several
date: 2026-11-28
source: Misc - non work non music · project chat
topic: AI & Claude
tags: [projects, threads, workflow, organisation]
summary: |
  For one person doing related work, one project with many threads is simpler: one shared folder, one memory and one index, with threads keeping jobs separate.
  Keep shared instructions and memory short and sectioned by area (VCV, Music, General); separate projects only make sense for a different default model or strict separation.
---

# Using one project instead of several

## Summary
For one person doing related work, one project with many threads is simpler: one shared folder, one memory and one index, with threads keeping jobs separate.
Keep shared instructions and memory short and sectioned by area (VCV, Music, General); separate projects only make sense for a different default model or strict separation.

## Question
> I think I might be using projects wrong. I've got 3 projects, one of which I was using for my Fable designs, one of which was making PCB rack modules and music modules, and then a kind of general thread. I'm now experiencing problems where when I want to document all of these, I have to work across 3 projects. For what I'm doing, would it be wise to just have one new project and do everything from there as threads?

## Answer
For a single person working on related things, yes, one project is the simpler setup. You're the only user, the work overlaps (modules, guides, notes), and splitting it means your shared folder, memory and docs get split too.

**What one project gives you**
- One shared folder, one memory and one index for guides and the knowledge base.
- Threads still keep each job separate, and I can pick the right model per thread.
- The repositories can all sit in one place (this project already lists many).

**Trade-offs**
- Project instructions and memory are shared, so keep them short and organised by area (VCV, music, general). Your current VCV rules would sit alongside everything else.
- The chat gets busier, so I'd group updates by area.
- A separate project still makes sense only if you want a different default model or strict separation, for example the token-heavy Fable design work.

**Suggested move:** make one new project (or reuse this one), copy the VCV workflow notes into its memory, and keep the Drive knowledge base as the shared record. Old projects can stay as an archive.

I can't move threads or files between projects myself. I can write a short hand-over note (instructions plus memory) for you to paste into the new project. Want that?

## Question 2
> Please write the handover note.

## Answer 2
Here's the hand-over note to paste into the new project's instructions or first message. I only know this project's setup, so add anything from your other two.

---
**Project instructions**
Hardy works alone across music/synth modules, guides and general questions. Organise by area: VCV, Music, General.

**VCV Rack modules**
- Fable threads produce only the module design spec (ports, params, DSP/sequencing logic, edge cases), saved to `vcv-modules/<module-name>.md`. Fable never plans or writes VCV boilerplate (plugin.json, Makefile, plugin.cpp/hpp, model registration, panel SVG, standard widget/param code).
- Build threads start fresh, pointed at the spec, and use `vcv-modules/_template` as the base. Prefer Opus/Sonnet for builds, brainstorming and small fixes. Keep Fable for hard design steps only, as Hardy is conserving Fable tokens.
- Keep threads small and focused.
- Modules live in repos such as 2026SoloModules.

**Guides**
- Single self-contained HTML file (not PDF), saved to `vcv-modules/<module-name>-guide.html` and published as an artifact, with one shared index page linking all guides. Hardy shares or publishes manually.

**Knowledge base**
- Google Drive folder "Knowledge Base": https://drive.google.com/drive/folders/1XFQ1lprQQ0_OgQhkYse4gK9fqEfd4jp4 (Markdown notes in `notes/`, README with template).
- Index page (artifact): https://claude.ai/artifact/HMxNw4sC5AUJmQCBMBQicA
- Good Q&As get saved as one full-text note each, then the index is refreshed.

**In flight**
- Footwork drum sequencer (browser artifact: https://claude.ai/artifact/Ji2G7hike65w2D4CmE5atH) is being extended with a pattern generator, then becomes a VCV module in 2026SoloModules (working name "Juke", modelled on Riddim).
---

Things to copy over by hand: files in `vcv-modules/`, `guides/` and the other project folders, plus the memory from your other two projects.

## Question 3
> What does organize by area mean, and what do I have to do?

## Answer 3
It just means the instructions and memory in the new project are written in sections (VCV, Music, General) instead of one long list, so Claude finds the right rules fast. The note I wrote is already laid out that way.

**You don't have to do anything for it.** The only steps are:
1. Create the new project.
2. Paste the hand-over note into its instructions.
3. Copy over your files and any memory from the other two projects when you want them.

After that, you just start threads as normal. If you like, name them with a prefix such as "VCV:" or "Music:" so they're easy to scan.

## Sources
- <https://drive.google.com/drive/folders/1XFQ1lprQQ0_OgQhkYse4gK9fqEfd4jp4>
- <https://claude.ai/artifact/HMxNw4sC5AUJmQCBMBQicA>
- <https://claude.ai/artifact/Ji2G7hike65w2D4CmE5atH>

Related: [[Setting up a Q&A knowledge base]] · [[Minimising token usage]]
