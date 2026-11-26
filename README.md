# 2026 Solo Modules

Haer's home for new VCV Rack modules. Each folder is its own plugin (its own
`plugin.json`, version and build), so modules can be added and updated
without stepping on each other.

| Folder | Module | What it is |
|---|---|---|
| [Paddock](Paddock/) | Swerve, Gallop, Tumble | Blasser-style coupled circuits: a bounce oscillator, four bounce oscillators bounding each other, five touch-coupled rhythm cores with gongs |
| [Riddim](Riddim/) | Riddim | Dancehall riddim drum machine: 24 classic riddims, each with its machine flavour, ten synthesised voices |
| [CymbolismFable](CymbolismFable/) | Cymbolism, Cymbolism Extended | Physical cymbal: ten types computed from their geometry, nonlinear crash bloom, stick, mallet, rods, brush and bow; Extended adds morph, warp, tuning to scales, sustain, freeze, stereo and more |
| [Heathrow](Heathrow/) | Heathrow | Stereo phaser / flanger with through-zero, a second switchable engine and 6 memory slots |
| [Joanna](Joanna/) | Joanna | Physical model of an upright home piano and its room: felt hammers, beating unisons, years since tuning, wear, age, three pedals, lid, wall and room |
| [Datamatic](Datamatic/) | Datamatic, Test Pattern | Ryoji Ikeda style: numbers (pi, primes, text, CV) become interlocking gates at up to 16x the clock, played by hard-edged sine, click and noise voices |
| [Krka](Krka/) | Krka | Four function generators with 260 shapes, 16 chain algorithms, assignable CV and 18 presets (Zadar-style, no menus) |
| [Lindenmayer](Lindenmayer/) | Sprout, Graft, Bloom | L-system sketches: a rhythm that grows, drum rules that rewrite themselves, chords grown by rules |
| [Surprise](Surprise/) | Surprise | Novelty follower: CV and trigger when the audio does something new, plus a BORED (loop-fatigue) output |
| [Pond](Pond/) | Pond | Frog and toad chorus: 16 species, call and answer, HUSH (predator) and a tentative return |
| [Hangover](Hangover/) | Hangover | Handpan (hang drum) by modal synthesis with the real instrument's dirt: tuning drift, beating, rattles, bleed, breath, skin, slop, room |
| [Panorama](Panorama/) | Panorama | Steel pan (tenor, double second, triple cello) with sympathetic neighbours and imperfect, editable tuning |
| [Gongoozler](Gongoozler/) | Gongoozler, Gonzo | Gong and tam-tam physical model (tam-tam, wind, opera, bossed; hard hits bloom), plus Gonzo, the same engine pushed past real gongs |
| [Bore](Bore/) | Scanner, Bore Room | MRI scanner from the inside: real pulse sequences through three coils, pump and bore, plus the bore as a reverb |
| [Pivot](Pivot/) | Pivot | Metric modulation clock: a chain of OLD note = NEW note tempo pivots that follows Live's clock, glides, and lands back on Live's bar lines |
| [VinylizerFable](VinylizerFable/) | Vinylizer Fable | The sound of a 90s record from the physics of cutting, pressing and playback, in ten switchable processes (no crackle) |
| [Chirp](Chirp/) | Chirp | Pitch-swept chirp voice: kicks, zaps, bats, frogs, crickets, birds, looped cicadas; pitch contour and envelope outs |
| [Groover](Groover/) | Groover | Ableton grooves (.agr / .mid) on VCV drums: grooved clock, 8 trigger lanes, accent CV |
| [ClapfallFable](ClapfallFable/) | Clapfall | Multi-particle percussion: modelled claps, snaps, sticks and drum-machine pulses, from an 808 clap to applause |
| [Menagerie](Menagerie/) | Menagerie | Macro oscillator with 16 engines of our own (none from Plaits or Braids) behind SHAPE, TEXTURE and LIFE |
| [Rimbaud](Rimbaud/) | Rimbaud | Physical model of a snare drum: coupled heads, colliding snare wires, rimshot, cross-stick, buzz strokes, top/bottom mics |
| [Lilt](Lilt/) | Lilt | Relabi generator: four summed LFOs crossing four thresholds make triggers that drift around the pulse |
| [Peal](Peal/) | Peal, Bell Foundry | Change ringing sequencer (methods, calls, open handstroke) and a 12-bell modal voice to ring it |
| [CopyThat](CopyThat/) | Copy That | FM radio link that loses the signal (Roger That-style): mistune the receiver, let it lose track, cancel the clean part to keep only the damage |

Every push builds all plugins for Apple Silicon Macs (GitHub Actions). Branch
builds go to the **preview** release, `main` builds to **latest**.

Conventions: plugin versions stay 2.x (Rack 2 skips 3.x), control labels 3.2 mm,
jack labels 2.9 mm, Hær logo on the bottom rail (`res/haer_logo.py`).

## Licences

Each plugin folder carries its own `LICENSE.txt`. Our own modules are MIT, so anyone may port them to other formats, platforms or hardware as long as the copyright notice comes along. The exception is **Ectoplasm**, which contains the GPL-3 Ectocore firmware and must stay GPL-3.0-only.
