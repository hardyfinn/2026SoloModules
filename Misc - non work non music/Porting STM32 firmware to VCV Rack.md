---
title: Porting STM32 firmware to VCV Rack
date: 2026-11-17
source: Misc - non work non music · project chat
topic: AI & Claude
tags: [vcv-rack, stm32, firmware, porting, dsp]
summary: |
  Firmware for an STM32H750 can become a VCV plugin if the source code is available: separate the DSP core from the hardware layer and wrap it in a Rack module.
  A compiled .bin/.hex alone would need Cortex-M7 emulation, which is slow, fragile and rarely worth it.
---

# Porting STM32 firmware to VCV Rack

## Summary
STM32H750 firmware can become a VCV plugin if you have the source: split the DSP core from the hardware layer and wrap it in a Rack module.
A compiled binary alone would need Cortex-M7 emulation and is rarely worth it.

## Question
> If I supply a firmware for stm32h750, can you create a vcv plugin version?

## Answer
Yes, and it works best if you can share the **source code**. I'd separate the DSP core from the STM32 hardware layer (ADC, DAC, GPIO, codec), then wrap the core in a VCV Rack module with matching knobs, jacks and lights. Firmware for Daisy-style or other C/C++ projects usually ports cleanly.

A compiled .bin or .hex on its own is a different job. That would mean emulating the Cortex-M7 and its peripherals, which is slow, fragile and rarely worth it.

Send the repo or a zip, plus a panel photo or front-panel layout if you have one, and I'll get started.

## Sources
- No links were given in the answer.
