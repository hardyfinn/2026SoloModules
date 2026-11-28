---
title: Max MC multichannel objects
date: 2026-11-06
source: Music production · Max MC objects
topic: Music
tags: [max-msp, multichannel, cycling74, programming]
summary: |
 What the mc. objects in Cycling 74 Max are and how multichannel patch cords work.
 Covers the main objects and when to use them.
---

# Max MC multichannel objects

## Summary
What the mc. objects in Cycling 74 Max are and how multichannel patch cords work.
Covers the main objects and when to use them.

## Question
> Tell me about the mc object in cycling74 Max

## Answer
"MC" isn't one object. It's Max's multichannel system (Max 8 and later). One patch cord can carry many audio channels at once, so you can patch one voice and get 8, 32 or more.

**The core ideas**
- **Put `mc.` in front of almost any audio object.** `mc.cycle~`, `mc.saw~`, `mc.lores~` and `mc.*~` each run a separate copy of the object for every channel. Cables carrying several channels show up thick and multicoloured.
- **`@chans 8`** sets the number of voices at the start of a chain. Every object after it picks up that count by itself.
- **Making each voice different:** send messages to an MC object. `spread 0.05 0.23` spaces the values evenly across the channels, and `deviate` scatters them randomly. A plain number goes to every channel.
- **Splitting and joining:** `mc.unpack~` splits a multichannel cable back into ordinary single cables, and `mc.pack~` combines single cables into one. `mc.mixdown~ 2` folds everything down to stereo, and `mc.dac~` plays it.
- **Going further:** `mc.gen~` runs your own gen~ patch once per channel. `mc.voices` hands out notes polyphonically, which is lighter than `poly~`.

**The patch I made you**, MC-Drift-Ensemble.maxpat: eight detuned saw voices, each drifting on its own slow LFO (a bit like your Drift Ensemble module). It's 8 voices from about 8 objects. Click the speaker icon and raise volume to about 0.3. The "spread" and "deviate" buttons re-roll each voice's drift speed. A second section sends four of the drift LFOs out to the disting NT as CV, and its level starts at 0. I wrote the file without Max on hand, so tell me if an object doesn't load.

**Variations:** turn `@chans` up to 32 for a dense swarm. Swap `mc.saw~` for `mc.noise~` into `mc.reson~` for tuned noise clouds. You can also drop the patch into a Max for Live audio effect and use it as a chorus or ensemble on a Hapax chord.

## Sources
- The conversation itself; no outside sources were cited.
