# Copy That: design v2 (after Fable review)

VCV Rack module inspired by the Fairfield Circuitry Roger That pedal ("simplified FM modulator/demodulator" mimicking FM radio signal loss). Plugin "CopyThat" in hardyfinn/2026SoloModules (folder CopyThat), 2.0.0, MIT, Hær logo, labels 3.2 mm / 2.9 mm, WET/DRY excluded from randomize. Behavioural model: no Fairfield schematic or code exists publicly, none is used.

## Review of v1: what changes and why
1. **Simulate in complex baseband, not with an audio-rate carrier.** A real carrier needs to sit far above audio, which v1 could only fake with a low carrier at 4x oversampling (aliasing, sample-rate dependent). Instead model the radio the way SDRs do: the station is a complex phasor whose instantaneous frequency is deviation x audio; the receiver sees it offset by its mistuning, through an IF filter, then an FM discriminator. Cheaper, sample-rate independent, and every artifact comes from real FM behaviour.
2. **Let the artifacts emerge, don't paste them on.** v1 added crackle/pops at random. Real FM makes them itself: when the IF filter is too narrow or the receiver is mistuned, the carrier-to-noise ratio falls past the FM threshold and the phase wraps, giving sharp "clicks" that thicken into rushing noise. Distortion comes from sidebands cut by the IF filter. Squeals come from heterodyning with a neighbouring carrier and from AFC hunting. Only the hum and RFI buzz are added sources.
3. **TUNE is the receiver dial.** Turning TUNE moves the receiver relative to the station: near one end it sits on the station (subtle), turning up drifts off it (warble, distortion, clicks, then noise). The mapping is steep and uneven like the pedal ("very non-linear and fine"), with a small dead-on zone and a coarse far end. RANGE 2 (extended) widens the dial so it reaches the **neighbouring station**, which carries a delayed (130 ms), slightly detuned ghost of your own input; the capture effect makes the receiver flip abruptly between the two.
4. **SHIFT is tracking accuracy:** one knob sets IF bandwidth and AFC (automatic frequency control). Low = wide IF, fast AFC that pulls the receiver back on station: accurate. High = narrow IF (it starts to behave like a filter, as the maker notes) and a slow, overshooting AFC that hunts: warble, squeal, and pumping.
5. **DRIVE drives the transmitter.** JFET-style asymmetric preamp, then broadcast-style pre-emphasis and clip, and more DRIVE also means more deviation, which over-fills the IF filter. Per the pedal it acts on wet and dry.
6. **Make cancellation actually work.** The wet path is calibrated to unity gain and inverted when clean, and the dry path is delayed to match the wet path's latency, so DRY up against WET really nulls the clean part and leaves the artifacts. PHASE switch 180°/0°.
7. **Add a SIGNAL (fade) input.** The reviewer found the pedal's CV did little. Signal strength is the most musical thing to modulate: patch an LFO or envelope into FADE for dropouts, multipath flutter and stutters. Normalled to full strength.
8. **SIGNAL out instead of only a LOCK gate.** A 0-10 V signal-strength CV (what the receiver's meter would show) plus the LOCK gate.
9. VOICE filter (300 Hz to 3 kHz) stays wet only, so with cancellation it gives the pedal's mid-scoop.

## DSP (per poly channel, internal rate 4x with halfband up/down)
- Input x -> DRIVE gain (0 to +30 dB) -> asymmetric tanh preamp -> 50 µs pre-emphasis -> soft clip at ±1 -> m(t).
- Station phasor: phase += 2π (Δf·m + offsetHz)/fs, Δf = 12 kHz · (0.5 + DRIVE). Amplitude A = FADE level.
- Neighbour (RANGE 2 only): same chain on input delayed 130 ms, carrier 60 kHz away, amplitude 0.7.
- Channel: + complex white noise (fixed floor) + interference (hum as weak AM on A; rare GSM-style 217 Hz burst buzz at extreme TUNE).
- Receiver: LO offset from TUNE (curve below) minus AFC correction; complex mix; IF filter = 2x cascaded 2-pole complex lowpass, cutoff from SHIFT (wide 60 kHz -> narrow 3 kHz, exponential); discriminator arg(z[n]·conj(z[n-1])) · fs/(2πΔf); AFC = leaky integrator of discriminator DC, speed and overshoot from SHIFT.
- 50 µs de-emphasis, DC block 10 Hz, 15 kHz lowpass (broadcast bandwidth), output gain so clean = unity.
- SIGNAL = smoothed IF carrier power vs noise; LOCK = SIGNAL above threshold with hysteresis.
- Out = WET·(±wet, VOICE optional) + DRY·(delayed driven input). ±10 V soft limit.

## TUNE curve
TUNE 0..1 plus CV (±5 V = full range, attenuverter). Detune Hz = sign·k·(t^3) style curve: RANGE 1 spans 0 to 40 kHz off station (steep near zero), RANGE 2 spans -20 to 90 kHz so it passes the station and reaches the neighbour at 60 kHz.

## Panel (14 HP)
TUNE (large) + T CV attenuverter; SHIFT, DRIVE; WET, DRY; switches RANGE 1/2, VOICE off/on, PHASE 180°/0°.
Jacks (8): TUNE, SHIFT, DRIVE, FADE CV in; IN; OUT, SIGNAL, LOCK out. Polyphonic up to 16.
Mockup: copythat-panel-mockup.png (v2).

## Variations to try once built
- Slow triangle into FADE for a passing-car dropout; S&H into TUNE for channel surfing.
- RANGE 2 with TUNE near the neighbour, play a drum loop: stuttering flips between beat and its 130 ms ghost.
- DRY matched to WET, VOICE on: only the radio's damage, mid-scooped.
