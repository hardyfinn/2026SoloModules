# Spec review: Just Fiends, Weird Sisters, Mangrave vs the Mannequins hardware

Review only. No code was changed, nothing was committed. Code reviewed: repo `just-fiends` at commit `263a092` (version 2.4.0).

## What I read, and what I could not

| Source | Status |
|---|---|
| Mannequins Technical Maps (Just Friends, Three Sisters, Mangrove) | **Read in full.** The GitHub API was blocked, but a plain `git clone` of the repo worked. I read the Markdown sources of the maps (the PDFs in the repo are built from these same files) and looked at the Three Sisters and Mangrove block-diagram pictures. Quotes below are from these. |
| Mangrove + PPM user manual (Shopify PDF) | **Read second-hand only.** Direct download was blocked. I could only question it through a web-fetch tool that summarises and gives short quotes. The short quotes below are what that tool returned as exact wording; I could not check them against the PDF myself. Treat PPM findings as less certain. |
| whimsicalraps.com Mangrove page, ModularGrid PPM page | Read through the same tool. They hold almost no technical detail (ModularGrid: one sentence, 2 HP). |
| whimsicalraps.com Just Friends and Three Sisters product pages, wiki | **Not read** (blocked, and no link I was allowed to fetch). Nothing below relies on them. |
| Just Type (Teletype/i2c modes: Synthesis, Geode) | **Not read.** The Technical Map does not cover them. I have not judged "poly pitch" and "Geode" against any spec. |

Things the published specs simply do not state (so I mark them "spec unclear" rather than guess): exact knob ranges in Hz, noon values, FM depth scaling, output levels of Mangrove SQUARE and the PPM outputs, SPAN's range in octaves.

Severity words: **wrong** (contradicts the spec), **partly** (right idea, details differ), **matches**, **extra-by-design** (an intended learning aid), **spec unclear**.

---

## Summary

**Just Fiends: the basic slope controls are right, but it is not yet a faithful Just Friends.** TIME, INTONE (at both ends), RAMP, CURVE, the 0-8 V and ±5 V levels, ASR, LFO and oscillator modes all match. But seven things contradict the Technical Map, and several of them break the hardware's best-known tricks:

1. Trigger normalling runs the wrong way (hardware: patch 6N and it fires everything to its left; here: patch IDENTITY and it fires everything to its right).
2. In Transient mode a new trigger during the fall restarts the slope. Hardware ignores triggers for the whole rise and fall. This is what makes clock division and subharmonics work on the hardware; here they do not.
3. The FM knob is exponential with "anticlockwise = inverted". Hardware is linear FM, with clockwise = all channels equally and anticlockwise = INTONE-style (more on higher channels).
4. Sustain + Sound is "gated free-running oscillators" here. Hardware is a gate-following trapezoid wave driven by an external oscillator.
5. Shape-range MIX is a plain maximum. Hardware divides each slope by its channel number first (8 V, 4 V, 2.66 V...), which is what makes the staircase/sequence tricks.
6. Five of the six RUN modes differ from the hardware: Strata, Spill and Floom are different designs, Plume's RUN direction is reversed and it ignores held gates, Shift and Volley use different voltage maps. The negative half of the RUN knob does nothing in five modes.
7. Sound + Transient impulses come out 0 to +5 V instead of -5 to +5 V.

The guide admits Spill, Floom and MIX are "interpretations" but does not mention items 1-4, and it tells the user the patches "transfer to the real module with the same knob positions", which is not true for those items.

**Weird Sisters: Formant mode is close; Crossover mode and the anticlockwise half of QUALITY are not Three Sisters.** FREQ, V/oct, LOW = FREQ minus SPAN, HIGH = FREQ plus SPAN, formant-mode band passes, resonance and self-oscillation are right in kind. But CENTRE in crossover mode is a fixed band pass that ignores SPAN (hardware: a band whose width is SPAN, which SPAN can close like a VCA); anti-resonance does not bring the removed frequencies back (hardware: a dry/wet control that passes the whole spectrum fully anticlockwise); ALL IN replaces rather than adds to the individual inputs; SPAN cannot go "negative"; self-oscillation starts much later on the knob than 3 o'clock.

**Mangrave: the closest of the three. Core, impulse, FORMANT, BARREL, AIR, CONSTANT switch and FM INDEX match the Technical Map well.** Main gaps: in classic IMPULSE mode the impulse cannot be retriggered while falling, so the hardware's "multiphonics / split tones" cannot happen; the FM input passes DC (hardware is AC-coupled); in Constant Wave the impulse length ignores FM; SYNC is a hard reset where the Technical Map says soft (the PPM manual says hard, so the sources disagree). PPM behaviour agrees with what I could read of the PPM manual, with HOLLOW the least certain.

---

## Just Fiends (Just Friends)

Source: Just Friends Technical Map ("JF map"), section names given. Code: `src/FiendsEngine.hpp` (engine), `src/JustFiends.cpp` (module).

| # | What the spec says | What the code does | Severity | Suggested fix |
|---|---|---|---|---|
| 1 | **Trigger normalling goes right to left.** JF map, "TRIGGER Input Normalling": "a trigger patched to an input will cascade to the other unpatched inputs to its left... A trigger patched to 6N (while no other trigger inputs are patched) will excite all 6 inputs." | Cascades left to right: IDENTITY feeds 2N...6N; a cable in 6N fires only 6N. `JustFiends.cpp:236-257`. Tooltips (`:174-175`) and the guide ("One cable into IDENTITY fires all six") describe the reversed direction. | **wrong** | Run the normalling loop from 6N down to IDENTITY; update tooltips and guide. |
| 2 | **Transient: triggers are ignored for the whole slope.** "Incoming triggers are ignored while a slope generator is active. Once the slope generator reaches the end of the release phase, it becomes receptive to triggers again." Clock division and pitch division rely on this. | A trigger during the fall restarts the rise from the current level; only the rise is protected. `FiendsEngine.hpp:198-201`. Measured: a 1 s envelope triggered every 0.7 s fires 6 times out of 6; the hardware rule gives 3. | **wrong** | Accept a trigger only when the slope is idle (in both Shape and Sound transient). |
| 3 | **FM knob.** "FM knob & FM INPUT jack": noon = none; clockwise = "depth of the modulation linearly applied to all slopes equally"; anticlockwise = "in the INTONE style, i.e. in proportion to each channel's index". Linear through-zero. Shape range DC-coupled, Sound range AC-coupled. | Exponential FM in octaves per volt on all channels; anticlockwise simply inverts it. Always DC-coupled. `JustFiends.cpp:226`, `FiendsEngine.hpp:150-154`. Guide says "Exponential FM... Anticlockwise inverts it". | **wrong** | Make FM linear; clockwise adds to TIME, anticlockwise adds to INTONE; AC-couple it in Sound range. |
| 4 | **Sustain + Sound = trapezoid waves that follow the incoming gate.** "the slopes become trapezoid impulse-train VCOs. An external audio-rate clock source... must be patched... they will track the pulse-width of the clock source." It is the same ASR as Shape, just fast. | Six free-running oscillators at the TIME pitch, switched on and off by the gate through a 2 ms VCA. `FiendsEngine.hpp:157, 229-231, 327`. The pitch comes from TIME, not from the gate signal. | **wrong** | Use the same ASR logic as Shape/Sustain, with the ±5 V output. |
| 5 | **Shape MIX.** "Each slope's output value is divided by its index. The MIX jack then outputs the largest of the resulting values." IDENTITY high = 8 V, only 2N high = 4 V, only 3N = 2.66 V. | Plain maximum of the six, no division. `FiendsEngine.hpp:372-376`. The guide states this openly. | **wrong** | Divide channel N by N before taking the maximum. |
| 6 | **Sound MIX.** "an equal mix of all the current slopes, limiting the final amplitude to ~15V peak-to-peak w/ tanh shaping." | Sum x 0.4 through tanh that tops out at ±10 V (20 V peak to peak). Six unison squares measured 8.3 V peak. `FiendsEngine.hpp:377-381`. | partly | Scale the tanh to top out at ±7.5 V. |
| 7 | **Sound range output is -5 to +5 V**, and an impulse "rests at its minimum". | In Sound + Transient the impulse is 0 to +5 V and rests at 0 V. `FiendsEngine.hpp:352-355`. | **wrong** | Output `curve x 10 - 5` V so it rests at -5 V. |
| 8 | **STRATA** (Shape/Sustain + RUN). An ARSR envelope: gate high = rise to maximum, fall to a sustain level set by RUN, hold; gate low = fall to 0. Slew-limiter use is the special case of holding the gate high. RUN "is remapped from (-5V,5V) to (0V,8V)" and CURVE is applied. | No attack to the top; channels just slew toward RUN. Gate low **freezes** the channel instead of releasing to 0. With nothing patched all channels track (hardware: nothing happens without a gate). RUN is read as 0-8 V directly; CURVE is not applied. `FiendsEngine.hpp:312-322, 348`. | **wrong** | Implement rise to max, fall to sustain, release to 0 on gate low; map -5..+5 V to 0..8 V; apply CURVE. |
| 9 | **PLUME** (Sound/Sustain + RUN). Free-running oscillators through low pass gates. "the trigger inputs are gate sensitive, allowing the LPGs to be held open". "Positive RUN voltages shorten the attack and decay"; negative lengthen. | Pluck only: after a 1 ms attack it always decays, even while the gate is held. RUN direction is **reversed**: 0 V = 20 ms, 5 V = 3.9 s. Negative RUN does nothing. `FiendsEngine.hpp:232-234, 280, 336-342`. | **wrong** | Hold the gate open while high; make higher RUN shorter and use the negative half for longer. |
| 10 | **SPILL** (Sound/Transient + RUN). IDENTITY free-runs and its end-of-cycle fires the other channels as impulses; RUN moves the retrigger point as in Shift. No external trigger needed. | Each external trigger plays a timed burst of free-running tone; RUN sets the burst length (5 ms to about 2 s). `FiendsEngine.hpp:225-228, 328-331`. Guide admits this is an interpretation. | **wrong** (declared) | Clock channels 2N-6N from IDENTITY's cycle end, with RUN as retrigger point. |
| 11 | **FLOOM** (Sound/Cycle + RUN). Each voice gets its own internal sine modulator. RUN sets the modulator ratio: "At -5V... half the frequency... At 0V... the same... At +5V... double". FM knob = depth; "The FM INPUT jack... becomes an FM depth input". | A ring: each voice is modulated by its neighbour's output; RUN sets depth (0-5 V); FM knob and jack stay ordinary FM. `FiendsEngine.hpp:282, 294`. Guide admits this is an interpretation. | **wrong** (declared) | One sine modulator per voice, RUN = ratio 0.5x to 2x, FM knob/jack = depth. |
| 12 | **SHIFT** (Shape/Transient + RUN). -5 V = always retriggerable; 0 V = after the end of the rise; +5 V = only when finished. "When a slope gets retriggered, it immediately resets to 0V and begins the rise phase". | Uses 0 to 5 V only: 0 V = always, 5 V = finished; negative RUN does nothing. A retrigger during the fall turns round from the current level instead of resetting to 0 V. `FiendsEngine.hpp:139, 202-210`. | partly | Map -5 V..0 V..+5 V to start, end of rise, end of cycle; reset to 0 V on retrigger. |
| 13 | **VOLLEY** (Shape/Cycle + RUN). "below -4V... choked"; "At -4V, 1 envelope"; "At 0V... 6 repetitions"; "At +5V... 36". Count latched at the trigger. | 1 cycle at 0 V (and below) up to 16 at 5 V. Latched at the trigger (matches). No choke. `FiendsEngine.hpp:220-224`. | partly | Use the hardware map: off below -4 V, 1 at -4 V, 6 at 0 V, 36 at +5 V. |
| 14 | **RUN is bipolar, -5 to +5 V, and a dummy cable = 0 V.** | The RUN knob is -5..+5 V but every mode except Strata clips it to 0..5 V, so the left half of the knob is dead. `FiendsEngine.hpp:139`. The knob and ON button themselves are intended extras. | partly (knob/button: extra-by-design) | Fix with items 8-13. |
| 15 | **INTONE.** Noon = all equal; fully clockwise 2N = 2x ... 6N = 6x; fully anticlockwise 1/2 ... 1/6. CV ±5 V = full sweep. "The knob has a special shaping to enable finer control around noon." | Ratio = N to the power of the knob: exact at noon and both ends. CV scaling matches. No special shaping near noon; the curve between the ends is this plugin's own. `FiendsEngine.hpp:148`, `JustFiends.cpp:220-223`. | matches (ends) / spec unclear (in between) | None needed; optionally flatten the knob near noon. |
| 16 | **RAMP.** Anticlockwise = "instant attack/long release", noon = triangle, clockwise = "long attack/instant release". Total time unchanged. CV ±5 V = full sweep. | Same. `FiendsEngine.hpp:278`. | matches | - |
| 17 | **CURVE.** Anticlockwise rectangle, then logarithmic, linear at noon, exponential, sine fully clockwise. Rectangle is high for the rise phase, so RAMP becomes pulse width. | Same order and same rectangle rule. `FiendsEngine.hpp:101-121`. | matches | - |
| 18 | **Levels.** Shape 0-8 V; Sound -5 to +5 V. | Shape 0-8 V; Sound cycle ±5 V. (Sound/Transient differs, item 7.) | matches | - |
| 19 | **Sustain + Shape (ASR).** Rise while high, hold, fall when low; a new gate "will immediately resume rising from its current value". | Same. `FiendsEngine.hpp:211-213, 306-307`. | matches | - |
| 20 | **Cycle.** Loops; a trigger resets to the start of the cycle (hard sync); rising edge only. | Same. `FiendsEngine.hpp:214-219`. | matches | - |
| 21 | **TIME.** Clockwise = faster; 1 V/oct; "roughly -2V to +5v. Voltages outside this range will be clamped". Ranges given only as "minutes to milliseconds" and "Hz to kHz". | Clockwise faster, 1 V/oct. No clamp on the CV. Ranges about 2 minutes to 128 Hz (Shape) and 8 Hz to 8.4 kHz (Sound), middle C at noon: plausible but the spec gives no numbers. `FiendsEngine.hpp:150-154`. | matches / spec unclear (ranges) | Optionally clamp the TIME CV to -2..+5 V. |
| 22 | **Trigger threshold** "around 1V". | Turns on above 0.6 V, off below 0.3 V. `FiendsEngine.hpp:299`. | partly (minor) | Raise to about 1 V on, 0.5 V off. |
| 23 | Hardware has no velocity. | Menu option, on by default: in Sound range the trigger voltage sets loudness. This changes behaviour (see bug A). `FiendsEngine.hpp:303`. | extra-by-design, but changes hardware behaviour | Default it to off. |
| 24 | Poly pitch, Geode | Menu modes standing in for Teletype/i2c. I had no spec to check them against. | extra-by-design / not checked | - |
| 25 | Display, tooltips, trigger buttons, CV trimmers, poly in/out | Do not alter behaviour. | extra-by-design | - |

### Guide claims about Just Fiends (`docs/JustFiends-Guide.html`)

| Guide says | Against the spec |
|---|---|
| "Each jack also feeds every unpatched jack to its right. One cable into IDENTITY fires all six." Also patches 1, 2, 5 and "Pam's... into IDENTITY and 4N to split the normals" in the hardware section. | Wrong for the hardware (item 1). On hardware you patch 6N to fire all six. |
| "Triggers during the rise are ignored, so a fast clock gets divided naturally." | Hardware ignores triggers during the rise **and** the fall. With the current code a fast clock is not divided cleanly (item 2). |
| "FM: Exponential FM of TIME... Anticlockwise inverts it." | Not what the hardware does (item 3). Not listed under "differences". |
| Sound/Sustain: "Gated oscillators... Gate voltage sets the level." | Hardware: trapezoid waves driven by the gate (item 4). Not listed under "differences". |
| Strata "A low gate freezes that channel"; Volley "1 at 0 V up to 16 at 5 V"; Plume "RUN sets the decay, from 20 ms to about 4 s"; Shift "0 V: any trigger restarts". | All differ from the hardware numbers and directions (items 8, 9, 12, 13). The differences section only says "details are this module's own versions". |
| "Everything above transfers to the real module in your case, with the same knob positions as starting points." | Not true for normalling, FM, RUN voltages, Sustain/Sound, MIX. Should be reworded or the engine fixed. |
| "MIX here is the highest slope in Shape range and a soft-clipped sum in Sound range." | Honest about the difference, but it is easy to match the hardware (item 5). |
| INTONE, RAMP, CURVE, RANGE levels, ASR, LFO descriptions. | Correct. |
| "TIME... 1 Hz at noon, from about two minutes to 128 Hz." | True of the plugin; the spec gives no numbers to compare. |

### Bugs found in Just Fiends (not spec questions)

- **A. Loudness is read at the instant the trigger crosses 0.6 V** (`FiendsEngine.hpp:303`). With a square gate that is fine. With anything that rises gradually (a sine or triangle oscillator, an envelope) the "velocity" is about 12%. Measured: a ±5 V sine into IDENTITY in Sound/Transient gives impulses of only 0.66 V. The spec explicitly expects oscillators as trigger sources. Fix: turn velocity off by default, or track the gate's peak for a few milliseconds.
- **B. Stuck voltage after leaving Strata.** Turning RUN off in Shape/Sustain (or switching mode) leaves each channel parked at its last level until its next gate. Measured: 4.00 V still there 5 s later. Cause: Strata sets the slope to "idle" while it is above 0 (`FiendsEngine.hpp:321`). Fix: when leaving Strata with the level above 0 and no gate, start a fall.
- **C. Trigger buttons do nothing in Strata when no jack is patched**, because unpatched channels always track (`FiendsEngine.hpp:313`).
- **D. Tests lock in non-hardware behaviour** (`test/engine_test.cpp`: Volley = 16 cycles, "a low gate freezes that channel in strata", "a 2.5 V gate plays at half level"). They will need changing with any fix. There is no test for normalling direction or for trigger-skipping.
- Small: `JustFiends.cpp:248-249` sets the same flag twice (harmless).

---

## Weird Sisters (Three Sisters)

Source: Three Sisters Technical Map ("3S map") and its block diagram `block_diagram_overview.png`. Code: `src/SistersEngine.hpp`, `src/WeirdSisters.cpp`.

| # | What the spec says | What the code does | Severity | Suggested fix |
|---|---|---|---|---|
| 1 | **CENTRE in crossover mode is a variable-width band.** "the first SVF's cutoff frequency (highpass mode) is equal to the LOW block's cutoff frequency, and the second SVF's cutoff frequency (lowpass mode) is equal to the HIGH block's cutoff". "CENTRE's pass band is widened and narrowed by SPAN". | CENTRE is a fixed-width band pass at FREQ; SPAN does not touch it. `SistersEngine.hpp:67-70, 91, 101`. Measured: CENTRE's response is identical at SPAN 0, half and full. With wide SPAN there are holes between the three bands. | **wrong** | Build CENTRE as high pass at the LOW cutoff into low pass at the HIGH cutoff. |
| 2 | **SPAN can close CENTRE like a VCA.** "When the 'upper' cutoff passes below the 'lower' cutoff (ie. when SPAN is CCW of 10:00), the signal is increasingly attenuated." So about 10 o'clock is "zero span" and below that the cutoffs cross. | SPAN runs 0 to 4 octaves and is clipped at 0; fully anticlockwise all three sit at FREQ. No crossing, no gating. `SistersEngine.hpp:27`. | **wrong** | Let SPAN go negative below about 10 o'clock (needs fix 1 for the gating to work). |
| 3 | **Anticlockwise QUALITY is a dry/wet control.** "mixes the frequencies above LOW's cutoff back in". LOW gets the first filter's high pass, HIGH gets its low pass, CENTRE gets both. "Fully CCW, the entire spectrum of white noise is passed", with a notch around the cutoff because the two are out of phase. | LOW and HIGH: a band-pass signal is subtracted; the removed frequencies never come back. Measured at fully anticlockwise, LOW still passes only 0.1% at ten times its cutoff, and there is no dip at the cutoff either (level there goes up, 0.50 to 0.87). CENTRE: becomes a pure notch (band fully removed, rest passed), which is close to the hardware picture but loses the band itself. `SistersEngine.hpp:72-75`. | **wrong** (LOW, HIGH) / partly (CENTRE) | Mix in the first stage's opposite output, inverted, scaled by the anticlockwise amount, as the map describes. |
| 4 | **ALL IN is added to each sister's own input.** The block diagram shows each block with two inputs (its own and ALL). Text: "Patching a signal to ALL(IN) will result in a signal that is always present in the mix". | ALL IN reaches a sister only if that sister's own jack is empty (it is replaced, not added). `WeirdSisters.cpp:95-98`. Guide: "ALL feeds every sister with nothing patched". | **wrong** | Feed each sister `own input + ALL`. |
| 5 | **Self-oscillation point.** "When QUALITY reaches approximately 3 o'clock, the filter blocks will go into self-oscillation". Pinging: "just below the point of self-oscillation (2:00 to 3:00)". | Measured: nothing until about 90% of the clockwise half (roughly 4:30); 3 o'clock is still far from ringing on its own. `SistersEngine.hpp:29-33`. | partly | Re-map the knob so damping reaches zero at 3 o'clock. |
| 6 | **Self-oscillation level.** The map speaks of "3 sine-waves at 10V peak-to-peak" and ALL OUT distorting when all three sing. | Sine of about 4.5 V peak (9 V peak to peak) each; ALL about 8.5 V peak, soft-clipped at 10 V. | matches (close) | - |
| 7 | **LOW cutoff = FREQ - SPAN, HIGH = FREQ + SPAN**, in volts (octaves). | Same. `SistersEngine.hpp:88-92`. | matches | - |
| 8 | **Formant mode: three band passes, 12 dB/oct**; CENTRE "is unaffected by SPAN in FORMANT mode". | Three 4-pole band passes with 12 dB/oct sides; CENTRE at FREQ. (Built from two band passes rather than low pass + high pass; the shape is the same.) | matches | - |
| 9 | **Crossover LOW and HIGH: 24 dB/oct low pass and high pass.** | Two 2-pole stages in series each. `SistersEngine.hpp:62-70`. | matches | - |
| 10 | **QUALITY noon = minimum resonance, clockwise = more.** CV ±5 V = full sweep with knob at noon. | Same. `WeirdSisters.cpp:94`. | matches | - |
| 11 | **FREQ jack is 1 V/oct; FM goes through an attenuverter; SPAN CV adds to the knob.** | Same. FM is octaves per volt at full; SPAN CV 1 V = 1 octave at full. The spec gives no scaling for these two. | matches / spec unclear (scaling) | - |
| 12 | **ALL OUT** "an equal mix of all three", can distort when loud. | Sum of the three, soft-clipped at ±10 V. `SistersEngine.hpp:105`. | matches | - |
| 13 | FREQ knob range, SPAN range in octaves | Code: 20 Hz to 20 kHz (640 Hz at noon), SPAN up to 4 octaves each way. Not published. | spec unclear | - |
| 14 | Display, tooltips, CV trimmers, polyphony | Do not alter behaviour. | extra-by-design | - |

### Guide claims about Weird Sisters

| Guide says | Against the spec |
|---|---|
| "SPAN... 0 to 4 octaves each way. Fully anticlockwise, all three meet at FREQ." | Hardware meets at about 10 o'clock and crosses over below it (item 2). |
| "CROSSOVER: LOW is a low pass, CENTRE a band pass, HIGH a high pass, so the three outputs split the sound into bands that add back up at ALL." | On hardware yes, because CENTRE fills the gap between the cutoffs. In this code CENTRE is a fixed narrow band, so they do not add back up when SPAN is wide (item 1). |
| "Anticlockwise is anti-resonance: CENTRE turns into a notch and the edges get dips." | Hardware: all three become dry/wet blends back to the full signal, with notches. In the code LOW and HIGH do not even get a dip (item 3). |
| "ALL feeds every sister with nothing patched." | Hardware: ALL feeds every sister always, added to its own input (item 4). |
| "fully clockwise the sisters sing on their own"; patch 10: "3 o'clock (they ring after each ping)". | Hardware sings from about 3 o'clock; ping zone is 2 to 3 o'clock (item 5). |
| The guide's closing note says formant mode and anti-resonance "are interpretations". | Fair warning for anti-resonance, but crossover CENTRE and ALL IN are not mentioned. |

### Bugs found in Weird Sisters

- **Display's ALL curve adds loudness and ignores phase** (`WeirdSisters.cpp`, display loop). The real ALL output has cancellations between the sisters (the map mentions "various notching"); the curve will not show them.
- **Poly channel count ignores FM, SPAN and QUALITY inputs** (`WeirdSisters.cpp:82`): a poly CV alone will not create extra channels. Minor.
- Only one of the two filter memories is range-limited (`SistersEngine.hpp:54`); the "not finite" check catches blow-ups, so no audible fault found. The fuzz test passes.

---

## Mangrave (Mangrove + PPM)

Sources: Mangrove Technical Map ("MG map", read in full) and block diagram; PPM manual ("PPM manual", second-hand short quotes only, see top). Code: `src/GroveEngine.hpp`, `src/Mangrave.cpp`.

### Mangrove core

| # | What the spec says | What the code does | Severity | Suggested fix |
|---|---|---|---|---|
| 1 | **The impulse can be retriggered while it is falling** (this makes multiphonics). MG map, "Multiphonics": "In this brief falling state, the impulse generator is receptive to triggers: if it receives a trigger, the generator turns around and starts rising again." Triggers during the rise are ignored. | In IMPULSE (classic) mode a trigger is accepted only when the impulse is at rest. The turn-around-while-falling rule exists but only in PPM SYNC and CYCLE. `GroveEngine.hpp:76-84, 122-124`. So split tones cannot happen in classic mode; divisions are always clean. | **wrong** (for multiphonics) | Allow the turn-around during the fall in IMPULSE mode too. |
| 2 | **FM input is AC-coupled.** "it blocks DC signals, so it cannot be used as a pitch offset." | DC passes: 2.5 V steady on FM moves the core from 261.6 to 392.4 Hz. `GroveEngine.hpp:98`. | partly | Add a DC blocker on the FM input. |
| 3 | **Constant Wave adds the frequency to FORMANT.** The block diagram takes this from the "Frequency Calculator", after FM is added; text: "In constant wave mode, MANGROVE will behave rather traditionally as an FM carrier." | Impulse length follows knob + FINE + V/oct only; FM is left out. `GroveEngine.hpp:111-112`. Under deep FM the waveform will not stay constant. | partly | Use the FM'd frequency for the impulse length in Constant Wave. |
| 4 | **SYNC.** MG map: "accepts triggers or oscillators to soft-reset the oscillator core" (linking to reversing sync), AC-coupled. PPM manual: "SYNC resets the oscillation cycle for hard-sync." | Hard reset of the core to the start of its cycle on a rising edge above 1 V. `GroveEngine.hpp:91-94`. | spec unclear (the two official sources disagree) | Leave, or offer soft (direction-reversing) sync as a menu option. |
| 5 | **Impulse:** each rising edge of the core fires an envelope that "rises to +5v, then falls to -5v where it rests". | Same. Longer than one core cycle = skipped cycles = undertones, as the map describes. | matches | - |
| 6 | **BARREL:** anticlockwise = 100% rise; noon = triangle, "harmonic content is at a minimum"; clockwise = all fall. ±5 V CV = full sweep. (The two manuals swap the words "ramp" and "saw", and one sentence in the map says "minimum (CW)" by mistake, but both agree anticlockwise is the pitch-division end.) | Same directions and CV range. `GroveEngine.hpp:55`, `Mangrave.cpp:152`. | matches | - |
| 7 | **FORMANT:** "As FORMANT increases, the duration of each impulse decreases." CV: "+/-5V equating to a 50% sweep of the knob". | Same, including the half-range CV. `GroveEngine.hpp:51-53`, `Mangrave.cpp:151`. | matches | - |
| 8 | **Constant Wave / Constant Formant:** wave = length is a fixed share of the period; formant = length fixed whatever the pitch. | Same (tested in `grove_test.cpp`). | matches | - |
| 9 | **AIR:** closed at minimum; "reaching a 10V peak-to-peak signal at noon"; then overdrive "squaring off at the maximum". CV through an attenuverter. | Silent at 0, exactly ±5 V at noon, squared off at full (peaks about 5.65 V). CV through a trimmer. `GroveEngine.hpp:57-61, 155-157`. | matches | - |
| 10 | **FM INDEX:** "expects 0-5V... normalled to +5V, so full modulation... when nothing is patched". | Same; plus a depth knob (default full). The MG map only describes a CV input; I could not confirm whether the hardware has an index knob. `Mangrave.cpp:148-150`. | matches (+ knob extra-by-design) | - |
| 11 | **FM type:** MG map "linear"; PPM manual "linear through-zero". | Linear, through zero. 5 V at full index = 100% frequency swing; the specs give no depth figure. | matches / spec unclear (depth) | - |
| 12 | **PITCH, FINE, V/oct:** V/oct tracks, FINE "over a one-octave range". | 1 V/oct; FINE ±6 semitones; knob 8 Hz to 8.4 kHz, middle C at noon (range not published). | matches / spec unclear (range) | - |
| 13 | SQUARE comes from the core. Level not published. | ±5 V square. | matches / spec unclear (level) | - |

### PPM (relying on second-hand reading of the PPM manual)

| # | What the spec says | What the code does | Severity | Suggested fix |
|---|---|---|---|---|
| 14 | **IMPULSE mode:** classic; SINE is normalled to TRIGGER. "If a signal is connected to TRIGGER, the SINE from CORE is overriden." | Core fires the impulse unless TRIGGER is patched, then TRIGGER does. `GroveEngine.hpp:121-124`. | matches | - |
| 15 | **CYCLE mode:** the impulse loops as its own oscillator. "FORMANT controls the pitch of the IMPULSE oscillator", BARREL its shape; Constant Wave ties it to the core pitch, Constant Formant frees it. "TRIGGER can be used to soft-sync that oscillator, only while the waveform is falling." | Same on every point. `GroveEngine.hpp:128-132`. | matches | - |
| 16 | **SYNC mode:** "identical to cycle except that the SINE to TRIGGER" normal stays connected; clockwise FORMANT then adds overtones. | Impulse loops and the core soft-syncs it (only while falling). `GroveEngine.hpp:125-127`. | matches | - |
| 17 | **TO AIR:** "Injecting an additional signal into the AIR circuit along with the IMPULSE oscillator's output." | Added to the impulse before the AIR VCA and overdrive. `GroveEngine.hpp:155`. | matches | - |
| 18 | **SINE, SAW:** "moulded from the underlying triangle core oscillation." | Both derived from the core, ±5 V. | matches / spec unclear (levels) | - |
| 19 | **PWM:** "BARREL sweeps the pulses of the PWM output." | High while the impulse is rising, so BARREL sets the width. `GroveEngine.hpp:158`. | matches (as far as the manual goes) | - |
| 20 | **HOLLOW:** "introduces divided pulse waveforms", described to me as a mix of PWM and sub-octave pulse shapes that follows FORMANT. | A plain square that flips at each impulse start (half the impulse rate). No PWM component. `GroveEngine.hpp:79-83, 159`. The guide already calls this an interpretation. | spec unclear / partly | Check against the real manual or a scope shot before changing. |
| 21 | FIRE button, displays, CV trimmers, polyphony | Extras. The FIRE button counts as "TRIGGER patched" while held, so holding it stops the core firing. | extra-by-design | - |

### Guide claims about Mangrave

| Guide says | Against the spec |
|---|---|
| Core, impulse, FORMANT, BARREL, AIR, CONSTANT descriptions. | Correct. |
| "AIR... nearly a sine around 9 o'clock". | The map says this too ("approaching a 'sine' shape around 9:00"). Faithful to the text. |
| "SYNC in: Resets the core." | Agrees with the PPM manual; the Technical Map says soft reset (item 4). |
| "On the hardware, FM INDEX is a CV input only; here there is also a knob." | The Technical Map describes only the CV input. I could not confirm the hardware panel; treat as unverified. |
| "Mangrave's core and impulse follow the Mangrove technical map closely." | True except for retrigger-while-falling (item 1), FM AC-coupling (2) and FM in Constant Wave (3). The multiphonics gap is worth saying in the guide. |
| PPM section and the five PPM patch ideas. | Consistent with what I could read of the PPM manual (patch names Quadricycle, Get Complex, Detune Party, Over/Under appear there). |

### Bugs found in Mangrave

- **Under through-zero FM the core does not fire impulses while running backwards** (`GroveEngine.hpp:101-103`: only a forward wrap counts as a new cycle). During deep FM the impulse train drops out for the reversed part of each modulation cycle. Probably audible as thinner FM than expected; worth a listen.
- **Poly channel count ignores FORMANT, BARREL, AIR, FM INDEX and TO AIR inputs** (`Mangrave.cpp:137`). Minor.
- FORMANT knob + CV is not limited in Constant Wave, so strong negative CV can ask for impulses hundreds of cycles long (very deep undertones). Harmless, but the knob range and CV range are then inconsistent with Constant Formant, which is limited.
- Fuzz test passes; no crashes or runaway values found.

---

## Top fixes, in priority order

1. **Just Fiends: reverse the trigger normalling** (6N cascades down to IDENTITY) and correct the tooltips and guide. Small change, affects every patch.
2. **Just Fiends: ignore triggers for the whole slope in Transient mode**, so clock division and subharmonics work as on the hardware.
3. **Weird Sisters: rebuild crossover CENTRE** as high pass at the LOW cutoff into low pass at the HIGH cutoff, let SPAN cross below about 10 o'clock, and add ALL IN to each input instead of replacing.
4. **Just Fiends: make the FM knob match** (linear; clockwise = TIME, anticlockwise = INTONE-style), divide by channel number in the Shape MIX, and make Sustain/Sound a gate-following trapezoid.
5. **Just Fiends RUN modes: fix Plume first** (hold while gated, reverse the RUN direction), then Strata (real ARSR, release on gate low), then the Shift and Volley voltage maps. Alongside: Weird Sisters anti-resonance as a true dry/wet, and Mangrave retrigger-while-falling in IMPULSE mode for multiphonics.

Also worth doing at once because they are quick: Just Fiends bug A (velocity read at the threshold, default it off) and bug B (voltage stuck after leaving Strata).
