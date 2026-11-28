---
title: Master clock for Live, modular and VCV
date: 2026-10-16
source: Music production · Music production interview
topic: Music
tags: [clock-sync, modular, ableton-live, midi]
summary: |
 Pick one master clock and keep a fixed sync setup rather than patching ad hoc.
 The answer covers which device should lead and how to distribute clock to the modular and MIDI gear.
---

# Master clock for Live, modular and VCV

## Summary
Pick one master clock and keep a fixed sync setup rather than patching ad hoc.
The answer covers which device should lead and how to distribute clock to the modular and MIDI gear.

## Question
> i do not have a fixed setup for syncing everything together. This is an area where maybe I should work with one fixed set up for a while. I have lots of midi gear, modular and daw and tend to plug in what i need ad hoc. Do you have any recommendations for master clock? The disting NT is used as an audio interface alongside patching algorhythms.

## Answer
Yes, I'd let Live be the master clock, and your disting NT setup makes that easy. Settling on one fixed rig for a while is a good idea too.

**My recommendation: Live as master**
- **Modular:** Live's CV Tools (CV Clock Out) sends clock and reset as audio through the NT's USB outputs, which can carry CV. That clock is sample-accurate and lines up with your recordings.
- **Hapax:** set it to follow Live's MIDI clock.
- **VCV Rack:** run it as a plugin inside Live, so it follows Live's tempo automatically.

**Alternative: Hapax as master.** It sends MIDI clock to Live and a CV clock to the modular. It's more hands-on and works without the computer, but Live following an outside clock can drift a little, so you'd need to tune Live's sync delay.

I can write this up later as a step-by-step routing sheet.

Next, sound design: once a patch is going, what do you usually reach for to shape the sounds themselves? For example oscillators and filters in the rig, disting NT algorithms, VCV, or Live devices and resampling?

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Sound design between Detroit techno and Autechre]]
