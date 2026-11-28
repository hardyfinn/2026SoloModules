---
title: 4ms Rotating Clock Divider uses
date: 2026-10-22
source: Music production · Doepfer and 4ms module questions
topic: Music
tags: [eurorack, clock-division, 4ms, modules]
summary: |
 Common and less common uses for the 4ms Rotating Clock Divider.
 Covers both plain clock division and creative rotating and CV tricks.
---

# 4ms Rotating Clock Divider uses

## Summary
Common and less common uses for the 4ms Rotating Clock Divider.
Covers both plain clock division and creative rotating and CV tricks.

## Question
> Can you explain common and less common uses for the 4ms rotating clock divider?

## Answer
The 4ms Rotating Clock Divider (RCD) takes one clock and gives you 8 outputs, each dividing it by a different amount (by default ÷1 to ÷8). The trick that gives it its name is the **Rotate** CV input, which shifts which division appears on which jack. It also has a **Reset** input, plus DIP switches on the back for gate or trigger outputs, counting up or down, and longer division ranges (up to ÷16, ÷32 and beyond). It only divides; it never multiplies.

**Common uses**
• **Slower clocks for other modules:** clock Metropolix from ÷2, step Marbles from ÷4, and fire a long envelope from ÷8.
• **Drum layering:** ÷1 hats, ÷2 kick, ÷4 snare, then ÷3, ÷5 and ÷7 for hits that drift against the bar (polymeter).
• **Phrase markers:** set a long division (÷16 or ÷32) and use it to trigger fills, open a filter, or reset a sequencer every few bars.

**Less common uses**
• **Mutating rhythms (the Autechre angle):** patch a slow stepped CV into Rotate, such as Marbles X, a Metropolix CV lane, or a K-Accumulator output. A hat on output 3 might play ÷3, then ÷6, then ÷2, so the groove keeps reshuffling while staying locked to the clock.
• **Clock it from randomness:** feed Marbles t or Random8 triggers into it instead of a steady clock, and you get thinned-out, related random rhythms.
• **Subharmonic oscillator:** feed it an oscillator square wave at audio rate (Bohm, or a Three Sisters or Mangrove output squared up). The outputs become sub-octaves and odd subharmonics, which sound great as Detroit bass under a chord.
• **Stepped CV source:** in gate mode, mix several outputs at different levels through Cold Mac or any mixer. You get a repeating staircase CV for pitch or filter, which is basically a free mini-sequencer.
• **Logic:** route two outputs into DROID and combine them with AND or XOR (e.g. ÷3 XOR ÷4) for odd patterns that still repeat.

Honestly, DROID and Pam's can already divide clocks. The thing the RCD adds is that one-knob, CV-controlled rotation, which is fun to perform with live.

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Doepfer A-185-2 Precision CV Adder]]
