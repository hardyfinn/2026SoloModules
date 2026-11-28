---
title: "Fitting a TB-303 model from recordings"
date: 2026-11-23
source: "Fable 5.2 · Design Roland TB-303 for VCV Rack"
topic: Music
tags: [tb-303, modelling, recording, disting-nt]
summary: |
 Fitting the 303 model to recordings of a real unit was planned at one to two weeks of calendar time, with about half a day of recording and an hour or two of listening.
 A stock 303 has no audio input to the filter, so the filter is measured from how it reshapes the 303's own saw; a MIDI retrofit and a DC-coupled recorder like the Disting NT make the session more exact.
---

# Fitting a TB-303 model from recordings

## Summary
Fitting the 303 model to recordings of a real unit was planned at one to two weeks of calendar time, with about half a day of recording and an hour or two of listening.
A stock 303 has no audio input to the filter, so the filter is measured from how it reshapes the 303's own saw; a MIDI retrofit and a DC-coupled recorder like the Disting NT make the session more exact.

## Question
> I have a real tb303 and Im interested to know how much time it would take us to improve this modelling by analysing real recordings.  Give me a plan

## Answer
You're on Fable now, and the plan is ready: about one to two weeks of calendar time, with roughly half a day of your time recording and an hour or two of listening.

I checked the plan after switching and corrected one thing. A stock 303 has no audio input to the filter, so the filter is measured from how it reshapes the 303's own saw.

Three answers would let me write the recording sheet: is your 303 stock or modded, what will you record into, and should the fitted values be a selectable profile (my recommendation) or replace the current defaults?

## Question 2
> My 303 is stock but minded by Denton electronics for midi and more cv.

## Answer 2
That's good news for the plan, because a stock sound circuit with MIDI and extra CV makes the recordings easier and more exact. With MIDI I can give you MIDI files to play into the 303, so you don't program patterns by hand and every take has exact note timing. If any of the added CV inputs reach the filter cutoff, we can sweep it from your interface, which is more precise than tape marks on the knob.

I don't know that mod's exact socket layout, so could you list the added sockets as they are labelled, or upload a photo of them? The other two questions still stand: what you'll record into, and whether the fitted values should be a selectable profile or replace the defaults.

## Question 3
> Sorry. Kenton not denton

## Answer 3
Kenton makes sense, thanks. From memory, and I haven't checked this against Kenton's documentation, their 303 retrofit takes notes over MIDI with accent from velocity and slide from overlapping notes, and can also control filter cutoff from MIDI. It was often fitted with sockets for CV, gate, filter, accent and slide inputs.

If yours matches that, the whole session can run from MIDI files I supply, including stepped cutoff sweeps, which would cut your recording time to about two hours. Could you confirm which sockets your unit has, and what you'll record into?

## Question 4
> I would record into disting nt

## Answer 4
The Disting NT is a good fit. It is DC-coupled, so it can record the 303's CV OUT alongside the audio, and its outputs can send CV into the Kenton sockets for exact cutoff sweeps. I'll plan around 48 kHz unless you tell me it can run higher.

The one thing I still need before writing the recording sheet is which Kenton sockets your unit has. I'll go with the selectable profile unless you say otherwise.

## Sources
- None cited in the answer.

Related: [[TB-303 component values that shape the sound]], [[Modelling a Mackie CR-1604 channel from recordings]]
