# X|FORMER sequencers for Ableton Live

Seven Max for Live MIDI effects. Each one runs one sequencer engine from the X|FORMER firmware and plays it as MIDI notes. The gate length is the note length, and the clock is Live's.

| Device | Firmware engine | What it is good for |
|---|---|---|
| **XF Algo** | Algo (Tuesday) | 16 generative algorithms (TriTrance, Stomper, Markov, Aphex, Autechre, Turing...). You steer them with Flow, Ornament and Power. |
| **XF Stochastic** | Stochastic | A rhythm and melody generator. It plays new material live, or captures a phrase and loops and mutates it. |
| **XF Note** | Note track (Metropolix style) | A 64-step sequencer with pulse counts, ratchets, conditions and the accumulator. |
| **XF Indexed** | Indexed | Up to 48 steps, each with its own length. Rhythms that drift against the bar, plus two modulation routes (Mod A/B). |
| **XF Discrete** | Discrete Map | A sweeping voltage crosses up to 32 thresholds; each crossing plays that stage's note. Great for arpeggios and "scanned" melodies. |
| **XF PhaseFlux** | PhaseFlux | 16 stages on a 4x4 grid. Each stage fires a burst of pulses shaped by a timing curve and a pitch curve. |
| **XF Fractal** | Fractal | A looper that records the MIDI you play into it, then replays it with branches (transposed, reversed, inverted copies) and ornaments. |

For a table of every control on every device, and what each one does to the notes, open **XFORMER-Guide.html** (same folder).

Requirements: Live 12 Suite running Max 9. The engines use the `[v8]` JavaScript object, which arrived in Max 9.

---

## 1. Installing

1. Keep all the files from `devices/` together in one folder. Each `.amxd` loads its `.js` files from the folder it sits in. A good place is `User Library/Presets/MIDI Effects/Max MIDI Effect/XFORMER/`.
2. Drag a device onto a MIDI track, in front of an instrument.
3. **Freeze each device once (recommended).**
   - Click the device's *Edit* button to open it in Max.
   - Click the snowflake (Freeze) button at the bottom of the patcher window, then save.
   - Freezing copies the `.js` files into the `.amxd`. You can then move the device, collect the Live set, or send it to someone else, and it keeps working.
4. Press Play. Every device plays a pattern straight away (XF Fractal starts with a short demo phrase until you record your own).

If a device stays silent, open Max's console (the Max window) and look for lines that start with `xformer:` or `v8:`. Please send me any you see.

---

## 2. Quick start and starting values

### XF Algo
- **Algorithm** TriTrance, **Flow 4**, **Ornament 4**, **Power 12**, **Loop Inf**, **Rate 1/16**, **Scale Major**, **Root C**, **Gate 50 %**.
- Start by turning **Flow** and **Ornament**. They are the two seeds, and each value gives a different but repeatable pattern.
- **Power** sets how many steps play (density).
- Set **Loop** to 16 to lock a phrase you like. **Inf** keeps generating.
- **Reseed** rolls new dice while keeping the knob settings. A song restart or Reset returns to the knob-defined pattern.
- Good first algorithms:
  - **Stomper**: acid bass.
  - **Markov**: melodic, wandering lines.
  - **Aphex**: broken, glitchy lines.
  - **Autechre**: pattern transformations.
  - **Turing**: Music Thing Turing Machine.

### XF Stochastic
- **Rhythm Live**, **Melody Live**, **Complexity 50 %**, **Range 50 %**, **Variation 16 %**, **Length 1x**, **Rate 1/16**, **Scale Major**.
- **To capture a phrase:** while it plays something you like, press **New Rhythm** and/or **New Melody**. The last bars are captured and the mode switches to *Loop*.
- **In Loop mode:** **New Rhythm** and **New Melody** roll a new loop. **Undo** (in the editor) goes back one.
- **Mutate**, **Patience** and **Jump** (in the editor) let a loop slowly drift.
- **Evolve Off** (default): every time you press Play, the stored loop starts fresh, so arrangements are repeatable. **Evolve On**: the drift carries on between plays.

### XF Note
- **First 1**, **Last 16**, **Run Forward**, **Rate 1/16**. A new device starts with an 8-note line on every other step.
- Open the **Editor**, pick a layer (Gate, Note, Length, Ratchet, Pulses, Condition, Accum...) and click or drag on the 16 columns. The page buttons show steps 1–16, 17–32 and so on. Shift-click a page button to copy the current page there.
- **Pulses** holds a step for 1–8 clock pulses. **Gate mode** decides which of those pulses fire: All, First, Hold, or First+Last. Together these are the Metropolix trick.
- **Accum** on a step adds the accumulator to that step's pitch. Every time the step triggers, the accumulator moves by *Acc Step*, between *Acc Min* and *Acc Max*.
- **Fill** turns on steps whose condition is *Fill*, and turns off steps whose condition is *!Fill*. Map it to a pad.

### XF Indexed
- **Steps 8**, **Run Forward**, **Play Loop**, **Stretch 1**. A new device has 8 steps over one bar in a long-short rhythm.
- In the **Editor**, the **Duration** layer sets each step's length. Dragging snaps to 1/64 notes; hold Shift when you click for single ticks. 1/4 = 192 ticks, and the labels show note values.
- Durations do not have to add up to a bar. Make the total 5/16 or 7/16 and the pattern drifts against the beat.
- **Gate** is the note length inside the step (it can't be longer than the step). **Groups** tags steps A-D for the routes.
- **Stretch** multiplies every duration (the module's Divisor). 2 = everything half speed.
- **Mod A / Mod B** are two dials standing in for the module's CV inputs. In the editor, *Route A from* picks a dial, *Route A to* picks Duration, Gate or Note, *Route A steps* picks which groups it affects, and *A Amount* scales it. Put a Live LFO on Mod A for steps that breathe.
- **Reset Bars** = the module's Sync: Reset Measure.

### XF Discrete
- **Source Triangle**, **Sweep 1 bar**, **Spacing Position**, **Gate 0 %** (0 % = a short trigger). A new device has 8 rising stages with a little melody.
- How it works: a ramp (Saw or Triangle) sweeps from -5 V to +5 V over the **Sweep** time. Every stage has a **threshold** and a **trigger** (Rise, Fall, Both or Off). When the ramp crosses a threshold in that direction, the stage's **note** plays.
- **Above / Below** squeeze or stretch the window the thresholds live in. **Spacing Length** spreads the active stages evenly instead of using the threshold values.
- **Source Input** uses the **Input** dial instead of the ramp. Map a Live LFO or envelope to it and the melody follows the modulation.
- **Scanner** walks over the 32 stages; each stage it enters flips its trigger to the next type. Automate it slowly for patterns that rewrite themselves.
- Editor layers: **Trigger**, **Note**, **Threshold**. *Sweep timing*: **Free** (the module's default) counts from the last reset; **Bar-aligned** also restarts the sweep every Reset Bars.

### XF PhaseFlux
- **Path Snake**, **Rate 1/16**, **Pitch Mode Per stage**. A new device has four stages: a climb, a fall, a bell-shaped bloom and a bounce.
- Each stage lasts **Length** x Rate (4 = one beat at 1/16) and fires **Pulses** (0-16) inside that time.
- **Time curve** places the pulses: Even, Bell (bunched in the middle) or Bounce. **Time warp** pushes them towards the start or end.
- **Pitch curve** (Ramp, Bell, Triangle, Bounce) with **Pitch dir** (Up, Down, Bipolar) and **Pitch range** (1/2 to 3 octaves) sets each pulse's note above or below **Base pitch**.
- **Mask** thins the pulses (1 in 2, 2 in 3...), **Repeat** and **Window** fold the curves, **Melody mask / tilt** keep only tonal (or only off-scale-centre) notes.
- **Note acc** / **Pulse acc** on a stage make the pitch or the pulse count drift every time the stage (or every pulse) plays. Their limits and order are in the editor.
- The main dials are global nudges: **Pulses +-**, **Length +-**, **Warp**, **Response** and **Cycle Warp** act on all stages at once. They are good for automation.
- **Path** changes the walk across the 4x4 grid (rows, columns, spiral, diagonals...). The device shows the grid with each stage's pulse count.
- **Pitch Mode Global** runs one pitch curve across all stages, at the **Pitch Rate** ratio, so melodies drift slowly against the rhythm.

### XF Fractal
- **Rate 1/16**, **Capture Event**, **Timing Feel**, **Loop 1-16**. A new device plays a 16-cell demo phrase.
- **To record:** route MIDI into the track (play a keyboard with the track armed or monitoring In, or set *MIDI From* to another track), press **Record**, and play.
  - *Capture Section* writes one cell every Rate step: the pitch, how long the key was held, and (with *Timing Feel*) where in the step you played it.
  - *Capture Event* writes a cell each time the pitch changes, so you get a compact note list whatever your timing.
  - *Record mode Latch* only writes steps where you played something (overdub). *Replace* also writes rests.
  - **Lock** freezes the recording and the ornament choice. **Clear** erases the trunk.
- **Branches** (0-7) add transformed copies after the trunk: transpose, reverse, inverse, retrograde inverse, rotate, compress, expand, gate thin. **Seed** picks which transforms, **Pool** (editor) limits the kinds allowed, **Path** sets which branches play going out and which coming back.
- **Ornaments** is the chance a note gets a flourish; **Orn Depth** unlocks bigger ones (runs, arpeggios and mordents from 40 %, the trill from 75 %). *Orn First/Last* limit them to part of the loop.
- **Order** reads the loop Forward, Reverse, Pendulum, Random, Converge, Diverge or in Pages. **Delay** plays everything N steps late (a canon with itself when you layer two devices).
- The editor shows the trunk (8 pages of 16 cells) and lets you edit pitch, gate length and onset by hand. The red mark is where the next cell will be recorded.
- *MIDI Thru* (editor) lets you hear what you play. *Main note To scale* snaps the recorded pitches to the device scale.
- The recording is saved with the Live set, like the patterns of the other devices.

### All devices: velocity, slides and resets
- **Velocity**: Algo makes its own velocities (accents). Switch *Vel Source* to *Fixed* to use the Velocity knob. Stochastic and Note always use the Velocity knob (default 100).
- **Slides: Legato** (default) turns a slide into overlapping notes, so a mono synth with glide or legato mode slides. **Gap** keeps every note separate.
- **Reset button**: restarts the pattern on the nearest 1/16 note. It is a Live parameter, so you can MIDI-map or key-map it, or automate it.
- **Reset by Note**:
  - *Off*: incoming notes do nothing (the default, so playing a keyboard into the track doesn't reset anything).
  - *Any*: any incoming note resets.
  - *Pitch*: only the chosen note resets. Use a clip on the same track with a note (C1 by default), or route MIDI from another track.
- **Play From**:
  - *Song* (default): the pattern is locked to the song position. Starting playback at bar 9 plays exactly what you would hear at bar 9 when playing from bar 1, and loop braces stay in sync.
  - *Here*: the pattern starts wherever you press Play.
- **Reset Bars**: restarts the pattern every N bars (0 = off).
  - Algo does a hard restart.
  - Stochastic, Note, Indexed, Discrete, PhaseFlux and Fractal use the module's own *Reset Measure*. The position goes back to the first step, but dice and accumulators keep rolling.

---

## 3. How it works (signal flow)

```
Live transport
  │
[metro 5 ticks @quantize 5 ticks]      fires every 1/96 note, only while Live plays
  │
[transport] ── ticks ──> [expr int($f1/5.+0.5)]  = slot number (1/96 note)
  │                              │
  │                         [t i i i]
  │              ┌───────────────┼──────────────────────┐
  │        1. play slot     2. remove slot          3. "pos <slot>"
  │              │               │                       │
  │           [coll] <───────────┘                    [v8 xf_xxx.js]   engine + timing host
  │              │                                       │  │  │  │  │
  │         [zl iter 2]          ┌── notes for later ────┘  │  │  │  └─> [pattr xfstate] (pattern saved in the Live set)
  │              │               │                          │  │  └──> [s ---xfd] -> both [jsui] views
  │        [midiformat] <────────┼──── notes due right now ─┘  └─> bang [midiflush] on stop
  │              │               │
  │        [midiflush] ─> [midiout] ─> instrument
  │
  ├─ tempo ──> [change 0.] ─> "tempo"   ─> v8
  └─ time sig > [zl change] ─> "timesig" ─> v8

[live.thisdevice] ─> "loaded" ─> v8 (reads every knob, restores the pattern)
                  ─> [live.path live_set] ─> [live.observer is_playing] ─> "playing 0/1" ─> v8
[midiin] ─> [midiparse] ─> "midinote" ─> v8 (Reset by Note; XF Fractal records these notes)
knobs ─> [prepend p <name>] ─> [s ---xfp] ─> v8
buttons ─> [route bang 1] ─> "act reset" etc. ─> [s ---xfp]
[jsui] strip click / Editor button ─> [pcontrol] ─> opens [p editor] (the big window)
```

**Why a lookahead and a `[coll]`?**
- JavaScript in Max runs on the low-priority thread, so anything it sends "right now" can arrive a few milliseconds late. Notes would then wobble.
- So the engine runs **1/3 of a beat ahead** of the playhead and writes each note into the `[coll]`, keyed by its exact 1/96-note slot.
- The high-priority clock reads the slot from the `[coll]` exactly on time. JavaScript only has to be early, never punctual.
- The cost: a knob change is heard about 1/3 of a beat later (about 170 ms at 120 BPM).

**Gate to MIDI.** The firmware thinks in "gate high/low plus pitch". The host turns that into note-on/off pairs:
- a gate going high starts a note;
- a pitch change while the gate stays high starts the new note and ends the old one 2 ticks later (overlap = legato);
- a sliding step glues onto the previous note when *Slides = Legato*;
- [midiflush] ends any note still sounding when Live stops.

**Song-position lock.** When you start or jump, the device resets the engine at the start of the song (or at the last Reset Bars line) and fast-forwards it silently to the playhead. That is why the same bar always plays the same notes.

**Saving.** Every device except Algo keeps its pattern (steps, stages or the recorded Fractal trunk) in a hidden `[pattr]` parameter, which Live saves with the set, in presets, and with undo. All knobs are normal Live parameters, so they can be automated, mapped or used in Macros.

---

## 4. Why code here, and not patch cords

The project rule is to patch where possible. The patching (clock, MIDI, UI, saving, editor window) is all real Max objects. The **engines** are code, because:

1. **They must match the module note for note.**
   - The firmware uses specific random number generators, 32-bit integer and float maths, sort orders and queue rules.
   - In JavaScript I could test every engine against the firmware itself, running in its simulator: 200+ random scenarios each, all identical.
   - That kind of check is impossible with a patch.
2. **Size.** The Algo engine alone is around 1,200 lines of logic with 16 algorithms. As patch cords it would be thousands of objects nobody could read.
3. **Fast-forward.** Song-position lock needs the engine to run thousands of steps in a few milliseconds when you jump in the timeline. Only code can do that.

The engine files are commented in plain language. The source and the test tools are in `source/`.

---

## 5. Differences from the module (on purpose or unavoidable)

- **Microtonal scales are left out.** MIDI notes can't play them. The 14 twelve-tone scales are all there, and *Mode* rotates them (Major + Mode 1 = Dorian, and so on).
- **Note device: Aligned play mode only.** The module's default *Free* mode ignores Pulse Count, which is the point of the Metropolix-style track.
- **Bar-line note cut.** At every bar line the module drops pending note-offs on Stochastic and Note, which can hold a note across the bar. The devices don't copy that, so no note hangs.
- **Stochastic** starts with a freshly rolled pattern instead of the module's empty one. With Evolve off, every play starts from the stored pattern.
- **Algo ratchets and trills** are timed in milliseconds, as on the module. At extreme tempos they sit slightly differently against the grid, exactly like the hardware.
- **CV-only features** (slide time, CV update modes) have no MIDI meaning and are left out.
- **Swing** is not included (as you asked).
- **Indexed, Discrete and PhaseFlux timing is exact.** The module times Indexed and Free-mode Discrete from a clock estimate that wobbles by a tick now and then; the devices use Live's exact position.
- **Indexed, Once mode:** when the run ends, the module leaves the last gate high until the next reset (a hung note). The device ends the note normally.
- **Discrete:** Pluck, Slew and Offset are CV effects and are left out. Input and Scanner are dials (map Live LFOs to them) instead of CV inputs.
- **PhaseFlux:** changing Path or Cycle takes effect straight away (the module waits for another layout change). Slide time and the CV "Always" mode are left out.
- **Fractal:** the source is the MIDI coming into the device instead of another module track, so the two-source gate/CV logic is not needed. The recording survives Stop and Play (the module clears it on reset); use Clear to erase it. Recorded pitch is rounded to the nearest semitone for MIDI. Capture runs at the playhead, playback 1/3 beat ahead, so a cell recorded in a very short loop is heard one pass later.
- **About 170 ms knob latency** at 120 BPM (see the lookahead above). Automation and mapped knobs are affected the same way.

---

## 6. Variations and creative options

1. **Polyrhythms.** Put two copies of XF Note on two tracks, one with Last = 16 and one with Last = 13 or 15, both at 1/16. They drift and realign every few bars.
2. **One brain, two voices.** Put XF Algo on a bass track and an Instrument Rack with two chains, one transposed +12 or +19, with different filters. Ratchets and trills stack nicely.
3. **Song-section resets.** Put a clip with C1 notes on the same track (Reset by Note = Pitch). Each clip launch restarts the pattern exactly at the downbeat of a new section.
4. **Captured jams.** Run Stochastic in Live mode with Complexity around 70 %, press New Rhythm when a groove appears, then raise Mutate to 10–20 % and Patience to about 60 %. The loop slowly rewrites itself.
5. **Accumulator arps.** In XF Note, set four gated steps with Note = 0, put Accum = +1 on step 1, and set Acc Max to 7 and Acc Order to Pendulum. One short pattern climbs and falls through the scale.
6. **Record the output.** Set another MIDI track's input to this track (Post FX) and record. You get regular MIDI clips to edit by hand.
7. **Indexed polymeters.** Two XF Indexed on two tracks, one with 5 steps of 1/8. and one with 7 steps of 1/16: they line up again only every few bars.
8. **Discrete as an LFO-to-melody converter.** Source Input, then map Live's LFO (or an Envelope Follower on a drum bus) to the Input dial. The groove of the drums plays the melody.
9. **PhaseFlux bursts on drums.** Put it before a Drum Rack, Scale Chromatic, Base pitch 36, Pitch range 1/2 octave: the pitch curve now picks drum pads, and the time curve makes fills.
10. **Fractal canon.** Record a phrase into one XF Fractal, copy the device to a second track with Delay 4 and Branches 3. It answers the first track with transformed copies.

Easy patch-level additions if you want them (tell me and I'll add them):
- a MIDI Thru switch;
- a MIDI channel selector;
- a "transpose by incoming note" mode, where the played key sets the Root;
- an output for the accumulator as a Live modulation.

---

## 7. Troubleshooting

- **No sound:**
  - Check that the device sits before an instrument and that Live is playing (the devices only run with the transport).
  - Check the Max window for errors.
  - If the device was moved without freezing, the `.js` files may be missing. Freeze it, or keep the files next to the `.amxd`.
- **Notes sound late on knob moves:** this is the 1/3-beat lookahead (section 3).
- **Pattern changes when you start mid-song with Play From = Here:** that is expected. Use *Song* for repeatable arrangements.
- **Stochastic loop lost after reopening:** make sure you saved the Live set after pressing New Rhythm or New Melody. The pattern lives in the set, like any parameter.
