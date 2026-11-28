# Dub Siren (VCV) — design draft, 2026-11-26

Modelled on Hardy's DIY siren box (SPEED, PITCH, FEEDBACK, MODE 1-4, BEAT 1/2/3/OFF, SIREN VOL, TRIG toggle, SIREN + TONE buttons) and on the 1980s-90s sound-system sirens it descends from: two 555-style oscillators (one LFO sweeping one square-wave VCO), a "let go" tail where the pitch sinks as the timing capacitor drains, all fed into an echo pushed near self-oscillation.

24 HP, brushed aluminium, knob caps coloured like the box. Control labels 3.2 mm, jack labels 2.9 mm, Hær logo, MIT, plugin 2.0.0.

## Controls (same places as the box)
- SPEED (red): LFO rate 0.1-30 Hz. With CLOCK patched, snaps to clock divisions.
- PITCH (red): base pitch, about 40 Hz-4 kHz; V/OCT input adds on top.
- FEEDBACK (yellow): echo repeats; past ~3 o'clock it runs away into self-oscillation, darkening each pass.
- MODE 1-4: 1 falling saw (classic drop "pew pew"), 2 rising saw (whoop), 3 triangle (wail), 4 square (hi-lo two-tone).
- BEAT 1/2/3/OFF: the siren chops itself in a rhythm. 1 straight 8ths, 2 offbeat skank, 3 triplet roll, OFF continuous. Follows CLOCK if patched, else SPEED.
- LATCH toggle (box's TRIG): siren keeps running without holding the button.
- SIREN button: play the swept siren while held (plus SIREN gate in).
- TONE button: steady tone at PITCH, no sweep, the horn blast (plus TONE gate in).
- VOLUME (green): output level, excluded from randomize.

## Extras the box doesn't have
- DEPTH trim: how far the LFO sweeps (box has it fixed).
- RELEASE trim: length of the sinking-pitch tail when you let go (0 = hard stop).
- TIME trim: echo time 40 ms-1 s.
- ERA switch: 80s = rounder 555 square into a tape echo (wow, flutter, warm dark repeats, slight saturation). 90s = harder square into a cheap digital delay (PT2399-style grit, brighter metallic repeats, aliasing on feedback).

## Jacks
In: V/OCT, SPEED CV, FDBK CV, CLOCK, SIREN gate, TONE gate, IN (audio into the echo, so it doubles as a dub delay for other parts). Out: OUT.

## Open question
What BEAT does on the real box (guessed above).
