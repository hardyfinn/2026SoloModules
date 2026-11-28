---
title: Fancyyyyy Rung Divisions overview
date: 2026-11-02
source: Music production · Fancyyyyy Rung Divisions
topic: Music
tags: [eurorack, clock-division, shift-register, rungler]
summary: |
 Rung Divisions is a clock divider feeding a Rungler-style 16-step shift register.
 Chance sets whether the register loops, drifts or takes in new data; outputs are 1-bit, 3-bit and 8-bit CV plus noise.
---

# Fancyyyyy Rung Divisions overview

## Summary
Rung Divisions is a clock divider feeding a Rungler-style 16-step shift register.
Chance sets whether the register loops, drifts or takes in new data; outputs are 1-bit, 3-bit and 8-bit CV plus noise.

## Question
> new thread. Fancyyyyy rung divisions

## Answer
**What it is:** Rung Divisions is a clock divider feeding a Rungler-style shift register, 12HP. One clock goes in and you get divisions /2 to /8. A 3-way switch next to each division sends it to Gate Bus 1, Gate Bus 2, or off, and each bus ORs together whatever you send it. Bus 1 also clocks a 16-step shift register that can run in either direction. The **Chance** knob decides what that register does: fully clockwise it loops and locks, fully counter-clockwise the Data input gets XORed in, and in between it's noisy. **Length** sets the loop point. Outputs are a 1-bit gate, a 3-bit stepped CV (8 levels), an 8-bit stepped CV (256 levels), and noise. ([Perfect Circuit](https://www.perfectcircuit.com/fancyyyyy-rung-divisions-v2.html), [manual excerpt](https://manuals.online/fancyyyyy/synthesizer/rung-divisions/user-manual/pgnr))

**In your rig:** Drive it from Live's clock through the NT. The buses give you polyrhythmic drum triggers, the 3-bit output can drive Metropolix or Just Friends pitch through a quantiser, and nudging Chance is a good way to get Autechre-style patterns that keep changing.

[…]

## Sources
- [Perfect Circuit product page](https://www.perfectcircuit.com/fancyyyyy-rung-divisions-v2.html)
- [Manual excerpt](https://manuals.online/fancyyyyy/synthesizer/rung-divisions/user-manual/pgnr)
