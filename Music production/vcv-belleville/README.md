# Belleville

A house and techno trio for VCV Rack, with no drums: **Strata** plays the chords and leads the harmony, **Undertow** plays the bass line and **Signal** plays the melody. Put them side by side, in any order, and they share one chart, key, clock and swing with no cables between them (the small green LINK light shows they have found each other). All three send notes out, so you choose the sounds (House Keys, your own voices, or out to hardware through the disting NT).

Licence: MIT. Version 2.0.0, untested in Rack so far.

## Quick patch

1. Put Strata, Undertow and Signal next to each other.
2. Clock: a sixteenth-note clock into Strata's CLOCK (right-click for 24 or 48 PPQN, for example from Live via CV Tools). Reset into RESET.
3. Strata V/OCT, GATE (both polyphonic) into a poly voice, for example House Keys, or VCV's VCO into ADSR and VCA with the poly cables.
4. Undertow V/OCT and GATE into a bass voice. ACC can open a filter envelope further, and SLIDE goes to a 303-style slide input if your voice has one (Undertow already glides its own V/OCT on slides).
5. Signal V/OCT and GATE into a lead voice; MOD (contour, 0-10 V) into its filter cutoff.
6. Optional: your kick's trigger into Undertow's KICK, so the bass learns where the kick is and keeps out of its way. Unpatched, Undertow assumes four to the floor.

## Strata (24 HP)

| Control | What it does |
|---|---|
| STYLE | 16 styles: Detroit, Chicago, Deep, Garage, Dub, Warehouse, Acid, Minimal, Electro, Ambient, IDM, Piano, Disco, Progressive, Melodic, Soulful |
| KEY, MODE | Key and scale. MODE "auto" uses the mode each progression was written in |
| PROG | The style's progressions (93 in all), named on the screen |
| RHYTHM | pad, stab on one, offbeat, Chicago 3-3-2, push, skank, dub, broken |
| VOICING | close, drop 2, open, rootless (good with a bass), chord memory (one shape moved in parallel, rave stabs), shell |
| COLOUR | plain triads at the left, as written at noon, extra 9ths, 11ths and 13ths to the right |
| SPREAD | lifts notes an octave for a wider chord; over 90 % it doubles the root below |
| LENGTH | how long each chord lasts: half, as written, double, x4 |
| DRIFT | once a loop, a chord may move to a relative (the relative major, up a fourth, suspended, borrowed from the next bar) and may later drift back home. The screen shows how many chords have moved |
| HOLD | stops DRIFT |
| NEXT | moves to the next progression at the end of the loop |
| CHORDS in | poly V/OCT, for example chords from the Hapax. Strata voices and plays them in its rhythm and the whole trio follows them |
| ENERGY in | 0-10 V. Low energy thins the optional chord hits, bass notes and melody notes; high energy fills them in. Patch a slow LFO or Long Form here for builds |
| CHANGE out | a trigger on every chord change |

Right-click: clock PPQN, swing for the whole trio (straight to 63 %), chord register.

## Undertow (20 HP)

| Control | What it does |
|---|---|
| STYLE | offbeat, rolling, acid, Reese, dub, electro, Chicago, pedal, skippy (2-step), sub, broken |
| DENSITY | more or fewer notes, live |
| FOLLOW | root only at the left, chord tones in the middle, a counter-line with passing and approach notes at the right |
| OCTAVE | -2 to +1 (plus OCT CV) |
| SLIDE, ACCENT | how often notes slide into the next one or are accented |
| DRIFT | every two bars, re-rolls a few steps of the line (plus DRIFT CV) |
| SPACE | leaves room for the kick (and, lightly, for Strata's stabs) |
| NEW | a new bass line |

Right-click: slide time.

## Signal (20 HP)

| Control | What it does |
|---|---|
| STYLE | string line, arp, hook, call and answer, bleep, lead, acid line, glitch |
| RANGE | how wide the line can roam (plus RANGE CV) |
| DENSITY | more or fewer notes |
| MOTIF | motif length: 3, 4, 5, 6, 7, 8, 12, 16, 24, 32, 48 or 64 sixteenths. Odd lengths turn against the bar |
| ANSWER | plays into the gaps between the chord stabs and lands on chord tones on the beat |
| OUTSIDE | chromatic notes off the beat (and on it, past halfway) |
| MUTATE | each time round, the motif changes a little and keeps the change, so the line slowly evolves; it also drifts back towards the original now and then. The screen counts the generations |
| FREEZE | keeps this version (button or gate) |
| NEW | a new motif |

Right-click: melody register.

## Variations to try

- **Detroit strings:** Strata DETROIT, RHYTHM pad, VOICING open, SPREAD 70 %; Signal STRING LINE, MOTIF 32, MUTATE 20 %; Undertow CHICAGO, FOLLOW 60 %.
- **Warehouse rave:** Strata WAREHOUSE, VOICING chord memory, RHYTHM offbeat; Undertow ROLLING, SPACE full; Signal BLEEP, MOTIF 5.
- **Autechre drift:** Strata IDM, DRIFT 70 %; Signal GLITCH, MOTIF 7, MUTATE 80 %, OUTSIDE 40 %; Undertow BROKEN, DRIFT 60 %.

## Building and checks

`make test` runs the engine checks with no Rack needed. `./test/render_trio out.wav STYLE BASS SIG BPM BARS SEED RHYTHM VOICING DRIFT MUTATE` renders a demo with plain test synths. `python3 res/make_panels.py` redraws the panels (needs fonttools), `python3 tools/preview.py` draws panel-preview.png.
