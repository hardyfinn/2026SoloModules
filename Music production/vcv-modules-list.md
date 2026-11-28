# Hær VCV module catalogue

Last updated: 27 November 2026. **60 plugins, 110 modules.**

Built from the latest builds in `/mnt/project-files/vcv-builds/` (each build's own `plugin.json` and panel sizes), the plugin repos and our project notes.

**Tested in Rack column:**

- ✅ = Haer has tried it in Rack and it works
- ❌ = not tried in Rack yet
- ? = no record either way (mostly the early modules from before we kept notes; tell us and we'll tick them off)

All plugins are mac-arm64 builds and stay on version 2.x (Rack 2 skips 3.x plugins).

---

## Drums and percussion

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| 808 Metal | 808Metal 2.1.0 | TR-808 cymbal and hi-hat circuit model (six square oscillators, bandpass, swing VCAs) with tuning, filter, envelope and drive | 24 | ❌ |
| Alexa02 | Alexa02 2.3.0 | Six LXR drum voices on trigger jacks, two kits with morphing between them | 44 | ❌ |
| Alexa01 | Alexa02 2.3.0 | One LXR drum voice (drum, snare, cymbal or hat) with every setting on a knob and six memories to morph between | 42 | ❌ |
| Boohm | Boohm 2.0.0 | Stereo kick with four engines, pitch sweep, click, hold-to-sustain HIT and six drive FX (companion to the Ohmforce Bohm) | 26 | ❌ |
| Scanner | Bore 2.0.0 | MRI scanner from the inside: ten real pulse sequences (T1 knocks, T2 knock trains, EPI beeps, diffusion thumps…) through three tuned coils, cold-head pump, bore reverb and earplugs; whole-exam SESSION mode; gradient waveforms as CV | 36 | ❌ |
| Symbolic | Symbolic 2.1.0 | Acoustic cymbal model (228 modes of a bronze plate) with bell, stick position, shimmer, ride wobble, china and hi-hat pedal | 28 | ❌ |
| Rimbaud | Rimbaud 2.2.0 | Physical snare drum: coupled heads with pitch drop, colliding snare wires, rimshot, cross-stick, buzz strokes, top/bottom/mix mics, EXT sympathetic buzz, 20 presets | 30 | ✅ |
| Thunder | Thunder 2.5.0 | Drum voice (was Perun) in the Perkons style: 15 algorithms (kicks, FM, snare, slap, tom, hats, Karplus, samples, metal, shakers, 909-style kit, chaos) with A/B MORPH, filter, overdrive, bucket-brigade delay, two learnable CV inputs, WEAR, MUTATE/LOCK, 8 memories with PATCH CV | 26 | ✅ |
| Thunder | Lightning 2.5.0 | 6-slot hit catcher for Thunder: KEEP has Thunder render the last hit start to finish (full decay, echoes, already trimmed), SEND puts its knob settings back on Thunder, audio IN fallback, per-slot TUNE/DECAY/LEVEL, 6 outs + MIX, sounds saved in the patch | 28 | 🆕 |
| 808 Circuits | 808 Kick 2.5.0 | TR-808 bass drum circuit model: bridged-T, attack jump, pitch sigh, feedback decay into sustain; inner parts on the panel (SWEEP, BEND, STRIKE, CLICK, RETRIG, SIGH, TENSION, SHELF, DRIVE), stock-808 dots, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Snare 2.5.0 | TR-808 snare circuit: two tuned bridged-T shells (TONE = balance) + filtered noise; RATIO, HISS, SWEEP, SNAP DECAY, COLOUR, stock dots, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Clap 2.5.0 | TR-808 clap circuit: band-passed noise, burst sawtooth + reverb tail; BURSTS 1-8, SPREAD, SNAP, TAIL, BAND, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Tom / Conga 2.5.0 | TR-808 tom/conga circuit: bridged-T with bend + pink-noise slap, LOW/MID/HIGH and TOM/CONGA; MUTE, DOUBLE, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Rim 2.5.0 | TR-808 rim shot circuit: two high resonators through a fast transistor VCA + high-pass; RATIO, RING, GATE, SNAP, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Clave 2.5.0 | TR-808 claves circuit: high bridged-T; RING, STRIKE, BEND, OVERTONE, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Maracas 2.5.0 | TR-808 maracas circuit: high-passed noise + short VCA; LENGTH, CUT, GRAINS, SHAKE, ATTACK, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Cowbell 2.5.0 | TR-808 cowbell circuit: 540/800 Hz squares, band-pass, two-stage VCA; INTERVAL, WIDTH, FILTER, SHAPE, AGE | 28 | ❌ (new today) |
| 808 Circuits | 808 Circuit Bench 2.5.0 | All 13 TR-808 voices in one kit (hats/cymbal = 808 Metal engine): 16 knobs take every part of the selected voice (named live, grouped by circuit stage), 4 kit memories + 11 factory kits, double-click = loaded kit then stock, RAND near the kit / SHIFT + RAND anywhere (trigger input), 13 TRIG + OUT, ACC, POLY, stereo mix | 36 | ❌ (new today) |
| 909 Circuits | 909 Kick 2.3.1 | TR-909 bass drum circuit: reset VCO + waveshaper, pitch sweep (TUNE), click (pulse + noise), stock body 49.5 Hz as a real 909; BASE, SWEEP, BEND, SHAPE, stock-909 dots, AGE; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Snare 2.3.1 | TR-909 snare circuit: two swept VCOs + filtered noise (TONE, SNAPPY); RATIO, BALANCE, SWEEP, BEND, SHAPE, COLOUR, AGE; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Tom 2.3.1 | TR-909 tom circuits LOW/MID/HIGH: swept VCO, waveshaper, skin noise; DOUBLE, MUTE, AGE; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Rim 2.3.1 | TR-909 rim shot circuit: three high resonators, transistor VCA, high-pass; RATIO, RING, GATE, SNAP, STRIKE, AGE; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Clap 2.3.1 | TR-909 clap circuit: band-passed noise, burst sawtooth + reverb tail; BURSTS, SPREAD, SNAP, TAIL, BAND, AGE; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Cymbals 2.3.1 | TR-909 6-bit hat/crash/ride path (clocked DAC, BITS, RECON, VCA, closed/open choke) around a built-in metal source or your own WAV; RAND + NEAR STOCK buttons with keys and trigger jacks | 16 | ❌ (new today) |
| 909 Circuits | 909 Circuit Bench 2.3.1 | All eleven 909 voices in one module: 16 knobs follow the selected voice (grouped by stage, stock/kit dots, double-click reset), 4 kit memories + 11 factory kits, RAND (SHIFT = wild) + RAND jack, CH chokes OH, own WAVs for hats/crash/ride, per-voice outs + stereo mix | 36 | ❌ (new today) |
| Gongoozler | Gongoozler 2.2.0 | Gong and tam-tam physical model (256 modes, ~1 % CPU): tam-tam, wind, opera and bossed/gamelan gongs; hard hits bloom into a shimmering wash, ROAR stretch, opera pitch GLIDE, ombak BEAT, mallet + position per hit, towel MUTE, HAND grab | 28 | ❌ (new today) |
| Gongoozler | Gonzo 2.1.0 | The Gongoozler engine pushed past real gongs: 2 cm to 20 m, mode LAW and ORDER (harmonic to scattered), DENSITY, BOW and audio EXCITE input, freeze, reversed TILT, bloom that sinks into the lows, GLIDE ±36 | 36 | ❌ (new today) |
| Sophie | Sophvcv 2.3.0 | Sixteen-pad metallic FM percussion: kicks, clangs, zaps, bells and shards | 48 | ? |
| STRIKE | HaerSystemMk1 2.11.0 | Modal percussion: 32 tuned resonators morphing wood, rubber, metal, glass, skin, plus a vowel mode; up to 8 voices | 20 | ❌ |
| 727 | SimpleKits 2.12.0 | 15 built-in Latin percussion samples, trigger and level per sound, accent, mix out and separate outs | 20 | ✅ |
| 707 | SimpleKits 2.12.0 | 13 drum voices with several recordings each, played round robin; accent, hat choke, mix and separate outs | 20 | ❌ |
| 909 | SimpleKits 2.12.0 | TR-909 multisample player: 11 voices, stepped knobs pick recordings made at each knob position, measured accents | 35 | ❌ (new today) |
| Breaker | Breaker 2.0.0 | Break analyser + drum pattern generator: finds the hits in your break WAV, sorts them kick/snare/hat/other, plays new patterns from them (Break, Breaks, Jungle, Hip hop, Broken techno); density, ghosts, fills, swing, variety, MUTATE + LOCK, lane outs + gates | 36 | ❌ (new today) |
| Jungle Sampler | SimpleKits 2.12.0 | Break slicer for your own WAV: 8/16/32 slices, repitch to tempo (Akai style), pitch, crunch, decay, start; trigger, slice, reverse and stutter inputs; stereo | 20 | ❌ (new today) |
| Randrum | Randrum 2.6.0 | Drum sample player that rolls dice on every hit (sample, pitch, decay, start, pan, reverse, chance); LOOP and MUTATE | 28 | ? |
| Tumble | Paddock 2.0.0 | Five rhythm cores coupled by TOUCH knobs (pull together / push apart, Blasser Rollz style), each striking its own tuned gong; 5 gates, stereo gongs, sync | 20 | ❌ (new today) |
| Tuned Perc | TunedPerc 2.4.0 | One percussion sample, pitch-detected and tuned to C, then played from V/OCT so it sits in key | 12 | ? |
| Panorama | Panorama 2.0.0 | Steel pan (tenor, double second, triple cello): modal note areas that ring into their neighbours, octave bloom, onset glide, rolls, poly GATE + V/OCT, imperfect re-rollable tuning with per-note hand tuning on screen | 40 | ❌ (new today) |
| Hangover | Hangover 2.0.0 | Handpan (hang drum) by modal synthesis with the real instrument's dirt: tuning drift, beating partials, rattles and a buzzing field, bleed between fields, breath through the gu, skin noise, sloppy hands, small room (DIRT master + 8 knobs, re-rollable instrument); 9 scales, DING/1-8/GU triggers, poly GATE + V/OCT, roll and palm-mute gates, stereo | 42 | ❌ (new today) |
| Chirp | Chirp 2.0.0 | Pitch-swept chirp voice: one glide that fades, the shape behind kicks, zaps, bats, frogs, crickets, birds and looped cicadas; trains of up to 16 chirps, LOOP, TONE sine to buzz; ENV and PITCH-contour outs for modulation; poly | 16 | ❌ (new today) |
| Datamatic | Test Pattern 2.1.0 | Four envelope-free Ikeda voices: sub sine (to square), sine tone LO / 1 kHz / up to 18 kHz with L/R beating and data harmonic SCAN, 1-32 sample clicks, white noise bursts; one LINK cable from Datamatic | 20 | ❌ (new today) |

## Oscillators and synth voices

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Bell Foundry | Peal 2.0.0 | Twelve modal bells (one per poly channel) to ring from Peal: church/handbell/gamelan profile, metal, thickness, size, crack, clapper; true-harmonic, old or ombak tuning; heard from the ringing chamber, churchyard or across town; RECAST rolls new bells | 24 | ❌ (new today) |
| Buchla 259t | Buchla 2.0.0 | Complex oscillator after the Buchla / Tiptop 259t: mod osc, principal osc, timbre / symmetry / order wavefolder; 16-voice poly | 34 | ❌ |
| 6 Copains | SixCopains 2.5.0 | Six Just Friends-style voices with morph, per-voice outs and a built-in Banks note player (port of 6Copains for disting NT) | 42 | ? |
| Pond | Pond 2.0.0 | Frog and toad chorus: up to 16 frogs of 16 species that call and answer (TALK: sync to answer), sing in bouts and speed up with TEMP; HUSH (a predator) silences them, then they return tentatively over RETURN; CLOCK, poly CALL, poly TRIG/GATE/PITCH outs | 22 | ❌ (new today) |
| Menagerie | Menagerie 2.0.0 | Macro oscillator with 16 engines of our own (none from Plaits/Braids) behind SHAPE, TEXTURE and LIFE: Gendy, Herd, Duffing, Scanned, Walsh, PLL, Bytebeat, Automaton, Bubbles, Spring, Bounce, Howl, Tine, Tonewheel, Pad, Harmonium; 16-voice poly, MAIN + AUX | 18 | ❌ (new today) |
| Zvuni | Zvuni 2.3.0 | Three-voice formant singer after Bulgarian women's singing: drone, melody and a partner tuned for a ringing second | 40 | ? |
| Mangrave | JustFiends 2.5.0 | Formant oscillator after the Mangrove technical map (impulse train, undertones, AIR overdrive) with the PPM section built in (impulse/sync/cycle, trigger, to-air, sine/saw/PWM/hollow); poly | 32 | ❌ |
| FMX | HaerSystemMk1 2.11.0 | Four-operator FM voice whose loudness and brightness follow a phase ramp or trigger; up to 8 voices | 24 | ❌ |
| Dub Siren | DubSiren 2.2.0 | The classic UK sound-system transistor siren (NJD SE-1 circuit, as in the Rigsmith NS1) modelled part by part: PITCH/MODE/BEAT switches, SIREN and TONE buttons, supply sag on TRIG off; EXTRAS switch adds DEPTH, RELEASE, 80s tape or 90s chip echo and CV jacks | 26 | ❌ (new) |
| Facet | Facet 2.1.0 | Early-90s layered digital synth (JD-800 style): 4 tones per patch from an original 98-wave ROM (loops, moving waves, attacks, noise, 4 user WAVs), per-tone filter, amp, pitch envelopes and 2 LFOs, full effects rack, 90s DAC switch, 96 presets, 8 memories, in-tune RAND, MUTATE + LOCK | 56 | ❌ (new today) |

## Sequencers and pattern makers

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Sprout | Lindenmayer 2.0.0 | L-system rhythm that grows: Fibonacci/Thue-Morse/Cantor/Tribonacci... strings spread over a bar, one generation denser every N cycles, or an endless stream; A/B/C trigger outs | 12 | ❌ (new today) |
| Graft | Lindenmayer 2.0.0 | Four-lane drum groove (Detroit/Electro/Break/Broken) rewritten each generation by chance rules: ratchet, drop, sprout, home | 16 | ❌ (new today) |
| Bloom | Lindenmayer 2.0.0 | Chord progressions grown by L-system rules on scale degrees, six modes, voice-led; poly chord, bass, gate, change outs | 14 | ❌ (new today) |
| Irama | Irama 2.2.0 | Gamelan sequencer, Java and Bali: gong cycles with colotomic triggers, pathet-weighted balungan that mutates per gong, saron/peking/bonang at 5 irama levels, kotekan/imbal pairs, angsel, kebyar, suwuk, ombak pairs on every pitch out | 40 | ❌ (new today) |
| Gatra | Irama 2.2.0 | Gamelan structure as a house/techno drum sequencer: straight kick, gong cycle on drum roles, LAND moves the gong from cycle end to downbeat, kotekan hats, polymeter cycles, swing, drop and fill | 24 | ❌ (new today) |
| Ageng | Irama 2.2.0 | Gamelan gong pair (gong ageng + second gong) and kempul: modal bronze with slow bloom, ombak beating, pitch sag, sub output, hand damping | 18 | ❌ (new today) |
| Gangsa | Irama 2.2.0 | Balinese gangsa: polos + sangsih players, every note an umbang + isep pair shimmering at the ombak rate, kantilan octave, hand damping, bamboo tube, seeded tuning errors | 20 | ❌ (new today) |
| Reyong | Irama 2.2.0 | Kettle gongs, Java bonang or Bali reyong: boss to rim strikes, byot choked hits, cek clash, sympathy between kettles, plus kenong and kethuk | 22 | ❌ (new today) |
| Peal | Peal 2.0.0 | Change ringing sequencer: 4-12 bells, one bell per clock, rows change by Plain Hunt, Plain Bob, Little Bob, Grandsire, Stedman, Cambridge Surprise or self-mutating Wander; bob/single calls by button, trigger or chance; open handstroke GAP; gate per bell, poly gates/pitches/row, lead-end, rounds and call triggers | 36 | ❌ (new today) |
| Comper | ComperFable 2.5.0 | Jazz piano comping player (notes out): rootless/shell/quartal/spread voicings with voice leading, swing/bossa/ballad/soul/house/reggae figures, 44 progressions (jazz, house, gospel, blues, disco, soul, funk, reggae), chart and progressions on the screen, PRESET knob, OCTAVE, LENGTH (human note lengths), LOOSE, LISTEN | 38 | ❌ (new today) |
| Soloist | ComperFable 2.5.0 | Horn soloist for Comper, Walker and Rider: modelled tenor sax, trumpet, trombone (or notes only); writes its own head from the chart, solos in phrases, trades 2s/4s/8s with drums/piano, OUTSIDE up to free, two Soloists harmonise or take turns; band follows with no cables | 30 | ❌ (new today) |
| Francois Markovian | FrancoisMarkovian 2.6.0 | 8-step, 6-track trigger sequencer whose playhead jumps by a hand-set Markov matrix, with evolve and freeze | 34 | ? |
| Rewrite | FrancoisMarkovian 2.6.0 | Per-track pattern grower: seeds a bar of triggers, then rewrites hits and rests by rules every few bars | 28 | ? |
| Seuperclidean | Seuperclidean 2.3.0 | Six Euclidean rhythms with bias warp, self-mutation, fills, swing, humanise and logic outs | 38 | ? |
| Seuperclidean 4 | Seuperclidean 2.3.0 | Four biased Euclidean voices in columns with step rings, mutation and cross-pair logic outs | 28 | ? |
| Metroplex | Metroplex 2.3.0 | 16-stage pulse-count sequencer with accumulators, slides, scale quantising and a menu display | 58 | ? |
| X\|FORMER Multi | XFormer 2.4.0 | All seven X\|FORMER sequencer engines behind one panel (Algo, Stochastic, Note, Indexed, Discrete, PhaseFlux, Fractal) | 26 | ? |
| Tetrapods | Tetrapods 2.2.0 | Four-track sequencer built from two tetrachords, including quarter-tone maqam genera, arp or poly chord, 8 patterns | 40 | ? |
| Thorax | Thorax 2.0.0 | 8-track algorithmic sequencer in the spirit of the Torso T-1, every control on its own labelled knob | 40 | ❌ |
| Kleener | Kleener 2.0.0 | Klee-style 16-bit loop sequencer: bits sum through pitch-weight sliders, bus gates, mutation, quantiser | 30 | ❌ |
| SQ10izer | SQ10izer 2.1.0 | 16 steps of small unquantised pitch offsets (up to ±40 cents) added to V/OCT: early analogue sequencer wobble | 20 | ? |
| The Changes | Weighted 2.7.0 | Chord sequencer that picks the next chord in a chosen style, voice-led; leads Weighted and follows Long Form | 28 | ❌ |
| Long Form | LongFormMixxa 2.3.0 | Arrangement conductor: draw a song form of up to 8 sections and 8 lanes; STEER picks energy, DRIFT rewrites the form | 52 | ❌ |
| Ritmo | SimpleKits 2.12.0 | 8-track Latin pattern sequencer, 32 rhythms, plays a 727 placed beside it with no cables | 28 | ❌ |
| Jack | SimpleKits 2.12.0 | 12-track house + techno sequencer, 32 pre-2000 classic grooves with fills (FILL jack + button), RAND button per lane (random 2-16 hits, 6 on average), plays a 909 or 707 placed beside it with no cables | 30 | ❌ (new today) |
| Jungle Seq | SimpleKits 2.12.0 | Jungle break re-sequencer: 32 steps of slice / hold / silence with reverse, stutter and pitch, 32 chops incl. 3+3+2, editable; mutate, fill, kick/snare/hat trigger outs; plays a Jungle Sampler on its right with no cables | 30 | ❌ (new today) |
| GridDrums | Gridworks 2.4.0 | Six-track, sixteen-step trigger sequencer played from a monome grid or the on-screen grid, 16 pattern slots | 24 | ❌ |
| LessConcepts | Gridworks 2.4.0 | Cellular-automaton melody sequencer for two voices (after the norns script less concepts), grid control | 28 | ❌ |
| MUTATE | HaerSystemMk1 2.11.0 | 8-track drum sequencer that writes its own patterns and slowly rewrites them: ratchets, micro-timing, conditions | 38 | ❌ |
| CONTOUR | HaerSystemMk1 2.11.0 | Rhythms where a curve crosses levels, with an 8-phasor interference source, up to audio rate | 28 | ❌ |
| CHAIN | HaerSystemMk1 2.11.0 | Four melody voices picking notes from a next-note table, with harmony rules between them | 34 | ❌ |
| VAULT | HaerSystemMk1 2.11.0 | Always records the last 64 bars of 8 gate/CV lanes; KEEP saves clips that play back transposed, reversed, thinned | 40 | ❌ |
| Datamatic | Datamatic 2.1.0 | Ryoji Ikeda style data rhythm: pi, e, primes, Fibonacci, counting, text, seeded random or CV become a 4-64 bit loop read by four interlocking gate lanes at 1-16x the clock; ROLL ratchets, CUT to digital silence, MUTATE; LINK cable to Test Pattern | 20 | ❌ (new today) |

## Clocks, logic and randomness

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| WARP | HaerSystemMk1 2.11.0 | Curved master clock: drifting tempo, bending bars, four related phase outputs | 24 | ❌ |
| Ensemble | DriftEnsemble 2.4.0 | One clock in, six human players out: 1/f timing drift with spread, lean, feel, swing, togetherness, leader | 22 | ? |
| Divskip | Divskip 2.1.0 | Four channels splitting a clock between two outputs (coin toss, divider, Turing, Euclid, ratchet, patterns...), chainable | 32 | ? |
| Dung Revisions | DungRevisions 2.2.0 | Clock divider into OR buses feeding a 16-step Rungler-style shift register, 1-, 3- and 8-bit outs | 28 | ? |
| Aksak | Zvuni 2.3.0 | Clock divider for Bulgarian limping meters (2+2+3, 2+2+2+3...) with custom groups and mutation | 10 | ? |
| Interlock | FrancoisMarkovian 2.6.0 | Six triggers in and out: a 6x6 grid where each track firing raises or lowers the others' chances | 16 | ? |
| Governor | FrancoisMarkovian 2.6.0 | Listens to up to 6 trigger tracks and outputs CVs that hold their density and variety at a target | 22 | ? |
| EGX Eightfold Gesture Extractor | Eightfold 2.1.0 | Turns the motion of a CV (or a channel index) into 8 gates, odd/even gates and a change pulse | 10 | ? |
| FLOW | HaerSystemMk1 2.11.0 | A small 2D fluid as a modulation source: four injectors stir it, eight probes read it | 30 | ❌ |
| EAR | HaerSystemMk1 2.11.0 | Listens to two audio inputs and learns a loop's groove as a phase curve that can drive WARP | 28 | ❌ |
| Lilt | Lilt 2.0.0 | Relabi generator: four LFOs at ratios of a pulse are summed and four thresholds on the sum make triggers and gates that drift around the beat; DRIFT, MUTATE (+FREEZE) and GRID steer between loose and locked; clock in, LFO/SUM/accent outs | 36 | ❌ (new today) |
| Groover | Groover 2.0.1 | Ableton grooves on VCV drums: loads Live .agr groove files or .mid (16 built-ins, slots 1-2 = 75% swing defaults), grooved clock out (early and late steps), 8 trigger lanes that keep gate length, ACCENT and LOOP outs, Live-style Amount/Timing/Velocity/Random/Quantize with CV | 20 | ❌ (new today) |
| Pivot | Pivot 2.0.0 | Metric modulation clock: follows Live's clock through a chain of up to 8 OLD note = NEW note tempo pivots (e.g. 1/8. = 1/4 is x4/3), snap or glide 1 beat-16 bars, LAND brings you home exactly on Live's bar line, AUTO/NEXT/SELECT walk the chain; GHOST clock of the tempo you left, RATIO (1 V per doubling), PIVOT and AWAY outs | 30 | ❌ (new today) |
| Deidre | Deidre's Latest Workout 2.0.0 | Pam's Pro Workout style 8-output clocked modulation with nothing in menus: 23 knobs (rate, 14 shapes, Euclid, loop nap/wake, flex, 18 cross ops, quantiser) follow the selected output, big clickable 7-page screen, per-knob CV, 64 banks, external clock any PPQN | 56 | ❌ (new today) |

## Pitch and quantisers

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Weighted | Weighted 2.7.0 | Weighted quantiser: random CV in, notes out, strong scale degrees chosen more often; gamelan pathet and tunings | 20 | ? |
| TUNE MAP | HaerSystemMk1 2.11.0 | Pulls pitch CV into harmonic fields (just, partials, gamelan, Bohlen-Pierce...) and morphs between fields | 18 | ❌ |
| Keel | Keel 2.0.0 | Kick drum tuning: follows the key or chord and sends V/OCT to any kick, interval pattern with MUTATE + LOCK, hears the kick's pitch (AUTO correct, LEARN), swaps its low end for a tuned sub | 30 | ❌ (new today) |

## Envelopes, functions and modulation

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Surprise | Surprise 2.0.0 | Novelty follower: NEW CV jumps when audio does something not heard in the last MEMORY s (8 bands, nearest-match memory), TRIG on new events, BORED loop-fatigue output, 8-band poly novelty, FREEZE/FORGET | 14 | ❌ (new today) |
| Krka | Krka 2.1.0 | Four function generators (Zadar-style, no menus): 260 drawn shapes in 26 banks, time 0.85 ms-30 min or clocked note values, response/warp bends, reverse, gate hold, analog retrigger, repeats, 16 chain algorithms with phase hand-over, assignable CV per channel, 18 presets | 51 | ❌ (new today) |
| Swerve | Paddock 2.0.0 | Bounce generator (Blasser Swoop style): triangle between two CV bounds with separate up/down slopes, LFO to audio, one-shot envelope mode, top/bottom pulses, paradox when bounds cross | 12 | ❌ (new today) |
| Gallop | Paddock 2.0.0 | Four bounce oscillators stacked so each is bounded by its neighbours (Blasser Fourses style): chaotic, self-changing shapes, tilt, bound inserts, mix + poly out, scope | 24 | ❌ (new today) |
| Just Fiends | JustFiends 2.5.0 | Six linked slopes (envelopes, LFOs, oscillators) sharing one Time, spread by Intone, with every mode labelled | 34 | ? |
| EFG Eightfold Function Generator | Eightfold 2.1.0 | 8-channel function and envelope generator fired by one gate through index, round, parallel and binary span modes | 22 | ? |
| ETS Eightfold Time Smear | Eightfold 2.1.0 | Time-shifts one CV into 8 copies read from a shared buffer at their own speed and phase | 12 | ? |

## Filters

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Weird Sisters | JustFiends 2.5.0 | Three linked filters after Mannequins Three Sisters: low/centre/high around one freq, span, crossover or formant, dry/wet to self-oscillation; poly | 24 | ❌ |
| TwinPeaks | TwinPeaks 2.0.0 | Hordijk-style twin-peak filter: two resonant low-passes subtracted, a resonant peak at each band edge; poly | 18 | ❌ |
| DistantPlanet | DistantPlanet 2.1.0 | Steiner-Parker style filter with LP, BP and HP inputs into one shared filter (patch several for notch/blends); poly | 8 | ❌ |

## Effects and audio processing

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Bore Room | Bore 2.0.0 | Reverb modelled on an MRI scanner tube: wall flutter, cross and length modes from bore size (head coil to 70 cm), short dark tail, room through the open ends, earplugs/headphones and body sound | 12 | ❌ |
| Razor | Razor 2.1.0 | Live reverse-tape splice: the next 1/2 to 4 beats are cut out, flipped and spliced back in time | 28 | ? |
| SPLICE | HaerSystemMk1 2.11.0 | Cut-up and smear buffer: stutters the slice just gone by with speeding, pitched, reversed or granular repeats | 26 | ❌ |
| DEPTH | HaerSystemMk1 2.11.0 | Reverb placing four stereo sources at different distances in one room, shared tail with freeze and bloom | 24 | ❌ |
| Morphader | Morphader 2.0.0 | Stereo crossfader with 14 ways from A to B (filter swap, gate chop, vocoder, tape stop, dub throw, freeze...) | 20 | ❌ |
| Vinyl Countdown | VinylCountdown 2.7.0 | Eight vinyl stages in record order: lathe, RIAA, pressing wear, inner groove, stylus, off-centre wow (33/45), valve phono preamp, second deck; bypass, 35 presets chosen with panel arrows | 36 | ❌ |
| Scratch | VinylCountdown 2.7.0 | Human scratch sequencer: 12 gestures on 8 steps or a trigger, scratching live audio or a WAV | 24 | ❌ |
| Studio | VinylCountdown 2.7.0 | The record before the lathe: console drive, master tape (speed, flutter, head bump), Dolby A and its classic mistakes, vari-mu glue | 20 | ❌ (new) |
| Listening Room | VinylCountdown 2.7.0 | The record after the preamp: power amp, eight speakers from phone to club stack, a room with bass modes, the listener's ears | 20 | ❌ (new) |
| On Air | VinylCountdown 2.7.0 | A record played on FM radio end to end: broadcast processing, stereo multiplex, multipath, a next station, and the receiver | 24 | ❌ (new) |
| Sound System | SoundSystem 2.0.0 | Makes any input sound like a big rig in a real place: 8 systems from dub hall to "through the wall" with a DOOR | 20 | ❌ |
| Heathrow | Heathrow 2.0.0 | Very controllable stereo phaser/flanger: morph, 2-24 stages, ± feedback, through-zero, clocked/stepped/audio-rate sweeps, second switchable engine (series/parallel/split), 6 memory slots with glide and SLOT CV | 42 | ❌ |
| CopyThat | Copy That 2.0.0 | FM radio link that loses the signal (Fairfield Roger That-style): DRIVE, TUNE the receiver off station, SHIFT tracking, out-of-phase wet for artifacts only, neighbouring station, FADE in, SIGNAL/LOCK out, poly | 14 | ❌ |

## Samplers, granular and loopers

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Pollen | Pollen 2.1.0 | Stereo granular sampler with 4 sounds, 2 morphing scenes, X/Y/Z/RAND modulation and a live buffer | 36 | ? |
| Tapejeans | Tapejeans 2.2.0 | Tape and microsound: a reel cut into splices, played as genes with vari-speed, morph and slide | 36 | ? |
| Stella | Stella 2.1.0 | Sample map player: lays folders of one-shots out on a 2D map by sound; four voices with chance of a nearby sample | 36 | ❌ |
| Ectoplasm | Ectoplasm 2.1.0 | Break slicer/mangler running the open-source Ectocore firmware (Zack Scholl, GPL-3): every hold-TAP/hold-MODE function on its own knob, BANK/TEMPO/PLAY/RESET, 14 CVs, display, loads plain WAV folders | 24 | ❌ (new today) |

## Mixers, routing and performance control

| Module | Plugin | What it does | HP | Tested in Rack |
|---|---|---|---|---|
| Mixxa | LongFormMixxa 2.3.0 | 8-channel performance mixer: mutes, solos, scenes and random mutes land on the beat; kick ducking and glue comp | 58 | ❌ |
| DriftMatrix | DriftMatrix 2.0.0 | 32 x 32 matrix mixer with a hidden random matrix, reseed, slew and 8 snapshots with A/B mixing | 36 | ❌ |
| GridMutes | Gridworks 2.4.0 | 18-channel clocked performance mutes from a monome grid: cue, lock, solo, A/B pairs, 16 scenes, click-free fades | 41 | ❌ |
| GridKeys | Gridworks 2.4.0 | Push-style note keyboard on a monome grid with scales, chord pads, 10-slot chord memory, strum, latch | 36 | ❌ |
| STEER | HaerSystemMk1 2.11.0 | 8 macros moving up to 16 parameters each, recordable gestures, 16 scenes and whole-rack snapshots to morph | 38 | ❌ |
| JumbleSale | Eightfold 2.1.0 | 8 in, 8 out routing after the Make Noise Jumbler: Rotate shifts inputs across outputs, Radiate blends neighbours | 20 | ❌ |
| Minimal Nation | Odds 2.1.0 | One big knob, one in, one out. The signal passes straight through and the knob does nothing. It just looks good | 12 | ? |
| Poltergeist | Odds 2.1.0 | A ghost that moves real cables inside a fence of modules to its right: swaps, wanders, vanishes; EXORCISE and Ctrl+Z put them back | 14 | ❌ |

---

## Plugins at a glance

| Plugin | Version | Repo | Modules | Build |
|---|---|---|---|---|
| Comper Fable | 2.5.0 | hardyfinn/2026SoloModules (ComperFable, PR #21) | Comper, Walker, Rider, Soloist | vcv-builds |
| 808Metal | 2.1.0 | [hardyfinn/808metal](https://github.com/hardyfinn/808metal) | 808 Metal | vcv-builds |
| Alexa02 | 2.3.0 | [hardyfinn/Alexa02](https://github.com/hardyfinn/Alexa02) | Alexa02, Alexa01 | vcv-builds |
| Boohm | 2.0.0 | [hardyfinn/Boohm](https://github.com/hardyfinn/Boohm) | Boohm | vcv-builds |
| Bore | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Bore, PR #8) | Scanner, Bore Room | vcv-builds |
| Buchla | 2.0.0 | [hardyfinn/Buchla](https://github.com/hardyfinn/Buchla) | Buchla 259t | vcv-builds |
| DistantPlanet | 2.1.0 | [hardyfinn/distantplanet](https://github.com/hardyfinn/distantplanet) | DistantPlanet | vcv-builds |
| Divskip | 2.1.0 | [hardyfinn/divskeep](https://github.com/hardyfinn/divskeep) | Divskip | vcv-builds |
| DriftEnsemble | 2.4.0 | [hardyfinn/DriftEnsemble](https://github.com/hardyfinn/DriftEnsemble) | Ensemble | vcv-builds |
| DriftMatrix | 2.0.0 | [hardyfinn/DriftMatrix](https://github.com/hardyfinn/DriftMatrix) | DriftMatrix | vcv-builds |
| DungRevisions | 2.2.0 | [hardyfinn/dung-revisions](https://github.com/hardyfinn/dung-revisions) | Dung Revisions | vcv-builds |
| Eightfold | 2.1.0 | [hardyfinn/NUSS](https://github.com/hardyfinn/NUSS) (PR #1) | EFG, ETS, EGX, JumbleSale | not in vcv-builds |
| FrancoisMarkovian | 2.6.0 | [hardyfinn/FrancoisMarkovian](https://github.com/hardyfinn/FrancoisMarkovian) | Francois Markovian, Interlock, Governor, Rewrite | vcv-builds |
| Gridworks | 2.4.0 | [hardyfinn/Gridworks](https://github.com/hardyfinn/Gridworks) | GridDrums, LessConcepts, GridKeys, GridMutes | vcv-builds |
| Groover | 2.0.1 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Groover, PR #17) | Groover | vcv-builds |
| Pivot | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Pivot, PR #12) | Pivot | vcv-builds |
| Deidre | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Deidre, PR #24) | Deidre's Latest Workout | vcv-builds |
| Gongoozler | 2.2.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Gongoozler, PR #13) | Gongoozler, Gonzo | vcv-builds |
| HaerSystemMk1 | 2.11.0 | [hardyfinn/HaerSystemMk1](https://github.com/hardyfinn/HaerSystemMk1) (PR #2) | WARP, CONTOUR, MUTATE, STRIKE, FMX, CHAIN, SPLICE, DEPTH, VAULT, STEER, FLOW, TUNE MAP, EAR | vcv-builds |
| Ectoplasm | 2.1.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Ectoplasm) | Ectoplasm | vcv-builds |
| Hangover | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Hangover, branch claude/project-thread-u8dufv) | Hangover | vcv-builds |
| Menagerie | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Menagerie, branch claude/project-thread-rtpp0w, PR #10) | Menagerie | vcv-builds |
| Heathrow | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Heathrow) | Heathrow | vcv-builds |
| CopyThat | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder CopyThat) | Copy That | vcv-builds |
| DubSiren | 2.2.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder DubSiren, PR #23) | Dub Siren | vcv-builds |
| Datamatic | 2.1.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Datamatic, PR #6 merged) | Datamatic, Test Pattern | vcv-builds |
| Krka | 2.1.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Krka) | Krka | vcv-builds |
| Lindenmayer | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Lindenmayer) | Sprout, Graft, Bloom | vcv-builds |
| Surprise | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Surprise) | Surprise | vcv-builds |
| HaerChirp | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Chirp, PR #4) | Chirp | vcv-builds |
| Panorama | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Panorama) | Panorama | vcv-builds |
| Pond | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Pond, branch claude/project-thread-52zze4, PR #5) | Pond | vcv-builds |
| JustFiends | 2.5.0 | [hardyfinn/just-fiends](https://github.com/hardyfinn/just-fiends) | Just Fiends, Weird Sisters, Mangrave | vcv-builds |
| Kleener | 2.0.0 | [hardyfinn/Kleener](https://github.com/hardyfinn/Kleener) | Kleener | vcv-builds |
| Lilt | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Lilt, branch claude/project-thread-v6ld0y) | Lilt | vcv-builds |
| LongFormMixxa | 2.3.0 | [hardyfinn/LongForm-Mixxa](https://github.com/hardyfinn/LongForm-Mixxa) | Long Form, Mixxa | vcv-builds |
| Metroplex | 2.3.0 | [hardyfinn/metroplex](https://github.com/hardyfinn/metroplex) | Metroplex | vcv-builds |
| Morphader | 2.0.0 | [hardyfinn/Morphader](https://github.com/hardyfinn/Morphader) | Morphader | vcv-builds |
| Odds | 2.1.0 | [hardyfinn/Odds](https://github.com/hardyfinn/Odds) | Minimal Nation, Poltergeist | vcv-builds |
| Irama | 2.2.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Irama, branch claude/project-thread-uon1bv, PR #7) | Irama, Gatra, Ageng, Gangsa, Reyong | vcv-builds |
| Peal | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Peal, branch claude/project-thread-xb83vo) | Peal, Bell Foundry | vcv-builds |
| Paddock | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Paddock) | Swerve, Gallop, Tumble | vcv-builds |
| Pollen | 2.1.0 | [hardyfinn/grain](https://github.com/hardyfinn/grain) | Pollen | vcv-builds |
| Randrum | 2.6.0 | [hardyfinn/randrum](https://github.com/hardyfinn/randrum) | Randrum | vcv-builds |
| Razor | 2.1.0 | [hardyfinn/razor](https://github.com/hardyfinn/razor) | Razor | vcv-builds |
| SQ10izer | 2.1.0 | [hardyfinn/SQ10izer](https://github.com/hardyfinn/SQ10izer) | SQ10izer | vcv-builds |
| Seuperclidean | 2.3.0 | [hardyfinn/seuperclidean](https://github.com/hardyfinn/seuperclidean) | Seuperclidean, Seuperclidean 4 | vcv-builds |
| Breaker | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (folder Breaker, PR #32) | Breaker | vcv-builds |
| SimpleKits | 2.12.0 | [hardyfinn/SimpleKits](https://github.com/hardyfinn/SimpleKits) (private) | 727, 707, 909, Ritmo, Jack, Jungle Seq, Jungle Sampler | vcv-builds |
| SixCopains | 2.5.0 | [hardyfinn/SixCopains](https://github.com/hardyfinn/SixCopains) | 6 Copains | vcv-builds |
| Sophvcv | 2.3.0 | [hardyfinn/Sophvcv](https://github.com/hardyfinn/Sophvcv) | Sophie | vcv-builds |
| SoundSystem | 2.0.0 | [hardyfinn/SoundSystem](https://github.com/hardyfinn/SoundSystem) | Sound System | vcv-builds |
| Stella | 2.1.0 | [hardyfinn/Stella](https://github.com/hardyfinn/Stella) | Stella | vcv-builds |
| Symbolic | 2.1.0 | [hardyfinn/symbolic](https://github.com/hardyfinn/symbolic) | Symbolic | vcv-builds |
| Facet | 2.1.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Facet/, PR #30) | Facet | vcv-builds |
| Rimbaud | 2.2.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Rimbaud/, PR #14) | Rimbaud | vcv-builds |
| Thunder | 2.5.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Thunder/, PR #27) | Thunder, Lightning | vcv-builds |
| 808 Circuits | 2.5.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Circuit808/, PR #28, GPL-3 as a whole) | 808 Kick, 808 Snare, 808 Clap, 808 Tom / Conga, 808 Rim, 808 Clave, 808 Maracas, 808 Cowbell, 808 Circuit Bench | vcv-builds |
| 909 Circuits | 2.3.1 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Circuit909/, PR #29, 2.3.1 kick pitch PR #33) | 909 Kick, 909 Snare, 909 Tom, 909 Rim, 909 Clap, 909 Cymbals, 909 Circuit Bench | vcv-builds |
| Keel | 2.0.0 | [hardyfinn/2026SoloModules](https://github.com/hardyfinn/2026SoloModules) (Keel/, PR #34) | Keel | vcv-builds |
| Tapejeans | 2.2.0 | [hardyfinn/tapejeans](https://github.com/hardyfinn/tapejeans) | Tapejeans | vcv-builds |
| Tetrapods | 2.2.0 | [hardyfinn/tetrapods](https://github.com/hardyfinn/tetrapods) | Tetrapods | vcv-builds |
| Thorax | 2.0.0 | [hardyfinn/Thorax](https://github.com/hardyfinn/Thorax) | Thorax | vcv-builds |
| TunedPerc | 2.4.0 | [hardyfinn/tunedperc](https://github.com/hardyfinn/tunedperc) | Tuned Perc | vcv-builds |
| TwinPeaks | 2.0.0 | [hardyfinn/twinpeaks](https://github.com/hardyfinn/twinpeaks) | TwinPeaks | vcv-builds |
| VinylCountdown | 2.7.0 | [hardyfinn/VinylCountdown](https://github.com/hardyfinn/VinylCountdown) | Vinyl Countdown, Scratch, Studio, Listening Room, On Air | vcv-builds |
| Weighted | 2.7.0 | [hardyfinn/Weighted](https://github.com/hardyfinn/Weighted) | Weighted, The Changes | vcv-builds |
| XFormer | 2.4.0 | [hardyfinn/xformer-vcv](https://github.com/hardyfinn/xformer-vcv) | X\|FORMER Multi | vcv-builds |
| Zvuni | 2.3.0 | [hardyfinn/zvuni](https://github.com/hardyfinn/zvuni) | Zvuni, Aksak | vcv-builds |

"vcv-builds" means the file `/mnt/project-files/vcv-builds/<Plugin>-<version>-mac-arm64.vcvplugin`.

## Notes

- **Eightfold** (EFG, ETS, EGX, JumbleSale) has no build in `vcv-builds`; JumbleSale is waiting in PR #1 on hardyfinn/NUSS.
- **Not VCV, so not listed above:** Loopshifter and Razor Max for Live devices, the X|FORMER M4L devices, and the Drift Ensemble disting NT algorithm.
- **Ideas and mockups only:** Tomtune (panel mockup, 3 Nov).

## Keeping this list up to date

Any thread that builds or changes a VCV module updates this file in the same turn: version, HP, description and test status. When Haer reports trying a module in Rack, change its mark to ✅. The PDF copy (`vcv-modules-list.pdf`) is regenerated from this file: `python3 vcv-modules-list-source/pdf.py vcv-modules-list.md /tmp/cat.html` then `node vcv-modules-list-source/pdf.js /tmp/cat.html vcv-modules-list.pdf` (needs `pip install markdown`).
