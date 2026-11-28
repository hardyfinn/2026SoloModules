---
title: "Modelling a Mackie CR-1604 channel from recordings"
date: 2026-11-21
source: "Fable 5.2 · Suggest next 20 VCV modules, Mackie Emulation"
topic: Music
tags: [mackie, mixing-desk, saturation, null-test, modelling]
summary: |
 A real desk can be modelled by playing known test signals through it, recording the result at set gain and EQ settings, fitting the model and checking it with a null test.
 The famous Mackie sound is mainly the mix bus overloading when many hot channels are summed, and adding synthesised 808 and 909 style drums and an acid line to the test file helps the fit where it matters.
---

# Modelling a Mackie CR-1604 channel from recordings

## Summary
A real desk can be modelled by playing known test signals through it, recording the result at set gain and EQ settings, fitting the model and checking it with a null test.
The famous Mackie sound is mainly the mix bus overloading when many hot channels are summed, and adding synthesised 808 and 909 style drums and an acid line to the test file helps the fit where it matters.

## Question
> I'm interested in number 13 on this grid, your hot Mackie overdriven desk emulation. I'd be interested to hear how that sounds. I have a Mackie CR desk, which I would love to get an accurate model for. Can you tell me if we could create a modeling suite for this where we use real recordings at various levels of gain on the Mackie for you to make a better emulation? How would this work?

## Answer
Yes, we can do this, and having the real desk makes a much better model than working from the schematic alone. The method is to play known test signals through the Mackie, record what comes back, and fit the model until its output matches your recordings.

**How it would work**

1. **I make a test signal pack.** One WAV file of a few minutes containing sine sweeps (these reveal the frequency response and every harmonic the desk adds), stepped tones from quiet to very hot (the saturation curve), pairs of tones (the intermodulation that makes a hot desk sound gritty), drum-like bursts (how it clips and recovers on transients), plus a drum loop and a music excerpt for checking at the end.
2. **You record it through the desk.** Interface output into a channel, main out back into the interface, at a list of settings I give you. Roughly: a loopback with no desk at all (so I can subtract your interface), trim at four or five positions, each EQ band at minimum, centre and maximum, the channel fader pushing the mix bus, and several channels fed at once. That last one matters most, because the famous Mackie sound is mainly the mix bus overloading when many hot channels are summed.
3. **You upload the recordings here**, named according to the sheet.
4. **I fit the model.** The circuit diagram gives the structure (trim stage, EQ, summing amp, output stage) and your recordings supply the real numbers for each stage: where each one starts to clip, how hard, how asymmetrically, and how that changes with frequency.
5. **I check it by null test.** I run the same test file through the model, subtract it from your recording, and report how much is left over, by frequency and by level. The drum loop and music were not used for fitting, so they are an honest check.

**What to expect**

The EQ curves and the steady-state distortion should match very closely. The harder part is behaviour while it is clipping hard on transients, which is where I would spend most of the effort. I analyse numbers and cannot listen, so your ears are the final judge, and the result is a model of your particular desk on that day.

Three things I need to know before I make the test pack:
- Which model exactly is it (CR-1604, CR-1604 VLZ, CR-1202, or another)?
- Which audio interface do you have? It needs line outputs hot enough to push the desk into clipping, and I will set the levels around it.
- Would you want the module to be one channel strip, or the whole desk with channels, EQ, sends and the mix bus?

## Question 2
> As this emulation will largely be used for house techno drum machine synthesizers, would it be appropriate to add those as test recordings? That's really the sound I'm after.

## Answer 2
Yes, and it will make the fit better where it matters. I'll put synthesised 808 and 909 style kicks, snares, hats, a full drum pattern and an acid bassline into the test file at several levels. You can also add a minute of your own machines: the sheet will explain how to record them dry and through the desk so I can compare the two.

## Sources
- None cited in the answer.

Related: [[Pre CR channel clipping stages]], [[Fitting a TB-303 model from recordings]]
