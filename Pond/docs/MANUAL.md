# Pond: frog and toad chorus

Pond is a pond full of frogs. Up to 16 of them sit at fixed places in the water,
each with its own size, temper and courage. They call, they listen to each other,
they answer, and when something scary arrives (the **HUSH** input) they all go quiet
at once. Then, one by one, the bravest start again: a quiet, short call, a long wait,
another, until the others take heart and the chorus is back.

No samples. Each frog is a small physical model: vocal cords (a whistle oscillator
and/or a glottal buzz) driving the resonances of the throat and vocal sac, cut into
pulses and notes the way real species cut their calls.

## Panel

### FROGS
| Control | What it does |
|---|---|
| **SPECIES A** | The species most of the pond is (16, list below). |
| **B** | A second species. |
| **MIX** | How many frogs are species B. Frogs change one at a time, spread over the pond, and only between calls. |
| **FROGS** | 1 to 16 frogs. |
| **HUSH** button | A predator. Press: the pond falls silent within 40 ms. Hold: it stays silent. Let go: the recovery starts. |
| **CALL** button | The keenest quiet frog calls now. Others may answer. |

### VOICE
| Control | What it does |
|---|---|
| **PITCH** | ±12 semitones for every frog (V/OCT adds to it). |
| **PULSE** | Pulse/trill rate x0.5 to x2. Whistling species (peeper, coqui, midwife toad) have no pulses and ignore it. |
| **LENGTH** | Note length x0.25 to x4. |
| **ROUGH** | Centre = the species' own voice. Left: cleaner. Right: raspy, wobbly, breathy. |
| **VARIETY** | How different the frogs are: size (big frogs are lower and slower), tempo, loudness. 0 = clones. |
| **SPACE** | How wide and far away the pond is: stereo spread, distance (quieter, duller) and night air (a little reverb). |

### CHORUS
| Control | What it does |
|---|---|
| **TEMP** | 5 to 30 °C. Warm frogs pulse and call faster (about twice as fast every 11 °C) and slightly higher. Below about 10 °C they go quiet; at 5 °C nobody sings. |
| **BUSY** | How keen the pond is to sing. 0 = silent except for CALL. |
| **BOUTS** | Excitement is contagious: hearing others makes a frog keener, but singing tires it. Up: the pond swells and dies away in waves. Down: a steady chorus. |
| **TALK** | The big one. **Centre**: frogs ignore each other. **Right (ANSWER)**: a frog will not start over a loud neighbour; it waits for a gap, and when a neighbour finishes, a nearby frog may answer it a moment later. Call and response, antiphony. **Left (SYNC)**: when one frog starts, nearly-ready frogs jump in with it, and the pond locks into step. |
| **RETURN** | 2 to 60 s: how long the frogs stay frightened after a HUSH. The bravest return first, with short quiet calls. Every call they hear gives the others courage, so the return speeds up once it starts. |

### Jacks
**Inputs** (CVs add to the knob; 10 V = full turn, TALK ±5 V = full sweep)

- **V/OCT**: transposes the frogs. Polyphonic: channel n goes to frog n.
- **PULSE, LEN, TEMP, BUSY, BOUTS, TALK, MIX, SPACE**: CV for those knobs.
- **CLOCK**: when patched, a frog that is ready to call waits for the next clock edge. With 16ths from your sequencer the pond plays on the grid but still chooses who calls when, and still answers.
- **CALL**: a trigger makes the keenest quiet frog call. Polyphonic: channel n makes frog n call (play the pond from a sequencer).
- **HUSH**: a trigger scares the pond silent; a gate keeps it silent while high.

**Outputs**

- **TRIG** (poly): a trigger when each frog starts a call.
- **GATE** (poly): high while each frog is calling.
- **PITCH** (poly): each frog's pitch as V/OCT from C4 (a spring peeper is about +3.5 V, a bullfrog about -0.1 V). Transpose down and use it to play other voices from the pond.
- **ENV**: overall loudness of the pond, 0 to 10 V. Good for ducking or opening a filter.
- **L, R**: audio. With only L patched, L is a mono mix.

The screen shows species A and B, how many frogs are calling, the temperature and a map
of the pond (left/right as you hear it, near at the bottom); each dot lights as its frog calls.
Green dots are species A, amber are B. Right-click: **New pond** moves the frogs and makes new individuals
(the pond is saved with the patch).

## Species
| # | Species | Call |
|---|---|---|
| 1 | Spring peeper | Single rising whistle, about 2.9 kHz |
| 2 | American toad | Long sweet trill, 45 pulses/s, several seconds |
| 3 | Gray treefrog | Short musical trill, about 22 pulses/s, two bands |
| 4 | Bullfrog | Deep "jug-o-rum", 2 to 5 low buzzing notes |
| 5 | Green frog | Plucked banjo "gunk" |
| 6 | Wood frog | Duck-like clucks |
| 7 | Chorus frog | Rising "crreeek", pulses speed up (a finger along a comb) |
| 8 | Coqui | Two notes, "co-QUI" |
| 9 | Natterjack | Loud rolling ratchet |
| 10 | Tree frog | Rapid "krak-krak-krak" |
| 11 | Fire-belly | Soft, sad "oop" |
| 12 | Midwife toad | Bell-like "poo" |
| 13 | Leopard frog | Slowing snore |
| 14 | Common toad | Quiet "qwark-qwark" |
| 15 | Common frog | Soft purring croak |
| 16 | Marsh frog | Loud rattling laugh |

Values are typical field-guide figures at 20 °C (dominant frequency, pulse rate, note length,
call rate); the models aim for the character of each call rather than a scientific reproduction.

## Patch ideas

1. **The heron.** Patch a slow random gate (Marbles, a 1-in-16 chance from Pam's) into **HUSH**.
   Set RETURN to 10 to 20 s and TALK right. The pond falls silent, then rebuilds on its own: free arrangement.
2. **Frog percussion on the grid.** 16ths into **CLOCK**, Tree frog or Wood frog, TALK about 3 o'clock, BUSY high.
   The frogs fill the gaps between each other like a shaker part that never repeats. Kick into HUSH for
   ducking that sounds like the frogs are scared of the kick.
3. **Frogs play the synth.** **TRIG** and **PITCH** (poly) into a poly envelope and oscillator, PITCH
   through an octave-down offset and a quantiser. Each frog becomes a note, and call-and-answer becomes a melody.
4. **Chorus as a sidechain.** **ENV** into a VCA or filter on a pad, so the pad breathes with the pond.
5. **Seasons.** A very slow LFO on **TEMP**: from cold, sparse and slow to warm, fast and dense.

## Tips
- TALK fully right with 3 to 5 frogs is the clearest call and response. Many frogs at SYNC gives the
  pulsing "wall" of a big tropical chorus.
- With BOUTS high and BUSY low, the pond is mostly quiet and erupts now and then.
- CALL during a long RETURN breaks the silence early and the others follow.
