# Irama

Gamelan sequencing for VCV Rack: two modules in one plugin.

## Irama (40 HP): gamelan sequencer, Java and Bali

One clock in; the whole layered gamelan texture out.

- **STYLE**: JAVA or BALI (gong kebyar).
- **FORM**: Java lancaran (16 beats), ketawang (16), ladrang (32), gendhing (64); Bali gilak (8), bapang (16), tabuh pisan (32), tabuh telu (64).
- **PATHET**: Java slendro nem / sanga / manyura, pelog lima / nem / barang; Bali selisir, slendro.
- **ROOT**: transposes everything, ±12 semitones. Right-click: snap to 12-TET.
- **IRAMA**: lancar, tanggung, dadi, wilet, rangkep (1, 2, 4, 8, 16 notes of the fastest part per balungan beat). Changes land at the next gatra.
- **TEMPO**: TRAD (the balungan beat stretches as irama rises, as in Java) or FIXED (the beat stays 4 clocks long; only the density changes, for techno).
- **KOTEKAN**: imbal, norot, nyog cag, telu, empat. POLOS and SANGSIH only add up to a continuous line together.
- **MUTATE**: chance per gong of rewriting a gatra (two at high settings). **LOCK** freezes the balungan. **NEW** rebuilds it.
- **FOCUS**: how strongly landing tones favour the stressed and strong tones of the pathet.
- **OMBAK** (0-10 Hz): every pitch output becomes 2 channels, umbang (low) and isep (high), detuned by a fixed number of Hz so the shimmer is even across the range.
- **ANGSEL** (knob = chance per gatra, button / trigger = next gatra): a 3+3+2 unison break over the last two beats of the gatra.
- **KEBYAR** (button toggles, gate holds): irregular unison bursts with gongs at the start and end.
- **SUWUK**: drops to irama lancar and ends on the next gong. Press again to restart.

Outputs: GONG, KENONG (kempur), KEMPUL (kemong), KETHUK (kempli), KEMPYANG (kajar), GATRA (every 4th beat), KENDANG (cues), ANGSEL (gate). V/OCT + GATE for SARON, PEKING, BONANG, POLOS, SANGSIH, SELEH (landing tone, bass octave) and POLY (all five parts, ready for Bell Foundry).

The clock is the pulse of the elaborating parts: 16ths in FIXED mode.

## Gatra (24 HP): gamelan structure for house and techno drums

Clock in 16ths. The kick stays four on the floor; the gong cycle runs over it.

- **CYCLE**: 8-32 beats. 12, 20 and 28 drift against the 4/4 bar (polymeter).
- **LAND**: END puts the gong on the last beat (gamelan), the middle on the "and" of 4 (pickup), ONE on the downbeat (techno). Every colotomic stroke moves with it.
- **BEAT**: one gamelan beat = a quarter or an eighth note.
- **KOTEKAN**: hats split into POLOS and SANGSIH: NYOG cag (strict alternation), TELU (3 against 4, with shared accents), EMPAT (shared kempyung hits).
- **DENSITY**, **SWING** (50-75%), **MUTATE** (re-rolls the hat pattern per gong), **ANGSEL** (chance of a break before the gong), **KICK** on/off.
- **DROP** (hold / gate): break; on release the gong and kick land on the next beat. Patch Irama's ANGSEL output here.
- **FILL** (hold / gate): kebyar-style random bursts.

Outputs: KICK, GONG (crash), KENONG (clap), KEMPUL (open hat), KETHUK (perc), KEMPYANG (rim), POLOS (hat), SANGSIH (shaker), ACCENT, ANGSEL (gate).

Weights for the tunings and pathet are designed from the pathet literature, not measured; gamelan tunings vary from set to set.

`make test` runs offline checks of both engines and writes `test/events.csv`; `python3 test/render_demo.py test/events.csv demo.wav` renders it.
`python3 res/make_panel.py` regenerates the panels; `python3 tools/preview.py outdir` draws previews.

## The instruments: Ageng, Gangsa, Reyong (2.1.0)

Three bronze voices built by modal synthesis (every partial a decaying resonator), designed to be played by Irama. Full design notes: `docs/instruments-design.md`.

**Ageng (18 HP): the gong pair and kempul.** GONG strikes the big gong, GONG 2 the second gong (INTERVAL semitones higher); ALT on makes the GONG input alternate the two. SIZE sets the decay (6 to 30 s), BLOOM the slow swell of the low hum after the strike, OMBAK the gong's own beating. KEMPUL plays a smaller hanging gong at K V/OCT (or two octaves above the gong). SUB outputs only the fundamentals, for a sub-bass channel. DAMP is a hand on the gongs.

**Gangsa (20 HP): the Balinese metallophone pair.** Two players, POLOS and SANGSIH, each with V/OCT and GATE. Every note sounds as an umbang + isep pair tuned apart, so it shimmers at the OMBAK rate (Hz, the same at every pitch, as on a real set). KANTILAN adds the octave-up pair. DAMP is how hard the hand chokes the previous note (on the next note, or on gate fall). SET adds small seeded tuning errors (right-click: New set). Patch Irama's two-channel ombak outputs straight into the V/OCT jacks.

**Reyong (22 HP): kettle gongs, Java bonang or Bali reyong.** STYLE switches padded Java sticks (B plays an octave up if only A's gate is patched) and bright Bali cord sticks. STRIKE moves from the boss (pure) to the rim (clangy). BYOT chokes notes into short muted hits, CEK adds the cymbal clash, SYMPATHY lets ringing kettles ring each other. KENONG and KETHUK inputs play the two time-keeping kettles; COLO outputs them alone.

`make test-instruments` runs the instrument checks and writes `test/instruments-demo-dry.wav` (Bali kebyar and Java ladrang, both driven by the Irama engine). `python3 res/make_instrument_panels.py` regenerates the three panels.

## Licence

MIT (see `LICENSE.txt`). Fonts on the panel (Inter, Climate Crisis) are under the SIL Open Font Licence.
