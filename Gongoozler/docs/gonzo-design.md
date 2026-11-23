# Gonzo: the Gongoozler engine pushed past real gongs (design)

Second module in the Gongoozler plugin (plugin 2.1.0). Same engine file, same four types.
Rule 1: every new engine control has a neutral default, so the original Gongoozler module
renders exactly as 2.0.1 (checked by rendering both and comparing).
Rule 2: at its default knob positions Gonzo IS a Gongoozler tam-tam. Every extension is
"turn this knob away from its mark".

## Extensions (engine `Controls` additions, neutral value in brackets)

| Control | Range | What it does | Engine |
|---|---|---|---|
| SIZE | 2 cm .. 20 m, log | crotale-sized pings to a sheet with sub-audio modes whose wobble you hear through ROAR | size clamp widened when `beyond`; f1 clamped 0.5 Hz .. 4 kHz |
| PITCHED (switch) | off/on [type default] | any type, tam-tam included, takes its lowest mode from V/OCT (0 V = C4) | `pitchAll` |
| LAW | x0.25 .. x2 [x1] | mode-spacing exponent. Left squeezes the plate modes together towards a harmonic-ish stack (x0.5 on a tam-tam = string/pipe spacing), right spreads them like a bar or glass | plate ratio -> ratio^law |
| ORDER | -1 .. +1 [0] | left pulls every mode onto the nearest whole harmonic (singing, organ-like gong, clusters beat on each harmonic); right scatters them at random (up to about +-6 semitones) | log-lerp to round(ratio); or ratio*exp(0.35*order*jit) |
| DENSITY | 3 % .. 100 % [100] | how many of the modes exist. Few = bell/bar with a handful of tones (and less CPU) | per-mode random threshold, partials and first 4 always kept, twins follow partner; gain x density^-0.25 |
| BEAT | 0 .. 3 [as before to 1] | past 1 the pairs detune into a chorus/cluster | same formula, wider |
| MALLET | -0.5 .. 1.5 [0..1 as before] | 13 us click up to a 0.2 s shove (pure sub thump) | same formula |
| BOW | 0 .. 1 [0] | continuous rubbing (noise) excitation at the strike position: swells and drones without hits | sA += bow^2 * noise * k |
| EXCITE (audio in) | +-5 V [0] | any audio drives the plate: the gong as a resonator/reverb for drums, voices, feedback | sA += ext * k |
| DECAY | -0.4 .. 1.5, top = freeze [0..1] | 0.25 s blips up to minutes, fully right = never decays | lamOwn = 0 when frozen |
| TILT (was DAMP) | -1 .. +1 [0..1 as before] | right: highs die first (real). Left: lows die first, leaving glassy highs hanging | damp<0: T60 = Tlow/(1 + fx/f) |
| BLOOM | -1 .. +1 [0..1] | right: energy climbs (real bloom). Left: energy SINKS, a bright hit collapses into a swelling sub hum | bloom<0: band i hands to band i-1, same rates, still energy-conserving |
| BUILD | 5 ms .. 20 s [60 ms .. 2.4 s] | instant crash to glacial bloom | wider Tb |
| THRESH | x0.01 .. x100 [x1] | how hard a hit must be before it blooms: left blooms at a whisper, right never | eth multiplier |
| GLIDE | +-36 st [+-7] | laser zaps and dive bombs | same |
| ROAR | 0 .. 2 [0..1] | up to 4x the stretch force | same, SMAX already bounds it |

Safety (only when `beyond` is set, so the original is untouched): BOW/EXCITE with frozen decay
would pump energy forever, so total energy above 60 (a 1.5-velocity hit is about 5-10) gets
extra damping 20*(E/60 - 1) per second. Strike weights are refreshed when POSITION moves while
BOW/EXCITE is active. `idle` must stay false while BOW > 0 or EXCITE is non-zero.

## Panel: 36 HP, 8 columns, same look as Gongoozler

- Row GONG: TYPE, SIZE, THICK, LAW, ORDER, DENSITY, BEAT, CAST
- Row STRIKE & RING: POSITION, MALLET, BOW, DECAY, TILT, GLIDE, ROAR, HIT
- Row CASCADE & OUT: BLOOM, BUILD, THRESH, TUNE, MUTE, WIDTH, LEVEL, PITCHED switch
- CV row: V/OCT, SIZE, LAW, ORDER, POSITION, MALLET, BOW, DECAY
- IO row: BLOOM cv, GLIDE cv, TRIG, VEL, HAND, EXCITE, OUT L, OUT R
- Knobs with a real-world zone get a small tick/"real" marking where Gongoozler's range sits.

## Right-click starting points (the range, not gongs)

1. Gongoozler tam-tam (everything at its mark)
2. Sinking sun: BLOOM -0.8, hard stick, bright hit that collapses into sub
3. Glass organ: ORDER -1, PITCHED, DENSITY 40 %, freeze-ish decay (play chords of one module per note, or melodies)
4. Bowed sheet: 8 m, BOW 0.5, DECAY long, ROAR 1.2, no hits needed
5. Laser gong: opera, GLIDE +30, MALLET click, short decay
6. Crotale rain: 6 cm, DENSITY 15 %, LAW x1.6, TILT negative
7. Whale: 15 m, LAW x0.5, BOW 0.3, BLOOM -0.5
8. Resonator: for EXCITE (drums in), DECAY medium, BLOOM 0.3

## Checks to add to test/render.cpp

- original module: 2.0.1 engine vs new engine, default new controls -> identical samples (same flags)
- Gonzo: 300 random patches over the full extended ranges incl. BOW + freeze + EXCITE noise for 20 s: finite, <= 10 V, energy bounded
- sink: BLOOM -0.8 hard stick hit, centroid must fall over 2 s
- ORDER -1: modes within 1 cent of whole harmonics
