---
title: "Max for Live, VST or VCV for a beat juggling device"
date: 2026-11-23
source: "Fable 5.2 · Design beat juggling Max for Live device"
topic: Tech & Work
tags: [max-for-live, gen, vst, vcv-rack, audio-performance]
summary: |
 Max is fast enough for a beat juggling device: gen~ compiles to native code, and the only weak spot is the lower-priority planning script, which is handled by queueing moves ahead.
 A VST would be technically stronger (exact transport per block, any DAW), and VCV would perform as well but has no song position, so its gain is CV control rather than a technical fix.
---

# Max for Live, VST or VCV for a beat juggling device

## Summary
Max is fast enough for a beat juggling device: gen~ compiles to native code, and the only weak spot is the lower-priority planning script, which is handled by queueing moves ahead.
A VST would be technically stronger (exact transport per block, any DAW), and VCV would perform as well but has no song position, so its gain is CV control rather than a technical fix.

## Question
> I'm interested to know if there is any better technical suitability for this device as a VCV rack module or vst. Is max a performant platform for this?

## Answer
Short answer: Max is fast enough for Doubles, and staying in Max for Live is the right call if you mostly use it inside Live. A VST would be the stronger platform technically. VCV wouldn't be a technical upgrade, but it would be a different creative fit.

**Max for Live performance**
- The sound is made in gen~, which Max compiles into native code, much like a C++ plugin. Two decks reading a buffer is cheap.
- The weak spot is the script that plans the juggle. It runs at lower priority than the audio and ticks every 20 ms, so it can jitter. I designed around that: it queues moves about 1.5 beats ahead, and gen~ plays them at the exact sample. Timing only slips if Live's interface stalls for longer than that.
- Song position comes from Live's API, which is a little loose. That's why the device resyncs to the beat clock, so you won't hear it.

**VST3/AU (C++)**
- Everything runs inside the audio callback. The host passes the exact transport position with every block, so there's no scheduling gap and no API resync.
- It works in any DAW, and it can be tested properly in the cloud, like the VCV builds.
- The costs: a bigger build, Mac code signing, and less of Live's set integration.

**VCV Rack**
- It would perform as well as the VST, but VCV has no song position, so it would follow a clock input.
- What it gives you is control. Every move, fader and HANDS dial could take CV and gates from your sequencers. That's a performance idea rather than a fix.

My recommendation is to keep the Max version for Live. If you'd like a VCV or VST version as well, the engine and planner would carry over almost unchanged.

## Sources
- None cited in the answer.
