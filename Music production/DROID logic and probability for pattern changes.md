---
title: DROID logic and probability for pattern changes
date: 2026-10-16
source: Music production · Music production interview
topic: Music
tags: [droid, eurorack, probability, sequencing]
summary: |
 How to use logic and probability on a DROID to change patterns over time.
 Includes a full ready-made DROID patch (pattern-mutator.ini).
---

# DROID logic and probability for pattern changes

## Summary
How to use logic and probability on a DROID to change patterns over time.
Includes a full ready-made DROID patch (pattern-mutator.ini).

## Question
> tell me more about item 4 - specifically how do i use logic and probability in droid to change patterns

## Answer
Here's the idea, with a ready-made patch in pattern-mutator.ini. DROID patches are a short list of settings rather than programming, so you only change numbers and jack names.

**Signal flow**
1. Live's 16th-note clock goes into DROID **I1**, and reset goes into **I2**.
2. A clock divider fires once every 4 bars.
3. On each of those pulses, DROID rolls a random number and compares it with pot **P1.1**. That makes a weighted coin, where turning P1.1 up makes changes more likely.
4. An AND gate with button **B1.1** (ARM) means changes only happen when you allow them.
5. **O1** goes to Marbles' déjà vu CV input. Set the knob low so the CV locks the pattern, and a "change" drops the lock for 4 bars so Marbles writes something new.
6. **O2** sends a reset pulse to the Metropolix at the same moment, which marks the change as a phrase boundary.

**Variations**
- Put an OR on a second coin running at 8 bars, so changes cluster unevenly.
- Send O2 to the Metropolix's aux input instead and assign it to transpose or a pattern change.
- Use DROID's algoquencer for drums. It has its own déjà vu and activity controls.

I'm sure of how the patch is structured, but please check the circuit names (especially the logic outputs `and` and `nand`) against your firmware's manual, since the version I could find online is older. I couldn't find a good online source beyond [this manual copy](https://www.manualslib.com/manual/2826230/Der-Mann-Mit-Der-Maschine-Droid-Master.html).

### The patch: pattern-mutator.ini

```ini
# PATTERN MUTATOR for DROID master18 + P2B8 (controller 1)
# Every 4 bars DROID flips a weighted coin. If it says "change"
# (and the ARM button is on), Marbles is unlocked for 4 bars so it
# writes a new pattern, and the Metropolix gets a reset pulse.
#
# Jacks:  I1 = clock in (16th notes from Live CV Tools via disting NT)
#         I2 = reset in (from Live CV Tools)
#         O1 = to Marbles DEJA VU CV input (high = pattern locked)
#         O2 = to Metropolix RESET input (pulse when a change happens)
# Controls: P1.1 = chance of change (left = never, right = always)
#           B1.1 = ARM on/off (LED L1.1 shows it)

# 1. Turn the 16th-note clock into one pulse every 4 bars
[clocktool]
clock = I1
reset = I2
divide = 64              # 16 for a quarter-note clock
output = _BAR4

# 2. Roll a random number (0 to 1) on every 4-bar pulse
[random]
clock = _BAR4
minimum = 0
maximum = 1
output = _ROLL

# 3. Weighted coin: "change" if the roll is below the pot setting
[compare]
input = _ROLL
compare = P1.1
ifless = 1
ifgreater = 0
output = _CHANGE

# 4. ARM button, so you can freeze everything while performing
[button]
button = B1.1
led = L1.1
output = _ARM

# 5. Logic: change only when the coin says so AND you are armed
[logic]
input1 = _CHANGE
input2 = _ARM
and = O2                 # reset pulse to Metropolix
nand = O1                # lock Marbles the rest of the time
```

## Sources
- Project file: pattern-mutator.ini (DROID patch, reproduced above)

Related: [[Beating loop fatigue in arrangement]]
