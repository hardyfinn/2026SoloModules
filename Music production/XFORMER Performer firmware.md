---
title: XFORMER Performer firmware
date: 2026-10-24
source: Music production · Find Performer firmware with Teletype features
topic: Music
tags: [eurorack, sequencer, firmware, performer]
summary: |
 The Performer firmware with Teletype, Flux and Metropolix-style features is X|FORMER by onoma2.
 Includes its feature list, how to flash it from the SD card and caveats.
---

# XFORMER Performer firmware

## Summary
The Performer firmware with Teletype, Flux and Metropolix-style features is X|FORMER by onoma2.
Includes its feature list, how to flash it from the SD card and caveats.

## Question
> Find the Performer sequencer firmware fork with Teletype features. (Question as recorded in the thread title.)

## Answer
The one you saw is almost certainly **X|FORMER** by onoma2: [github.com/onoma2/xformer](https://github.com/onoma2/xformer). It's a fork of the Phazerville Performer firmware and adds exactly that mix of features. The latest release is [0.8.0](https://github.com/onoma2/xformer/releases).

**Features that match what you remember**
- **Teletype:** the "T9Type" track runs a cut-down Teletype engine inside a track, with up to 8 at once. You edit scripts with a USB keyboard. There's no I2C and no Grid support.
- **Flux:** the "PhaseFlux" track is a 4×4 pulse/pitch grid modelled on IoLabs Flux.
- **Metropolix:** Note tracks get a Metropolix-style accumulator (wrap, pendulum, random, hold) and pulse count, which repeats a step for 1 to 8 clocks.
- **Other new tracks:** Tuesday-style Algo, Discrete Map-style Discrete, ER-101-style Indexed, a Stochastic track and a Bloom-style Fractal looper. There's also a scope view and more routing options. Given how much you like Autechre-style patterns that keep changing, the Algo, Stochastic and accumulator features are the ones to try first.

**How to flash it** (no coding needed, just a file and the SD card)
1. Back up the whole SD card. X|FORMER projects aren't compatible with stock or older-version projects, so keep your current files safe.
2. Download the release and copy its `UPDATE.DAT` to the root of the SD card.
3. On the Performer, press PAGE + SYSTEM, then F5 (Update) to restart into the bootloader, and confirm the update. Holding the encoder while powering on also gets you into the bootloader.

I checked the button combo in the stock Performer manual, but the bootloader screen itself comes from my memory of the original firmware. Follow whatever it shows if it looks different.

**Caveats:** the author says it's provided "as is", that most of the code was written with AI help, and that Launchpad support isn't fully tested. Bugs are discussed on their [Discord](https://discord.gg/z4TPePP8Y), and the manuals are on the [project site](https://onoma2.github.io/xformer/).

Two smaller forks also came up, but neither has Teletype or Flux features: [Vinx's](https://github.com/VinxScorza/performer) (generators and Launchpad work) and [aclangor's](https://github.com/aclangor/performer) (a basic Metropolix-like repeat mode).

[…]

## Sources
- [X|FORMER on GitHub](https://github.com/onoma2/xformer)
- [X|FORMER releases](https://github.com/onoma2/xformer/releases)
- [X|FORMER project site](https://onoma2.github.io/xformer/)
- [Vinx Performer fork](https://github.com/VinxScorza/performer)
- [aclangor Performer fork](https://github.com/aclangor/performer)
