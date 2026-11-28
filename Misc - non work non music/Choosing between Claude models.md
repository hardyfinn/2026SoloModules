---
title: Choosing between Claude models
date: 2026-11-04
source: Misc - non work non music · thread "Choosing between Claude models"
topic: AI & Claude
tags: [claude, model-choice, research, music-projects, token-usage]
summary: |
  Default to Opus 5.5; step up to Fable for the hardest creative or structural work (about 2.5x the usage), down to Sonnet 5.5 for fast iteration and web gathering (about half), and Haiku only for bulk chores.
  For interview research: Sonnet to gather, Opus to synthesise, and always check quotes against sources.
---

# Choosing between Claude models

## Summary
Default to Opus 5.5; step up to Fable for the hardest creative or structural work, down to Sonnet 5.5 for fast iteration and web gathering, and use Haiku only for bulk chores.
For interview research, gather with Sonnet and synthesise with Opus.

## Question
> Please explain why to choose  different models here, with a view to the quality of the outputs we have made in my music projects here (eg francois markovian) and researching the web for information in artist interviews.

## Answer
My recommendation is to make **Opus 5.5** your default for both kinds of work. Step up to **Fable 5.1** for the hardest creative or structural parts of a music piece. It's the most capable model, but it's slower and uses your allowance about 2.5× faster. Step down to **Sonnet 5.5** for quick iteration and for gathering interviews from the web, since it's fast and about half the cost of Opus. Then go back to Opus to make sense of what you found. **Haiku 5.5** suits only bulk chores, like summarising a stack of transcripts.

I couldn't see the François Markovian outputs because they live in a project this one can't read, so this advice is based on the kind of work rather than on those results. The full comparison, with speed and cost figures from Anthropic's current model reference, is in [choosing-claude-models.md](/mnt/project-files/models/choosing-claude-models.md).

### Full comparison (from project file models/choosing-claude-models.md)

#### Choosing between Claude models

**Short version:** Make **Opus 5.5** your default for both kinds of work. Switch up to **Fable 5.1** when a music piece needs the deepest thinking. Switch down to **Sonnet 5.5** for quick iteration and broad web searching. Use **Haiku 5.5** only for bulk, simple jobs like summarising a pile of transcripts.

A caveat first: your music projects (including the François Markovian work) live in other projects that this one can't read, so I haven't looked at those outputs. The advice below is based on the kind of work you described, not on judging what came out.

##### The four models at a glance

The figures are from Anthropic's current model reference. API prices are per million tokens, input / output. In the Claude app you pay a flat plan rather than per token, but the bigger models use up your usage allowance faster, roughly in proportion to these prices.

| Model | What it's for | Speed | API price (in / out) | Relative cost vs Opus |
|---|---|---|---|---|
| **Fable 5.1** | Anthropic's most capable model, for the hardest reasoning and long, multi-step work. It always thinks before answering. | Slowest. Hard requests can take several minutes. | $10 / $50 | about 2.5× |
| **Opus 5.5** | The current flagship and Anthropic's default. Strong depth and judgement at a lower price than the previous Opus. | Moderate | $4 / $20 | 1× |
| **Sonnet 5.5** | "Speed and capability for everyday work." Very capable, quicker and lighter. | Fast | $2 / $10 | about 0.5× |
| **Haiku 5.5** | The smallest and cheapest model, for simple high-volume jobs. | Fastest | $0.10 / $0.50 | about 0.025× |

All four can handle very long inputs (a 1M-token context, which is several novels' worth), so length alone isn't a reason to change model. The real differences are how deep the reasoning goes, how polished and well judged the output is, how long you wait, and how quickly you use up your limits.

##### Music projects (e.g. François Markovian)

Creative and structural music work rewards depth. It means keeping a concept consistent across a whole piece, reasoning about harmony, form or generative rules, and avoiding generic, flat output. That is where the larger models pull ahead.

- **Opus 5.5 as the everyday default.** It's the best balance of quality and usage, and is good enough for most writing, analysis, lyric or concept work and any code behind the project.
- **Fable 5.1 for the hard parts.** Use it to design the core idea or system of a piece, untangle something Opus keeps getting slightly wrong, or carry out a long multi-step build in one go. Expect slower replies and faster use of your allowance, so keep it for sessions where quality really matters.
- **Sonnet 5.5 for iterating.** Quick variations, small edits, reformatting and "try ten versions of this line" are fine on Sonnet, and it's about twice as cheap as Opus.
- **Avoid Haiku for the creative core.** It's noticeably more generic and is best for chores.

A practical pattern: do the big creative or structural thinking on Opus (or Fable), then polish and iterate on Sonnet.

##### Web research into artist interviews

This work splits into two stages, and each suits a different model.

1. **Finding and gathering** (searching, opening pages, pulling out relevant passages). Speed and breadth matter more than depth here, so **Sonnet 5.5** is the sweet spot. Anthropic's newest web search and fetch tools, which filter results more cleverly, are available on Opus 5.5 and Sonnet 5.5.
2. **Making sense of it** (comparing what an artist said across years, spotting contradictions, judging how reliable a source is, writing a careful synthesis). This is where **Opus 5.5** earns its cost, because it is better at nuance and less likely to blur or overstate what someone said.
3. **Bulk summarising.** If you already have lots of transcripts and just want each one summarised or tagged, **Haiku 5.5** does it very cheaply. Hand the results to Opus for the final write-up.

Whatever model you use, check direct quotes against the source link before relying on them. Interviews get misquoted and paraphrased online, and no model fully removes that risk.

##### Quick rules of thumb

- Unsure? Use **Opus 5.5**.
- The output feels flat, or the task is long and intricate? Try **Fable 5.1**.
- Lots of quick back-and-forth, or casting a wide net online? Use **Sonnet 5.5**.
- A large pile of simple, repetitive items? Use **Haiku 5.5**.
- Close to your usage limit? Step down one model rather than stopping.

## Sources
- Project file: models/choosing-claude-models.md

Related: [[Claude models for building VCV Rack plugins]], [[Minimising token usage]]
