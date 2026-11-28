---
title: Emulating monome norns in VCV Rack
date: 2026-11-16
source: Music production · norns in VCV Rack
topic: Music
tags: [vcv-rack, norns, monome, emulation]
summary: |
 A full norns emulation inside VCV Rack is possible in theory but months of fragile work.
 The softcut looper engine is the useful piece to port; existing projects are listed.
---

# Emulating monome norns in VCV Rack

## Summary
A full norns emulation inside VCV Rack is possible in theory but months of fragile work.
The softcut looper engine is the useful piece to port; existing projects are listed.

## Question
> new thread - viability of a module that emulates monome norns in vcv rack. is it possible?

## Answer
**Short answer:** in theory, yes. In practice, I'd advise against a full norns emulation. A smaller module built on norns' own looper code (softcut) is very doable and gets you most of the sound.

**Why a full emulation is hard.** A norns is a small Linux computer running three programs at once:
- **matron** runs the Lua scripts, the screen, the encoders and the keys.
- **SuperCollider** runs the synth "engines".
- **crone** handles audio, including softcut.

All three are written for Linux on a Raspberry Pi. To put them in a VCV module, we'd need to move them to the Mac and squeeze them inside Rack, SuperCollider included. That's months of fragile work, and lots of scripts would still break.

**What already exists:**
- [monome-rack](https://github.com/Dewb/monome-rack) (Dewb) covers the eurorack modules (Teletype, Ansible and others), not norns.
- [norns-desktop](https://github.com/schollz/norns-desktop) and [norns-dev](https://github.com/winder/norns-dev) run norns in Docker, but only on Linux, with no grid or MIDI.
- [seamstress](https://monome.org/docs/grid/studies/seamstress/seamstress-and-norns) runs norns-style Lua scripts on a Mac, but it makes no sound by itself.
- [softcut-lib](https://github.com/monome/softcut-lib) is norns' looper engine, published as a standalone library. That's the useful piece.

**Options:**
1. **Full emulation:** months of work, high risk, and only some scripts would run. Not recommended.
2. **"norns-lite" host:** runs scripts that only use softcut, with no SuperCollider engines. That's weeks of work, and still a coding-oriented tool.
3. **Softcut module:** a multi-voice tape-style looper with CV for rate, loop points, overdub and filter, plus a screen showing the buffer. It's about the size of our other module builds, it's patched visually, and it could live in Gridworks with mlr-style grid control. **This is my recommendation.**
4. **Real norns alongside Rack:** a norns, norns shield or DIY Fates, running audio through the disting NT, gives you every script as it really runs. This is the best fit if what you want is the scripts themselves.

If you want option 3, tell me and I'll start it in Gridworks.

## Sources
- [monome-rack](https://github.com/Dewb/monome-rack)
- [norns-desktop](https://github.com/schollz/norns-desktop)
- [norns-dev](https://github.com/winder/norns-dev)
- [softcut-lib](https://github.com/monome/softcut-lib)
