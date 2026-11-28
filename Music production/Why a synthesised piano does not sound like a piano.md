---
title: Why a synthesised piano does not sound like a piano
date: 2026-11-26
source: Music production · Upright piano
topic: Music
tags: [piano, physical-modelling, samples, synthesis]
summary: |
 A piano is about the hardest instrument to synthesise; good samples beat a hand-tuned model.
 The advice is to play a sampled upright, either in the DAW or in a simple sample-player module.
---

# Why a synthesised piano does not sound like a piano

## Summary
A piano is about the hardest instrument to synthesise; good samples beat a hand-tuned model.
The advice is to play a sampled upright, either in the DAW or in a simple sample-player module.

## Question
> Unfortunately, Joanna actually sounds terrible. I don't really know whether to start again or to try and tweak the model. The sound just is nothing like a piano. I think part of the problem may have come from trying to model a piano playing in a room. Your advice?

## Answer
My advice is to start again, but small. Keep Joanna as it is and build a new, dry piano core with very few controls, then judge it by ear before adding anything.

I think the room is only part of the problem. Joanna tried to do everything at once (room, cabinet, wear, age, sympathetic strings, coupled unisons), so when it sounds wrong there's no way to tell which layer is to blame. The things that make a piano read as a piano are mostly in the first half second, and they're all in the core:

- **The hammer strike.** Soft blows are dark, hard ones bright and a bit "clanky". That change of colour with velocity matters more than almost anything else.
- **The soundboard.** The wooden "knock" and body under the attack is a big part of what your ear calls piano. Without it, strings sound like a synth or a harp.
- **Slightly stretched overtones** (inharmonicity), strongest in the bass, which give the low notes their metallic growl.
- **Two-stage decay.** A fast initial drop, then a long quiet tail, with 2 or 3 strings per note beating gently against each other.

The plan: build only those four things, with a simple optional reverb. Then I'll send you short WAV demos (single notes low to high, a chord, a short phrase) for a verdict before any extras. If it sounds like a piano, we can add the "old home upright" character (detuning, wear) back on top as one or two knobs.

One question: should this go in 2026SoloModules as a new module? I'll start the design there while I wait.

## Question 2
> I'm not really enjoying this. The whole sound is off. I think this may be a case where samples are a much better idea than trying to synthesize what is obviously a complicated model.

## Answer 2
I agree, and I'm sorry this one didn't land. A piano is about the hardest instrument there is to synthesise. Even the commercial physical models took teams years, and good samples beat anything I can tune without being able to hear it. I'll stop work on the synthesised version (it stays on its branch, unmerged, in case you ever want it back).

There are two good ways to get a sampled upright into your setup:

1. **Play a sampled piano in Live, from VCV.** Send poly V/OCT, gate and velocity out of Rack as MIDI into a Live track holding a piano. That could be Live's own piano, or a free upright from Pianobook or Spitfire LABS (LABS has a nice soft upright). You get the best sound today, with no building at all.
2. **A sample-player module inside VCV.** I'd build a simple multisample piano player (velocity layers, release, pedal, a little detune and tone) and bundle a free upright sample set under a licence that lets you share it. That way the plugin can be shared publicly. This keeps everything inside Rack, but it'll only be as good as the free samples.

I'd start with option 1, since it sounds best right now, and only build the module if you want the piano living inside Rack.

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Free upright piano sample sets]], [[Hard physical modelling targets in synthesis]]
