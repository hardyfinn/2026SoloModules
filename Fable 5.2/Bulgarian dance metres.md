---
title: "Bulgarian dance metres"
date: 2026-11-28
source: "Fable 5.2 · Design Bulgarian harmonies module"
topic: Music
tags: [bulgarian-folk, asymmetric-metre, rhythm, sequencing]
summary: |
 Bulgarian dance music counts in uneven bars built from short (2) and long (3) beats, such as ruchenitsa 7/8 (2+2+3), daichovo 9/8 (2+2+2+3) and kopanitsa 11/16 (2+2+3+2+2).
 Players stretch the long beats a little, so a lilt control makes the timing feel played rather than quantised.
---

# Bulgarian dance metres

## Summary
Bulgarian dance music counts in uneven bars built from short (2) and long (3) beats, such as ruchenitsa 7/8 (2+2+3), daichovo 9/8 (2+2+2+3) and kopanitsa 11/16 (2+2+3+2+2).
Players stretch the long beats a little, so a lilt control makes the timing feel played rather than quantised.

## Question
> Throughout the thread, he recommended an accent sequencer. Please tell me more what that would do. Should it have a link to trigger the phrase here?

## Answer
Yes, it should have one. A phrase output that fires on the last note of each cycle would plug straight into Bulgaria's PHRASE input, and that's probably the most useful thing it would do. It would be a small companion clock for Bulgarian dance metres.

Bulgarian dance music counts in uneven bars, built from groups of short (2) and long (3) beats. A few examples:

- **Ruchenitsa**, 7/8: 2+2+3, so short, short, long.
- **Daichovo**, 9/8: 2+2+2+3.
- **Kopanitsa**, 11/16: 2+2+3+2+2, with the long beat in the middle.
- **Buchimish**, 15/16, and other longer patterns.

You'd feed it a fast clock, pick a metre or type your own grouping, and it would give you:

- **Beat triggers** on the start of each group, so your sequencer steps on the uneven pulse instead of every sixteenth.
- **Accent CV**, strongest on the downbeat and on the long beats, for velocity or filter.
- **Bar and phrase triggers.** The phrase trigger would come on the last beat of every 2, 4 or 8 bars, ready for PHRASE.
- **A "lilt" knob** that stretches the long beats a little, the way players do. Like LOOSE, it would make the timing feel played rather than quantised.

It would be a separate module of maybe 8 HP rather than part of Bulgaria, since you didn't want a clock inside it. If you want it, I'd suggest a new thread so Fable can design it. This thread is on Opus now, after the build. Should I ask for that thread?

## Sources
- None cited in the answer.

Related: [[Bulgarian village singing and the microtonal rub]]
