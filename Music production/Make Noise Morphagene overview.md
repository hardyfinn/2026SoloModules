---
title: Make Noise Morphagene overview
date: 2026-11-02
source: Music production · Make Noise Morphagene
topic: Music
tags: [eurorack, granular, morphagene, tape]
summary: |
 The Morphagene records into reels, cuts them into splices and plays them back as tape or grains.
 Covers every main control and patch ideas with clock, logic and random sources.
---

# Make Noise Morphagene overview

## Summary
The Morphagene records into reels, cuts them into splices and plays them back as tape or grains.
Covers every main control and patch ideas with clock, logic and random sources.

## Question
> new thread - make noise morphagene

## Answer
Here's a quick overview of the Morphagene to start with.

**What it is:** a tape and microsound module. It records into a "reel" (a long buffer on SD card, up to about 2.5 hours), which you cut into "splices" (up to 300 per reel), then play back and granulate.

**Main controls:**
- **Organize** picks which splice plays. CV here jumps between sounds, which makes it great for building a sound palette.
- **Gene Size** shrinks playback from the whole splice down to tiny grains.
- **Slide** moves the grain's position through the splice. A slow ramp gives you a time-stretch scan.
- **Vari-Speed** sets speed and direction. Noon is stopped, right plays forward, left plays in reverse.
- **Morph** controls how many genes overlap. Turned up past a point it adds random pitch and pan, which gives you clouds.
- **SOS** crossfades between the live input and the reel. It works as a sound-on-sound looper with gradual decay.
- **Clock in** syncs the genes to tempo. **EOSG out** fires a trigger at the end of each gene or splice. **CV out** is an envelope follower.

**How it could fit your rig:**
- Send Live's clock (via the NT) into Clock so the grains lock to your techno tempo.
- Patch EOSG into DROID to drive logic, for example advancing a pattern or flipping Marbles' mode every N genes. That gives you the self-mutating, Autechre-style feedback you've been after.
- Use stepped Marbles or Random8 voltage on Organize, and a slow K-Accumulator or Pam's ramp on Slide.
- Record a chord pad from the Hapax and splice it into four sections. Then arrange by changing Organize per section, which helps with loop fatigue.

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Intellijel Multigrain overview]]
