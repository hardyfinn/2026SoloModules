# Jazz piano comping module: research and proposal

Designed on Fable, 2026-11-23. Notes out only (your piano module makes the sound). Status: proposal with working prototype and 11 audio sketches. Nothing is built.

## What the problem actually is

A chord-machine plays the chord symbol. A pianist plays four separate decisions at once, and it is the *interaction* of the four that sounds human:

1. **Which notes** — jazz piano comping almost never plays the root. The left hand sits in a window roughly F3 to E5 and plays the 3rd, 7th and the colours; the bass has the root. Which subset, and in what order, is the voicing style.
2. **Where the notes go next** — a real player moves each voice as little as possible from the last chord. That one rule is the single biggest difference between "chords" and "piano". Measured on the prototype: with voice leading off, each voice jumps an average of 4.3 semitones at a chord change; with it on, 1.0. The top note moves 1.2 semitones instead of 4.6. That is audible immediately (sketch 03).
3. **When** — comping rhythm is not "on the beat". It is short cells of two or three hits, mostly on upbeats, repeated and varied over two bars, with hits that *anticipate* the next chord by landing on the upbeat before it. Charleston figures, dotted-quarter pushes, holds across the bar.
4. **How** — the chord is not struck simultaneously. Voices spread over roughly 5 to 20 ms (prototype mean 10.6 ms), usually bottom to top, occasionally top first. The top voice is a few velocity units louder than the inner ones. Everything sits a few milliseconds *behind* the beat, and phrases drift in loudness.

Points 3 and 4 are where your AGE/slop instinct from Doubles and Sawstack applies directly, and they are what nearly every existing chord module leaves out.

## What I built to check it

`comp_engine.py` is a complete working comping engine (chord parser, five voicing libraries, voice-leading search, rhythm cells with swing, feel/humanising, soloist listening, substitutions). It writes MIDI; the sketches are that MIDI through a general-MIDI piano, with a walking bass, drums and a made-up horn line as context so you can hear the comping in its place. The piano is deliberately a plain GM piano, not your module.

Everything in `sketches/`. `midi/` has the same files as MIDI, including piano-only versions, so you can drop them into your own piano. `sketches/piano-roll.png` shows three of them as piano rolls.

| Sketch | What it demonstrates |
|---|---|
| 01 voicing types | The same 8 bars four times: shell, rootless, quartal, spread |
| 02 feel | The same blues three times: feel off (machine), player, too loose |
| 03 voice leading | Minor ii-V-I, voice leading off then on. The core of the whole design |
| 04 density | Same blues at sparse, medium, busy |
| 05 inside then outside | Plain, then with tritone substitutions, altered dominants and chromatic side-slips |
| 06 listening | With a soloist: ignoring them, then listening (hits drop from 35 to 19 per 16 bars, and hits landing on top of the soloist drop from 62% to 25%) |
| 07 bossa | Straight eighths, bossa cells |
| 08 ballad | Solo piano, no band, roots added, spread voicings, slow |
| 09 modal fast | Quartal voicings, 184 bpm, busy |
| 10 soul | Sixteenth-note cells at 94 bpm |
| 11 house stabs | Offbeat stabs at 122 bpm, tight repeat |

I verified these numerically (voice movement, hit counts, pitch and velocity ranges, roll spread, overlap with the soloist) and by reading the piano rolls. I have not heard them. The style cells for bossa, soul and house are the parts I trust least; the swing cells and the voice leading I trust most.

## VCV Rack or Max for Live

Max for Live is a reasonable option, and for *this* module it is arguably the better fit: Live hands it the chord progression as clips, the bar/beat position and a key signature, and the output is MIDI to any instrument. In VCV none of that exists, so the module has to own its own chord chart and clock.

I recommend **VCV Rack** anyway, for three reasons: your piano module is in VCV, so notes-out means polyphonic V/oct and gates to a module already next to it; the "listen to the soloist" behaviour needs a live gate/pitch input, which is natural in VCV and awkward in Live; and this project's whole toolchain, manual and release process is set up for VCV. A Max for Live version would be a separate build later, not a fork — the engine is the same code.

## Proposed module

Working name **Comper Fable**. One module, about 30 HP, plus a chord-chart section. Polyphonic V/oct and gate out.

**Chord input.** Three ways, all present:
- **Chart** — a built-in 16-step chord chart, two knobs per step (root, quality) with a text display showing "Dm7", "G7alt". This is how it works standalone.
- **CV** — root V/oct in plus a quality CV in, so a sequencer drives it.
- **Poly in** — play or sequence a chord into it and it works out what the chord is, then comps on it. This is the one I expect you to use most with a sequencer.

**Voicing.** STYLE (shell, rootless, quartal, spread, mixed), SPREAD (how wide), REGISTER (where the top note sits), VOICES (2 to 5), LEAD (voice-leading strength, 0 to full), OUT (0 to full: altered dominants, tritone subs, upper-structure triads, chromatic side-slips), ROOTS (off, sometimes, always — for solo piano).

**Rhythm.** FEEL (swing, bossa, ballad, soul, house), DENSITY, SWING (manual, defaulting to a tempo-dependent curve: triplet-ish at medium tempo, flattening as it gets faster, which is what real players do), REPEAT (how strongly a two-bar cell is kept versus reinvented), PUSH (how much it anticipates the next chord), SEED with a NEW button.

**Feel (one knob plus trims).** AGE-style single knob scaling all of: timing behind the beat, roll spread, roll direction variety, velocity scatter, phrase loudness drift, note-length variation, dropped inner voices on short stabs. 0 is a drum machine, 1 is a good player, 2+ is a drunk one. Trims behind it for TIMING, ROLL, WEIGHT, SLOP, matching Doubles.

**Listening.** SOLO GATE and SOLO PITCH inputs, and a LISTEN knob. With it up, the module shuts up while a line is playing, answers in the gaps, and drops register away from the soloist's pitch. This is the feature that makes it feel like an accompanist rather than a backing track.

**Outputs.** Poly V/oct and poly gate (per-voice, so your piano gets real voice allocation and the roll timing survives), plus VELOCITY poly CV, a BASS root out (mono V/oct + gate) so a bass module can follow the same chart, and a CHORD CHANGE trigger.

**Presets.** 12: medium swing, fast bebop, ballad, bossa, modal quartal, Bill Evans-ish spread, blues shell, soul sixteenths, house stabs, out-and-sparse, solo piano with roots, duo with listening.

## Open questions

1. **VCV or Max for Live?** I recommend VCV, reasons above.
2. **One module or two?** I recommend one module plus a small expander later for a longer chart (64 steps) and per-step voicing overrides. Alternative: build the chart as a separate module from the start.
3. **Does it need the "work out the chord from poly in" mode in version 1?** I recommend yes; it is the most useful input in a patch and not much work.
4. **A second hand?** A right-hand melodic fill engine (answering phrases in the gaps, out of the same chord) could be an expander. I recommend leaving it out of version 1 so the comping is right first.
5. **Repo and name.** I recommend `2026SoloModules`, folder `ComperFable`, branch `claude/project-thread-7vm3pn`, name "Comper Fable", short name COMP in Rack. Say if you want a different name.

Next step on your word: the build spec for Opus.
