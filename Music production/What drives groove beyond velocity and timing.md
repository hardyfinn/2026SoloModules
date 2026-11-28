---
title: What drives groove beyond velocity and timing
date: 2026-11-10
source: Music production · Explore groove
topic: Music
tags: [groove, rhythm, swing, sequencing]
summary: |
 Groove comes from making hits differ on purpose, not only in velocity and timing.
 A 10-page field guide covers nine factors with numbers to start from; its text is included.
---

# What drives groove beyond velocity and timing

## Summary
Groove comes from making hits differ on purpose, not only in velocity and timing.
A 10-page field guide covers nine factors with numbers to start from; its text is included.

## Question
> Lets start with the basics. I guess velocity and timing are the key drivers of groove. What else?

## Answer
Yes, velocity and timing are the big two. Here are the others that do just as much work:

1. **Note length (gate/decay).** A short, clipped hat and a hat that rings into the next step feel completely different even with the same timing. Changing open/closed hats and gate length per step is a lot of the Detroit bounce.
2. **Tone per hit, not just level.** On real drums and good machines, a harder hit is also brighter and snappier. If your accent only turns up the volume, it sounds flat. Patch the accent to filter cutoff or decay too (on the System80 810, the Zadar envelopes, or Three Sisters cutoff).
3. **Which steps are filled.** Placement, syncopation and ghost notes. A quiet 16th just before the snare, or a kick that skips beat 3, often does more than any swing setting.
4. **Interplay between parts.** Groove lives in the gaps between instruments: hats answering kicks, the bassline avoiding the kick, a percussion line that fills holes. One part on its own rarely grooves.
5. **Envelope shape and pumping.** A slow attack sounds late even when it's on time, and sidechain ducking creates a breathing pulse. Your sidechain is already a groove tool, not just a mixing fix.
6. **Cycle lengths.** A 3- or 5-step part against a 4/4 kick shifts its accents every bar (polymeter). That's the easiest way to get Autechre-style movement without randomness, and Pam's or Metropolix can do it in seconds.
7. **Change over time.** A groove that repeats exactly gets tiring. Small variations every 4 or 8 bars (an extra ghost note, accents moving, swing opening up) keep it alive, and that's closely linked to your loop-fatigue problem.

A quick test you could try tonight: take one hat loop and leave timing alone. Change only the note length and the accent-to-filter amount, and listen to how much the feel moves.

Want me to go deeper on one of these, or turn a few into a patch for your rig?

## Question 2
> Please create a pdf looking more deeply at groove including velocity, timing and your suggestions above. About 10 pages long.

## Answer 2
Here's the guide: groove-field-guide.pdf, exactly 10 A4 pages.

It covers swing (with a swing-to-milliseconds table at 122 to 135 BPM), push and pull per instrument, velocity levels, note length, tone per hit, ghost notes and syncopation, how parts play off each other, sidechain pumping, parts with odd lengths (polymeter), and using groove changes to shape an arrangement. Each section has step-grid diagrams and an "In your rig" box for Live, VCV and your modules. It ends with five exercises and a checklist for when a loop feels dead.

If you want more depth anywhere, or a section turned into a VCV patch or Live set to try, just say.

### Full text of the PDF guide

```text
A FIELD GUIDE FOR THE EP

Groove:
what makes it move
Velocity and timing are the two drivers everyone names.
They're only part of the story. This guide goes through nine
things that decide whether a loop pulls you along or just
sits there. Each one comes with numbers to start from and
a way to do it in Live, VCV Rack or the modular.

01 Timing: the grid and swing

06 Placement, syncopation, ghosts

02 Microtiming: push and pull

07 Interplay between parts

03 Velocity and accent

08 Pumping and envelope shape

04 Note length

09 Cycle lengths (polymeter)

05 Tone per hit

10 Change over time, and exercises

THE ONE IDEA TO HOLD ON TO
Groove is difference. A loop where every hit is the same length, loudness, tone and distance from
the grid has no groove, however good the sounds are. Every technique here is a way of making hits
different from each other, on purpose, in a pattern the ear can learn.

01 · TIMING

The grid, and how swing bends it
Almost all techno timing comes back to the 16th-note grid. Swing is the simplest way to bend it. It takes
every second 16th in each pair and pushes it later. The first 16th of each pair stays put, so the
downbeats stay solid while the off-beats lean back.
Swing is usually given as a percentage, as on MPCs and in many sequencers. It describes where the
second 16th lands inside the 8th note. 50% is dead straight. 66% is a full triplet shuffle. Everything
between is a sliding scale from mechanical to loping.

beat 1

beat 2

Straight 50%
Light 54%
Medium 58%
Heavy 62%
Triplet 66%

Two beats of 16ths. The black dot stays on the grid. The orange dot (the off-16th) moves later as swing increases. The
hollow circle shows where it would land when straight.

Swing in milliseconds
The same percentage moves things by a different number of milliseconds at different tempos. The table
shows how late the swung 16th lands, in ms, at techno tempos.

BPM

16th note

54%

56%

58%

62%

66%

122

123 ms

9.8

14.8

19.7

29.5

39.3

125

120 ms

9.6

14.4

19.2

28.8

38.4

128

117 ms

9.4

14.1

18.7

28.1

37.5

132

114 ms

9.1

13.6

18.2

27.3

36.4

135

111 ms

8.9

13.3

17.8

26.7

35.6

Where to start
52 to 56%: the "is it even swung?" zone. Adds
life without sounding shuffled. A good default
for driving Detroit-style tracks.
57 to 61%: an obvious bounce. House-y, funky,
works great on hats and percussion.
62 to 66%: shuffle territory. Use on one part
against a straighter kick rather than on
everything.

The important trick
Don't swing everything by the same amount. A
straight kick and bass under swung hats and
percussion creates tension between the two
layers. That tension is a big part of what makes
classic Detroit records feel urgent rather than
lazy.

02 · MICROTIMING

Push and pull: placing each part on its own clock
Swing is a pattern applied to every other 16th. Microtiming is different: it's a fixed offset for a whole
instrument. Musicians do this naturally. A drummer plays the snare a touch late for a heavy, laid-back
feel, or rides the hats slightly ahead to drive the song. Hip hop producers from the MPC era built whole
styles out of a snare that drags. Ethnomusicologist Charles Keil called these tiny disagreements
between players "participatory discrepancies". He argued they are where the feel lives.

the grid
Kick
Bass
Closed hat
Ride / shaker
Clap / snare
Ghost perc
Chord stab
-20 ms
-10 ms
◀ early = pushing, urgent

0

+10 ms

+20 ms
late = laid back, heavy ▶

A starting map for an up-tempo track. The kick is the anchor at zero. The shaded bands show how much random wobble
each part can take before it sounds sloppy.

Rules of thumb
The kick is the anchor. Keep it on the grid and tight. Everything else is heard relative to it.
Early hats (2 to 6 ms) make a track feel urgent and forward-leaning. Late claps and snares (4 to
10 ms) make it heavier and more swaggering.
Under about 10 ms you feel an offset rather than hearing it as a separate event. Above about 20 to
30 ms it starts to sound like a flam or a mistake. Groove lives in that first zone.
Fixed offset beats random wobble. Random timing on its own just sounds loose. A consistent
offset, plus a little randomness on top, sounds like a player.

IN YOUR RIG
Live: each track's Track Delay box (under the mixer; show it from the mixer section view options)
shifts a whole track in ms, positive or negative. That's the cleanest way to set a clap 7 ms late. In
the Groove Pool, Timing sets how strongly a groove pulls notes and Random adds per-note timing
wobble. Use only a few percent of Random.
Modular: clock-delay a single voice. On Pam's New Workout, use the per-output Swing and
phase/offset settings on the channel that feeds the clap. Marbles' T Jitter adds wobble to its own
triggers. Keep it low (9 o'clock) on parts that should stay tight.
VCV: put a short delay on the gate path for one voice, or use a clock module with per-output swing.
Your Randrum and Seuperclidean plugins can drive individual voices from their own clocks.
Hybrid setups: audio from VCV and the modular arrives later than MIDI-triggered sounds in Live
because of buffers and converters. Measure it once (record a click through the loop and see how
late it lands), then correct with Track Delay. Otherwise latency will choose your microtiming for
you.

03 · VELOCITY AND ACCENT

Loud, quiet and in between: building a hierarchy
Velocity creates a hierarchy: the ear finds the strong beats, the medium beats and the whispers, and
works out the shape of the bar from that. A machine gun of identical hats has no hierarchy, so the brain
stops listening to it within a few bars.

1

2

3

4

Hat

100

100

100

100

100

100

100

100

100

100

100

100

100

100

100

100

84

118

55

Flat: 16 hats, all at velocity 100. Technically fine, rhythmically dead.
1

2

3

4

Hat

45

0

92

0

45

0

122

0

45

0

90

0

48

Three levels. Ghosts (around 40 to 55) on the downbeat 16ths, normals (around 85 to 95) on the off-8ths, accents (115+)
on the "and" of 2 and 4. Now it has a shape.

Three or four levels is enough
Accent (110 to 127): one or two per bar. These are the hits you'd tap your foot to.
Normal (80 to 100): the body of the pattern.
Ghost (30 to 60): felt more than heard. They fill gaps and make the pattern "roll". Section 06 covers
them.
Humanise (±3 to 8 on everything): small random variation on top so repeats are never identical.

ACCENT THE OFF-BEATS
In four-on-the-floor music the kick already owns the downbeats. Putting the hat accents on the offbeats (the "ands") gives the bar a second layer of pulse, and that's where most of the bounce comes
from.

IN YOUR RIG
Live: in the MIDI editor, draw velocities directly or use Velocity Deviation and note Chance. The
Velocity MIDI effect with Random at 5 to 10 is a quick humaniser. Groove Pool Velocity applies a
groove's accents too.
Modular: there's no velocity on a gate, so make accent a separate CV. Use a second Pam's output,
a Metropolix mod track, or a DROID algoquencer accent output, and send it to a VCA level or an
envelope amount. Your drum voices' accent inputs are made for exactly this.

04 · NOTE LENGTH

Note length: the hidden half of articulation
Timing tells you when a sound starts. Length tells you when it stops, and the ear cares about both.
Funk guitarists, bass players and the best drum programmers think about the gap after a note as much
as the note itself. A short, choked note leaves silence. That silence lets the next hit land with more
weight.

trigger

trigger

trigger

Short, snappy

Long, ringing

Slow attack

sits on the grid, leaves space

fills the gap, smooths the groove

peak lands late: sounds behind

The same trigger, three shapes. A slow attack doesn't just change the tone. It moves the perceived hit later, so a pad or
bass with a soft attack sounds behind the beat even when it's perfectly on time.

Where length matters most
Hats. Swapping closed and open hats is the classic version. Open hat on the off-beat, choked by the
next closed hat, is the foundation of countless Detroit and Chicago records. Programming the decay
per step is the modern version.
Bass. Staccato bass (gate 20 to 40%) bounces. Legato bass (gate 90 to 100%, or sliding) drives and
rolls. Mixing them in one line, with short notes on the off-beats and one long note held across the bar
line, is a very funky move.
Chord stabs. Short stabs (a 16th or less) are percussive and rhythmic. Long chords become
harmony and stop contributing groove. Pick one deliberately for each part.

Gate length as a groove parameter
Most sequencers can set gate length per step. Treat it as being as important as pitch. One useful
pattern is a "long-short-short-long" cycle across 16ths on a bassline. It forms a rhythm of its own on top
of the note rhythm.

IN YOUR RIG
Live: select notes and drag their ends, or use the MIDI tools to set length. Try a Drum Rack hat
with Choke groups so an open hat gets cut by the closed one.
Modular: Pam's Width per output sets gate length. Metropolix sets gate mode and length per
stage. Sending one Zadar envelope's decay time a random or sequenced CV gives you step-by-step
length changes on any voice. On Just Friends in transient mode, the TIME and INTONE controls set
decay across all six outputs at once.
VCV: any ADSR plus a sample-and-hold of random CV into the decay input does the same thing.

05 · TONE PER HIT

Tone per hit: louder should also mean brighter
Hit a real drum harder and it doesn't only get louder. It gets brighter, sharper and slightly higherpitched, with more attack. Your ear knows this, so a sample turned up in volume alone sounds like a
fader move, not a harder hit. Linking accent to timbre is one of the quickest ways to make programmed
drums sound played.

What to modulate per hit
Parameter

Accent does

Starting amount

Filter cutoff

Opens on harder hits

Velocity → cutoff 20 to 40%

Decay

Longer on accents, shorter on ghosts

Ghosts about 50% of accent decay

Pitch

Slight upward bump

+10 to +40 cents, or a tiny pitch envelope

Drive or fold

More harmonics

Just enough to hear; 10 to 20%

Sample start

Later start on ghosts = softer attack

Ghosts start 5 to 15 ms in

Round-robin: fixing the machine gun
Even with velocity and tone linked, the same sample on every 16th gets tiring. Real players never hit
the same spot twice. Use two to four slightly different hats (or one hat through slightly different
settings) and rotate them. In Live, put the variations in a Drum Rack or Simpler's slices and alternate
with a Random MIDI effect, or with a pattern on another note lane.

1

2

3

4

Accent
Hat A
Hat B

Two hat sources, split by role. Hat A is brighter and plays the 8ths. Hat B is duller with a softer attack and plays the inbetween 16ths as ghosts. The accent row opens the filter on both.

PATCH: ACCENT THAT CHANGES THE TONE (MODULAR)
1. Pam's output 1 → gate into your hat or percussion voice.
2. Pam's output 2, set to a sparser pattern (Euclidean 3 of 8, say) → Zadar trigger. Zadar's envelope
→ Three Sisters cutoff CV, with the hat audio running through Three Sisters.
3. Set the Three Sisters cutoff about 30% lower than you'd normally leave it, so non-accented hits
sound dull. The envelope opens it on accents.
4. Optional: also take that envelope to Cold Mac and use it as a little extra VCA level. Now accents
are louder and brighter together, just like a real drum.
Variation: instead of a fixed Euclidean, use Marbles' X output (smoothed random) into the cutoff
and sample it per hit. Every hat gets its own tone, and Marbles' Deja Vu lets you lock a "good"
pattern of tones once you hear one you like.

06 · PLACEMENT, SYNCOPATION AND GHOST NOTES

Which steps are filled, and which aren't
Before any swing or velocity, the notes themselves set most of the groove. Two ideas do the heavy
lifting: syncopation (putting weight where you don't expect it) and ghost notes (quiet hits that fill the
spaces).

1

2

3

4

4/4
Skip
Push
Break

Four kick ideas. "Skip" drops beat 3 and moves the kick to the off-beat before it, giving a hiccup. "Push" adds a 16th that
anticipates the next bar. "Break" is a broken pattern in an electro or breakbeat style.

Syncopation in practice
Anticipation: move a note a 16th before the beat. A bass or stab that lands on the last 16th of a bar
and ties over feels like it's leaning forward into the next bar.
Missing beats: removing a hit where the ear expects it is as strong as adding one. Dropping one
kick every 4 or 8 bars is a classic move.
Off-beat emphasis: chord stabs on the "e" or "a" of the beat (the 2nd or 4th 16th) against a four-onthe-floor kick are a core Detroit sound. Think of the off-kilter stabs and strings on classic Transmat
and KMS records.

Ghost notes
Ghosts are quiet in-between hits, usually 16ths, at velocity 30 to 60. You barely hear them, but take
them out and the groove goes stiff. They work best on the 16ths next to an accent: a ghost just before a
clap, or between two hats. They also give swing something to act on. A pattern of only 8th notes can't
swing at 16th resolution, because there's no off-16th to push.

1

2

3

4

Kick
Clap
Hat
Perc

A full bar with ghosts (hollow), normal hits (black) and accents (orange). The perc line fills gaps the hats leave and never
doubles them.

IN YOUR RIG
Probability is the easy way to get ghosts that come and go. In Live, set note Chance to 30 to 60%
on ghost notes only. On Pam's, use per-output probability. On DROID, an algoquencer with a low
"fill" setting adds sparse extras around a fixed pattern. The core pattern stays fixed while the extras
move around.

07 · INTERPLAY · 08 · PUMPING AND ENVELOPE SHAPE

Groove lives between instruments
A bassline that sounds boring on its own can be perfect against the kick. A hat pattern that grooves
alone can turn to mush when the percussion lands on the same steps. Groove belongs to the whole kit,
so write parts as conversations.

1

2

3

4

Kick
Bass
Hat
Stab

Interlocking: the bass avoids the kick steps and lands in the gaps. The hats' accents fall where the bass is silent. The stab
answers the bass rather than doubling it. Nothing fights for the same 16th.
Avoid doubling. If two parts hit the same step, one should be much quieter or a very different
sound.
Call and response. One part plays a figure in bar 1, another answers in bar 2. Funk, soul and dub
are built on this. It's also an easy way to make a 2-bar loop feel like a phrase.
Kick and bass are one instrument. Write them together. The classic techno bass is an off-beat
bass between kicks. Rolling 16th basslines that skip the kick steps are another.

Pumping: sidechain is a groove tool
Sidechain ducking does more than make space for the kick. It gives every sustained part a rhythmic
shape, so pads and bass swell on the off-beat. That swell is a groove of its own, and its shape matters.

kick triggers

pad / bass level after ducking

The release time sets the feel. A short release (about 60 to 100 ms) gives a subtle pulse. A release that recovers right at
the off-beat (about half a beat, roughly 230 ms at 130 BPM) makes the classic pump that peaks on the "and".

IN YOUR RIG
Live: Compressor with sidechain from the kick, ratio 4:1 or more, attack 0.1 to 1 ms. Set release by
ear until the swell peaks on the off-beat. For a more musical shape, a ghost kick works well: a
muted track with a simple kick pattern feeding the sidechain. Then you can duck on steps where
the audible kick doesn't play.
Modular: kick trigger → Zadar envelope (sharp attack, shaped decay) → inverted into a VCA's CV
on the pad/bass. Cold Mac can do the inversion and the crossfade. Because the envelope shape is
yours to draw, you can make swells that a compressor can't.

09 · CYCLE LENGTHS

Polymeter: the cheapest way to make a loop evolve
If every part repeats every 16 steps, the whole bar repeats exactly and loop fatigue sets in fast. Set
some parts to odd lengths (3, 5, 7, 11, 13 steps) and their accents line up differently against the kick
every time round. The pattern stops being a 1-bar loop and becomes a much longer cycle, without any
randomness.

Kick 4
Perc 3
Hat 5
Bass 7

Three bars. The kick cycles every 4 steps. The perc cycles every 3, so its accent shifts against the kick every beat. The
hat cycles every 5, the bass every 7. All four only line up again after 420 steps, which is about 26 bars.

How long until it repeats?
Combined cycle length is the lowest common multiple of all the lengths. 16 and 3 → 48 steps (3 bars).
16, 3 and 5 → 240 steps (15 bars). 16, 5 and 7 → 560 steps (35 bars). Add one more odd length and the
loop practically never repeats.

Keep it musical

In your rig

Keep kick, and usually bass, on 16 or 32
steps. They're the anchor the odd lengths play
against.

Live: set a clip's loop length to something like
3/16 or 5/16. Session clips of different lengths
loop independently.

Use odd lengths on percussion, hats, stabs,
modulation.

Metropolix: each track has its own length.
Try track 1 at 8 stages and track 2 at 7.

Start with one odd-length part. Two is already
rich. Three or more drifts toward the Autechre
end, which is great when that's what you want.

Pam's: Euclidean with a length other than 16
on one output.

Reset everything every 8, 16 or 32 bars if you
want sections to feel phrased. Leave it freerunning if you want endless evolution.

DROID: set different step counts per
sequencer circuit.
Modulation: a 5-step S&H sequence on filter
cutoff, against 16-step drums, adds movement
even when the notes are fixed.

AUTECHRE LINK
The "self-mutating" feel you like in Autechre often comes from exactly this: several cycles of
different lengths, plus rules that change one cycle when another reaches a certain point. Polymeter
is the first step. Logic (when part A plays, make part B skip) is the second, and your DROID is built
for it.

10 · CHANGE OVER TIME

Groove as arrangement
A groove that's exactly the same for six minutes gets tiring, however good it is. The groove itself can be
an arrangement tool, which helps with loop fatigue. You don't always need new parts. Often you just
need the same parts played differently.

groove density: swing depth, ghost notes, accent spread

Intro
16 bars

Build
16 bars

Main A
32 bars

Break
16 bars

Main B
32 bars

Outro
16 bars

Change the groove's density across a track: less swing, fewer ghosts and narrower accents in the intro and breaks; more
of all three in the main sections. Listeners hear the energy change even though the notes are almost the same.

Five groove moves for arrangement
1. Open the swing: automate Live's Groove Pool Global Amount, or Pam's swing via CV, from 52% to
58% across a build.
2. Add the ghosts: raise ghost note chance from 0 to 60% over 16 bars. The groove fills in without
anything new appearing.
3. Lengthen the hats: automate the hat decay. Short in verses, open in peaks.
4. Drop the anchor: take the kick out for 1 or 2 bars before a section change. The return feels huge.
5. Reset the polymeter: force all odd-length parts back to step 1 on the downbeat of a new section.
Everything snaps together and the listener feels the structure.

Exercises to try
Exercise

What to do

Listen for

Length
only

One hat loop. Leave timing alone and change only note
length and accent-to-filter amount.

How much the feel changes
without moving a single note.

Swing
sweep

Loop kick + hats. Sweep hat swing from 50% to 66% while
the kick stays straight.

Find the point where it starts to
"lope". Note the % for this tempo.

Late clap

Move the clap later in 2 ms steps up to 14 ms using Track
Delay.

Where it gets heavier, and where
it starts to flam.

Odd perc

Add one 5-step percussion part to a 16-step loop.

How long it takes before you
notice repetition.

Groove
steal

Drag a favourite Detroit or funk record's drum loop into
Live's Groove Pool (Extract Groove) and apply it to your
own drums.

Which part (timing or velocity)
carries the feel.

Quick checklist when a loop feels dead
☐ Are all hits the same velocity? ☐ Are all hits the same length? ☐ Does accent change the tone? ☐ Are kick

and bass fighting? ☐ Is anything off the grid on purpose? ☐ Is there at least one ghost note? ☐ Does anything
run at a different length? ☐ Does anything change after 8 bars?
```

## Sources
- Project file: groove-field-guide.pdf (text reproduced above)

Related: [[Pentatonic scales and major and minor]], [[Evolving patterns with Ableton MIDI tools and grooves]]
