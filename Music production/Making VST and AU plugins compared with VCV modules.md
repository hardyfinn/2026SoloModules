---
title: Making VST and AU plugins compared with VCV modules
date: 2026-11-22
source: Music production · project chat
topic: Tech & Work
tags: [plugins, vst, vcv-rack, development]
summary: |
 Making VST3 or AU plugins is similar DSP work to VCV modules but with more build, UI and host testing work.
 Audio-in, audio-out modules are the easiest to port first.
---

# Making VST and AU plugins compared with VCV modules

## Summary
Making VST3 or AU plugins is similar DSP work to VCV modules but with more build, UI and host testing work.
Audio-in, audio-out modules are the easiest to port first.

## Question
> We have made lots of vcv rack modules and a couple of max for live devices. Is it a comparable process to make vst or audio unit plugins? What's different And is there a smaller or larger number of tokens required to do so?

## Answer
Yes, it's a similar process, with a few real differences. The DSP code is the same kind of C++ we already write for VCV, so the sound engines carry over almost directly. The use of JUCE (the standard plugin framework) is what changes.

**What's different**
- **Framework:** VCV uses the Rack SDK. VST3 and AU normally use JUCE, which handles the plugin formats, parameters and presets.
- **Interface:** In VCV we draw the panel as an SVG with knobs and jacks. In JUCE we build a window with its own controls, which is more work to make look good, and I can't see it unless you send screenshots.
- **I/O:** VCV has cables and CV. Plugins have audio buses, MIDI and host automation, so CV-style ideas such as modulation outputs need rethinking. Many of our modules (sequencers, Peal, Lilt) would become MIDI or automation tools, and some wouldn't translate well.
- **Formats:** VST3 and AU are built separately. AU is Mac-only and must be code-signed and notarized to load reliably on newer macOS. Ableton uses VST3 or AU, so that works for you.
- **Testing:** Plugins need a host test, e.g. in Live. The validation tool `auval` for AU is fussy, and so is signing.

**Tokens:** Expect somewhat more than a VCV module, roughly 1.3 to 2 times, mostly for the interface, parameter and state handling, and the build and signing setup. The first plugin is the most expensive because we set up the JUCE project and build pipeline once. After that, each new one is closer to a VCV module in cost. Max for Live devices are cheaper still for simple tools.

If you want to try one, the best candidates are the audio-in, audio-out ones, such as Heathrow, Morphader or Surprise.

[…]

## Sources
- The conversation itself; no outside sources were cited.

Related: [[Open source licences for sharing modules]]
