---
title: Building the MTRX-1K Matrix-1000 emulator for Mac
date: 2026-11-28
source: Music production · MTRX-1K Mac build
topic: Tech & Work
tags: [juce, plugins, mac, matrix-1000]
summary: |
 MTRX-1K is a JUCE, GPL v3 Matrix-1000 emulator whose code already supports Mac AU, VST3 and standalone builds.
 You must supply your own Matrix-1000 system EPROM (v1.20, 32 KB) or it stays silent.
---

# Building the MTRX-1K Matrix-1000 emulator for Mac

## Summary
MTRX-1K is a JUCE, GPL v3 Matrix-1000 emulator whose code already supports Mac AU, VST3 and standalone builds.
You must supply your own Matrix-1000 system EPROM (v1.20, 32 KB) or it stays silent.

## Question
> https://github.com/gprmusicproject/mtrx-1k.  This GitHub repo holds an interesting plugin that I'd like to build for Mac. However, I have no idea if it is suitable for building for Mac or how to do it. What's the easiest way to do this?

## Answer
Short answer: yes, it looks suitable, and you won't need to build anything yourself. I'm building it for you now.

What it is: MTRX-1K is an Oberheim Matrix-1000 emulator (it runs the real firmware) made with JUCE, the same toolkit most commercial plugins use. It's GPL v3, so building it for yourself is fine. The author only ships and tests a Windows version, but the code already has Mac support built in: on a Mac it makes an **AU, a VST3 and a standalone app**, and it includes the Apple Silicon version of its emulation engine.

I've set up a Mac build on GitHub's Mac machines (the same way your VCV plugins get built). It's a big codebase, so the first build may take 30 to 60 minutes. You'll get a zip with the plugins and an "Install MTRX-1K" file you double-click.

**One thing only you can do:** the firmware isn't included and can't be shared. You need your own copy of the Matrix-1000 system EPROM (version 1.20, 32 KB `.bin`), plus optionally the 64 KB patch EPROM for the factory banks. Without it the plugin loads but stays silent. On the Mac these files go in `~/Library/Application Support/GPR Music Project/MTRX-1K/ROM`, and the installer opens that folder for you.

## Sources
- [MTRX-1K on GitHub](https://github.com/gprmusicproject/mtrx-1k)
