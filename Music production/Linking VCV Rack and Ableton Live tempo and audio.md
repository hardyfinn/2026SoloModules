---
title: Linking VCV Rack and Ableton Live tempo and audio
date: 2026-11-27
source: Music production · VCV Ableton Link module
topic: Music
tags: [vcv-rack, ableton-live, ableton-link, audio-routing]
summary: |
 One module could carry 16 stereo tracks each way, limited by VCV's 16-channel polyphonic cable.
 Ableton Link only carries tempo, so audio needs a companion plug-in or a virtual audio device like BlackHole.
---

# Linking VCV Rack and Ableton Live tempo and audio

## Summary
One module could carry 16 stereo tracks each way, limited by VCV's 16-channel polyphonic cable.
Ableton Link only carries tempo, so audio needs a companion plug-in or a virtual audio device like BlackHole.

## Question
> New thread, VCV Ableton link module. I want it to be able to link tempo and audio. How many tracks might this do?

## Answer
**Short answer: 16 stereo tracks from Live into Rack, and 16 stereo returns back to Live, from one module.** The limit is VCV's own polyphonic cable, which carries 16 channels. So one cable holds the left sides of all 16 tracks and a second cable holds the right sides. The computer could handle more, but 16 is the clean number for Rack.

**The catch: Ableton Link only sends tempo, not audio.** It carries tempo, beat position and start/stop. Nothing else. So the design has two parts:

1. **Tempo, using Link.** The module joins Live's Link session (you just switch on Link in Live's top bar). It gives you CLOCK, RESET, RUN, BAR, PHASE and BPM outputs, plus a NUDGE knob to line up the timing by ear. Tempo stays locked very tightly, well under a millisecond.
2. **Audio, using a small companion plug-in in Live.** You put "Tether Send" on any Live track and pick a slot from 1 to 16. That track then shows up on that channel in Rack, and the module's screen shows its level. "Tether Return" goes on a Live audio track to play a Rack output back. The audio passes directly between the two programs on your Mac. Expect about 5 to 10 ms of delay, and there's an ALIGN knob to compensate. If Live and Rack both use the disting NT as their audio interface, they share one clock and won't drift apart.

Mockup (24 HP, working name "Tether"): tether-mockup.png

**Two other options worth knowing about:**
- **Nothing to build, works tonight:** install the free BlackHole 16ch virtual audio device and combine it with the disting NT in macOS Audio MIDI Setup. Live sends tracks to BlackHole outputs 3 to 18, VCV's Audio 16 module picks them up, and an existing Link module handles tempo. It's 16 tracks and it works, but the setup is fiddlier and there's no track naming or metering.
- **If you own VCV Rack Pro:** run Rack as a plug-in inside Live. Live then handles tempo and audio directly.

Two things to know:
- The Link code from Ableton requires the GPL licence, so this plugin would be GPL rather than MIT.
- The send/return plug-ins mean building a VST3/AU as well as the VCV module. I recommend it, because that part is what makes it better than the BlackHole setup.

[…]

## Sources
- The conversation itself; no outside sources were cited.
