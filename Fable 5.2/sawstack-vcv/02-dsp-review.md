# Sawstack: DSP review

Written 2026-11-23 by Fable. Reviewed: `sawstack_proto2.py` (the AGE and DRIVE prototype) and the envelope code, read line by line and checked by hand calculation. Nothing here has been listened to. Numbers marked "calculated" come from the code's own constants.

## Verdict

The core is sound and I would keep it: five free-running phase oscillators, sine and saw taken from one triangle core, RICHNESS as amplify-and-wrap, linear VCA, per-unit component errors. One thing I checked carefully holds up well: the sine and the saw are 90 degrees apart at the fundamental, exactly as they would be on a real triangle core where the saw is made by flipping half the triangle. That means the middle of TIMBRE dips about 5 dB at the fundamental (calculated) instead of cancelling or adding, which is how the hardware should behave too.

There are eleven things to change before this becomes a build spec. Four affect the sound noticeably, four are correctness problems, three are about cost and practicality.

## Changes that affect the sound

**1. Coupling is modelled the wrong way.** The prototype pulls each oscillator toward the others using the sine of their phase difference. That only produces locking near unison. Real oscillators on one board couple through the power rail, which carries every oscillator's reset spike, so they also lock at octaves and fifths. That matters here more than anywhere, because SPREAD parks the five at octaves. Change: sum the five core signals into one "rail" signal, high-pass it, and let it nudge every oscillator's frequency. Octave locking then falls out for free, and it is cheaper (5 operations per sample instead of 20 sines).

**2. Drift has a hole in it.** There are wanders at 2.5 s and 25 s, and jitter smoothed at 400 Hz, with nothing in between. The range from about 0.1 to 1 second is where a beating unison sounds alive rather than slowly seasick. Change: add a third wander at 0.3 s at about a third of the depth, so the three approximate the 1/f noise real oscillators have.

**3. High notes should go flat, and they do not.** The prototype's tracking error is a straight scale error. Real exponential converters also droop at high frequency, differently per oscillator, and autotune cannot remove it. Change: add a per-oscillator droop of 5 to 20 cents at 4 kHz at full AGE. The effect is that the top of the octave stack sags and beats against the oscillators below it, which is a large part of why wide analog stacks shimmer.

**4. SPREAD is too regular at low AGE.** The offsets are exact multiples (-2, -1, 0, +1, +2), so with AGE down the beating repeats exactly, about every 1.4 seconds for a 10 cent swarm at C3 (calculated). Drift hides this at the aged setting. Change: widen the per-oscillator SPREAD error from 1.5% to 5% at full AGE, so the pattern stops repeating by the default setting without depending on drift.

## Correctness problems

**5. Mono output is wrong.** With one jack patched the prototype would sum left and right, which makes the centre oscillator about 2.4 dB louder than the outer ones (calculated). Change: in mono, mix the five with equal gains.

**6. Envelope times shift with AGE.** Bending the segments changes their length: the fall runs 10% long and the rise 4% short at full AGE (calculated). Change: normalise so the knob time stays true and only the shape and the per-note scatter change.

**7. Jitter depth depends on the sample rate.** It is defined as noise before a smoothing filter, so it would change with Rack's engine rate. At the prototype's rate it comes to about 2 cents at full AGE (calculated). Change: define it as a deviation after smoothing, in cents.

**8. Dead code in the sine shaper.** The tip-folding branch can never run, because the shaper output never exceeds full scale. The sound is as intended (the sine function folds by itself when over-driven), so only the code needs tidying. Also the sine has no turnaround glitch while the saw does. A small one should be on both.

## Cost and practicality

**9. Band-limiting.** The prototype brute-forces everything at eight times the sample rate, which is fine for sketches and far too expensive for 16 voices of five oscillators. Change: band-limit the saw's steps directly (polyBLEP), and apply the per-oscillator saturation to the raw waveform before band-limiting, with the step size taken after shaping. Then only the output stage needs oversampling, at two times.

**10. Hiss and bleed at the default setting.** At AGE 0.6 the closed VCA leaks the oscillators at about -66 dB and the noise floor sits near -78 dB (both calculated). In a quiet patch you would hear a drone that never stops. Change: hiss and bleed only come in above AGE 0.5, so the default is silent when closed, and the top half of the knob is where the unit gets noisy.

**11. Each module needs a fixed personality.** The component errors must be saved with the patch, so a patch sounds the same when reopened, with a right-click "new unit" to roll another. In poly, each voice gets its own unit, as in an analog polysynth.

## Smaller notes

- The shaper offset passes through the VCA, so fast attacks carry a small thump. That is authentic, but I would halve it.
- The level wobble measured in the unison test was 9.6 dB aged and 9.3 dB tired, so above about AGE 0.6 the saturation is absorbing the extra movement. Fine as a behaviour, but it means DRIVE and AGE interact, and the manual should say so.
- I would drop warm-up drift from the earlier list. It is inaudible unless you play against another instrument, and it would put the module out of tune for no musical gain.
- Still an assumption, not a finding: the RICHNESS circuit and the SPREAD range, as in the proposal.
