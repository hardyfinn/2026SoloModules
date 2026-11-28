---
title: "VCV Rack expanders versus built-in controls"
date: 2026-11-21
source: "Fable 5.2 · Design 90s vinyl sound module"
topic: Tech & Work
tags: [vcv-rack, expanders, module-design, plugin-development]
summary: |
 Expanders make no difference to sound when the processing stays in the main module; the trade-off is panel width and handling.
 Expanders keep the main module narrower and can be built separately, but must touch the main module in a set order and their knobs are not saved in the main module's presets.
---

# VCV Rack expanders versus built-in controls

## Summary
Expanders make no difference to sound when the processing stays in the main module; the trade-off is panel width and handling.
Expanders keep the main module narrower and can be built separately, but must touch the main module in a set order and their knobs are not saved in the main module's presets.

## Question
> Is there any advantage or disadvantage to making a couple of these expanders rather than built-in?

## Answer
There's no difference in sound. An expander here is only a panel of extra controls, and the processing happens inside Vinylizer Fable either way. The trade-off is about the panel and how you use it.

**For expanders**
- The main module stays at 50 HP. Building in PRESSING, BOOTH and MUSIC CENTRE would add about 34 HP, making an 84 HP module, which is wider than many people's whole row.
- You only take up rack space for the parts you want in a given patch.
- Each one can be built, tested and changed on its own without touching a module that already works.

**Against expanders**
- They must sit touching the main module, in a set order on its side. If one is moved away, it silently stops doing anything.
- Settings are split across panels, so a preset saved on the main module doesn't include the expander's knobs unless you save the whole selection.
- It's a little more code, and one more thing that can go wrong.

**A middle option for PRESSING**
PRESSING is the one you asked for and will probably always want. It could be built in by making the main module about 64 HP, with BOOTH and MUSIC CENTRE left as optional expanders. That would mean redrawing the main panel.

I'd keep all three as expanders, because 50 HP is already large and the processing cost is the same. If you'd rather never think about placement, building in PRESSING is the one I'd pick. DECK and MASTER are separate modules either way.

## Sources
- None cited in the answer.

Related: [[How vinyl cutting and playback shape the record sound]]
