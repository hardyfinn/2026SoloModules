---
title: Gamelan scale degrees in pelog and slendro
date: 2026-11-06
source: Music production · Weighted quantizer
topic: Music
tags: [gamelan, scales, scale-degrees, tuning]
summary: |
 Pelog and slendro also have favoured and rarer scale degrees, shaped by pathet and the seleh (goal) tones.
 A follow-up designs a weighted random-to-note quantizer; the gamelan PDF text is included.
---

# Gamelan scale degrees in pelog and slendro

## Summary
Pelog and slendro also have favoured and rarer scale degrees, shaped by pathet and the seleh (goal) tones.
A follow-up designs a weighted random-to-note quantizer; the gamelan PDF text is included.

## Question
> is there a similar analysis of gamelan scales (pelog and slendro)? i have played with these scales but suspect there are also scale degrees which are used less commonly here too.

## Answer
Your suspicion is right, though the research looks different. I found no gamelan equivalent of the Essen-style note counts. The tradition is **pathet** (mode) theory, built on which tones end phrases.

- **Pelog** has 7 tones, but a piece mostly uses 5. Pathet bem uses 1 2 3 5 6 and barang 2 3 5 6 7. **Tone 4 is the rare one**, mostly ornament, and many gamelan have no key for it. In a study of 402 pelog melodies, none used fewer than 5 tones and many used 6, so the extra tone acts as a swap-in.
- **Slendro** uses all 5 tones, but each pathet has a stressed tone, a strong one and an **avoided** one: nem 2 / 5 / avoids 1, sanga 5 / 1 / avoids 3, manyura 6 / 2 / avoids 5. "Avoided" means rarely a cadence note, not absent, which is the same "using vs landing" split as the 2nd in pop.
- **The strong beat is at the end.** Gamelan phrases come in four-beat groups that resolve on the *last* beat, where the gong falls. That's the reverse of the Western downbeat.

Mantle Hood counted phrase-ending tones in the 1950s, but scholars now treat his pathet theory as outdated.

Want me to turn this into a PDF like the last one, with Marbles recipes and disting NT tuning tips?

## Question 2
> A pdf would be great. I'm thinking about creating a new module which looks at these relative weightings. I have often used a random cv signal into a quantizer to make melodies, but adding these relative weightings to the selection of the scale note would be an improvement. how would you design a module where an random input cv would output a quantized note based on these weighting. I'd like to pick a root note and scale and have the weightings changed based on your research.

## Answer 2
Here's the 13-page PDF. The first half covers slendro and pelog; the second half designs the module, which I've called **Seleh** (the Javanese word for a phrase's landing tone).

**How it works:** a normal quantizer gives each scale note an equal slice of every volt. Seleh makes each slice as wide as that note's weight. Higher voltage still means a higher note, so your random contour survives, and an even random source hits each note exactly as often as the research says.

**Controls:**
- Root, scale (Western modes plus slendro and pelog, with editable tunings) and profile (folk, classical, rock, Detroit Dorian, and the six gamelan pathet).
- **Focus** runs from a flat, normal quantizer to the research values to an exaggerated hierarchy, with CV and an invert option.
- **Accent** uses a separate landing profile on strong beats, which can be the last beat for gamelan.
- **Step** favours small intervals.
- A **Stability** output gives the chosen note's weight as CV. Patch it to a filter so the landing notes sound brighter.

The gamelan weights are designed from the pathet roles, not measured, because nobody seems to have published note counts for gamelan.

I'd build it as a VCV plugin first, then port it to the disting NT. Want me to start on that?

### Full text of the PDF

```text
M U S I C T H E O RY · R E S E A R C H N OT E S F O R T H E E P · PA R T 2

Gamelan Weights and the Seleh
Quantizer

Which tones of slendro and pelog are favoured and avoided, and a
design for a module that turns random CV into melodies shaped by
those weightings, or by the Western ones from part 1.
1 The short answer
2 Slendro and pelog: the basics
3 Pathet: favoured and avoided tones
4 What the research can and cannot tell us
5 Weight profiles for gamelan scales
6 The Seleh module: concept

7 How it picks a note
8 Controls, inputs and outputs
9 Profile library
10 Patching ideas
11 Ways to build it
12 Further reading

Companion to Scale Degrees in Practice. The gamelan sections rely on the Wikipedia articles on pathet, pelog
and slendro and on Jay Rahn's 2016 study of pelog tunings and melodies (partly read in summary). No
published study gives Essen-style note counts for gamelan, so the gamelan weightings here are designed from
the pathet literature, not measured. Each one is marked as designed.

Gamelan Weights and Seleh · 1

1. The short answer
You suspected that some degrees of the gamelan scales are used less than others. That is right, and
in pelog it is built into the system.
Pelog has seven tones, but a piece normally uses five of them. The pathet (modes) called lima
and nem use tones 1 2 3 5 6. Pathet barang uses 2 3 5 6 7. Tone 4 is the rare one in all three: it is
used mainly for ornament, does not usually appear on the gendèr, gambang or the punctuating
gongs, and many gamelan sets do not even have keys for all seven tones.
Slendro uses all five of its tones, but each pathet gives them different roles: one stressed tone,
one strong tone, and one avoided tone. "Avoided" means rarely used as a seleh, the tone a phrase
lands on. It does not mean the tone is missing.
Gamelan phrases are end-weighted. The balungan (core melody) is grouped in units of four
beats called gatra, and the important tone falls on the last beat, where the larger gongs sound.
Western music puts its anchor on the first beat of the bar. A module that knows about strong
beats needs to let you choose which beat is strong.
The research is about landing tones, not raw counts. Western corpus studies counted every
note; gamelan theory has mostly described which tones end phrases and which are avoided
there. Mantle Hood's 1954 study did count phrase-ending tones across many pieces, but its
conclusions about pathet are no longer considered authoritative.
The second half of this document designs a module called Seleh, after the Javanese word for a
landing tone. It takes a random CV, a root, a scale and a weighting profile, and outputs a quantized
pitch in which strong tones come up more often than weak ones. It includes the research profiles
from part 1, the gamelan profiles from this part, a Focus control that makes the hierarchy stronger
or weaker, and an Accent input for the "landing" beat.

2. Slendro and pelog: the basics
Javanese gamelan uses two tuning systems. A complete gamelan has two sets of instruments, one
for each, and they are never played at the same time.

2.1 Slendro
Slendro has five tones per octave, numbered 1, 2, 3, 5 and 6. There is no 4: the numbering is kept
compatible with pelog. In Central Java the five steps are roughly equal, so each one is around 240
cents (a Western whole tone is 200 cents and a minor third 300). Because the steps are close to
equal, slendro has no strong built-in "home" interval the way a major scale does. Any tone can be
home, and the pathet decides which.

2.2 Pelog
Pelog has seven tones, numbered 1 to 7, with unequal steps. In 1963 J. Murray Barbour described it
as five small steps and two large ones. Jay Rahn's 2016 study tested that description against 406
saron tones from 32 gamelan sets, including the 30 measured by Wasisto Surjodiningrat and

Gamelan Weights and Seleh · 2

colleagues in 1972, and found that it held in every case. One common approximation, used in
Figure 1, treats pelog as a subset of a 9-tone equal division of the octave: steps of about 133 cents,
with the two large steps at about 267 cents.
One octave in three tuning systems (cents)
12-TET (Western)
Slendro (≈ equal)
og (9-TET approx.)

1

♭2

2

1

3

2

1

0

♭3

2

100

4

5

3
3

200

♯4

400

500

6

♭7

5
4

300

♭6

5

600

1

6
6

700

7

800

1

7

900

1

1000

1100

1200

Figure 1. Slendro divides the octave into five roughly equal steps of about 240 cents. Pelog has seven unequal steps, five
small and two large; here it is approximated as a subset of 9-tone equal temperament. Real gamelan sets differ from both,
and from each other.

No standard tuning
Every gamelan set is tuned individually, and as Jennifer Lindsay puts it, "no two gamelan sets will have
exactly the same tuning, either in pitch or in interval structure." The tunings in Figure 1 are rough guides
only. For your module, this means the tuning should be editable, ideally by loading a Scala file, rather than
fixed.

2.3 Bali
Balinese gamelan also uses pelog-type and slendro-type tunings, but in a different way. The most
common Balinese mode, selisir, is a five-tone subset of pelog, made from groups of three and two
adjacent tones separated by gaps. It is the tuning of the popular gamelan gong kebyar and can be
considered the "default" pelog in Bali. Balinese instruments come in pairs tuned slightly apart, so
that each pair beats at a steady rate, giving the shimmering sound known as ombak (wave). Tunings
vary even more in Bali than in Central Java. Ensembles from the same village may be tuned very
differently.

3. Pathet: favoured and avoided tones
Pathet is often translated as "mode". Each tuning system has three pathet, and each pathet is linked
with a time of night in the shadow-puppet theatre (wayang kulit): in slendro, nem comes first, then
sanga, then manyura. Pathet affects which tones are emphasised, which are avoided, where
phrases end, and how the elaborating instruments (rebab, voice, gendèr) shape their lines.

3.1 The table of roles
Several sources describe each pathet by a stressed tone (sometimes called the "tonic"), a strong
tone (sometimes called the "dominant") and an avoided tone. The Wikipedia summary of these
sources gives:

Gamelan Weights and Seleh · 3

Slendro pathet

Parallel pelog pathet

Stressed tone

Strong tone

Avoided in slendro

Rare in pelog

Nem

Lima

2

5

1

7

Sanga

Nem

5

1

3

7

Manyura

Barang

6

2

5

1

Table 1. Tone roles by pathet, as summarised in the Wikipedia article on pathet. The terms "tonic" and "dominant" are
borrowed from Western theory and used by some sources only. The same article notes that pelog lima and slendro nem are
not fully parallel musically.

Two things stand out. First, the stressed and strong tones in each slendro pathet are a fourth or fifth
apart (2 and 5, 5 and 1, 6 and 2), much like 1 and 5 in Western music. Second, the avoided tone is the
one that would sound "wrong" as a phrase ending, much like the 7th in a Western major key, but it
still appears in the middle of phrases.

3.2 Pelog and its two five-tone sets
The pelog pathet split into two families by tone set:
Pathet lima and pathet nem (together called bem, especially in Yogyakarta) use 1 2 3 5 6.
Pathet barang uses 2 3 5 6 7.
So tones 1 and 7 act as alternatives. The Javanese names reflect this: tone 1 is called bem and tone 7
barang. Tone 4 sits outside both sets and is used mainly for embellishment. Mantle Hood called it a
"dissonant" pitch, and elaborating instruments often play 3 or 5 in its place. The word "pelog" is
itself also the name of tone 4.
Rahn's study of 402 pelog balungan (core melodies) gives a sense of how strict this is in practice. No
melody used fewer than five tones, but many used six: either 1 to 6, or 1 2 3 5 6 7. When a melody
adds a sixth tone, it is usually 4 alongside the bem set or 1 alongside the barang set, borrowed for a
passage. (This part of Rahn's paper I could only read in summary.) The picture is the pelog version
of a Western melody borrowing ♭7: a stable five-tone core plus a rare guest.

3.3 Where phrases land
Older sources also list typical cadence patterns for each pathet. These are four-tone balungan
figures whose last tone is the landing tone:
Pathet

Cadence

Lands on

Slendro nem

6532

2

Slendro sanga

2165

5

Slendro manyura

3216

6

Pelog lima

5 4 2 1 (older) or 5 3 2 1 (newer)

1

Pelog nem

2165

5

Pelog barang

3276

6

Table 2. Typical cadence figures (Wikipedia, "Pathet"). Each lands on the pathet's stressed tone on the last beat of the gatra.

Note that all of these cadences are stepwise descents to the landing tone. That matches the Western
finding that small steps dominate melodies, and that downward motion into a stable note is a
common way to end a phrase.

Gamelan Weights and Seleh · 4

3.4 End-weighting: the gatra
Balungan melodies are written in groups of four beats called gatra. The strongest beat is the fourth,
the last one, and the gong and kenong strokes mark the ends of larger units. The tone on that last
beat is the seleh, the goal of the phrase. Musicians often hear a gatra as moving towards its final tone.
This turns the Western picture around. In part 1, Prince & Schmuckler showed that the tonic is
strongest on downbeats in classical music. In gamelan, the equivalent "downbeat profile" belongs
on beat 4 of every four. For a module, that means the strong-beat position must be a setting, not a
fixed assumption.

4. What the research can and cannot tell us
No Essen-style counts. I found no published study that counts every tone across a large gamelan
corpus and reports the distribution, in the way Aarden did for folk songs or Temperley and de
Clercq for rock. A balungan corpus exists online (Rahn used Barry Drummond's compilation), so
such a count would be possible, but I have not found one published.
Hood's study and its critics. Mantle Hood's The Nuclear Theme as a Determinant of Patet in Javanese
Music (1954, reissued 1977) analysed balungan statistically, especially the tones on gatra
endings, and proposed the gong-tone scheme behind Table 1. Later scholars, including Javanese
musicians and theorists, have argued that pathet is better understood through the elaborating
parts, especially the rebab and voice, which range over several octaves, and through the whole
melodic context, not through balungan endings alone. The Wikipedia article states that Hood's
cadence-based distinctions "have been largely disproven". The roles in Table 1 are still a widely
used rule of thumb, which is how this document uses them.
Practice varies. Solo (Surakarta), Yogyakarta, East Java, Sunda and Bali differ in their treatment
of pathet and tuning. There is a published study of pathet and avoided tones in East Javanese
(Jawatimuran) gamelan that I could not access.
For your purposes this is fine. You are not trying to reproduce Javanese practice; you are borrowing
a principle: a small set of strong tones, a larger set of travelling tones, a rare guest, and a landing
beat. The weightings below are designed with that in mind.

5. Weight profiles for gamelan scales
Because there are no measured counts, these profiles are built from the roles in Table 1 and the
tone sets in section 3.2. The rule is simple:
Slendro: stressed tone 30%, strong tone 24%, the two remaining tones 18% each, the avoided
tone 10%.
Pelog: landing tone 28%, its partner 20%, the other three core tones 12–16%, tone 4 at 5%, and
the tone outside the set (7 for lima and nem, 1 for barang) at 3%.

Gamelan Weights and Seleh · 5

Slendro: designed weights for each pathet
30%

Nem

Sanga

Manyura

thgiew

20%
10%
0%

1

2

3

5

6

Figure 2. Weights for the three slendro pathet, built from the stressed, strong and avoided tones in Table 1. These are
designed starting points, not measured counts.

Pelog: designed weights for each pathet
30%

Lima

Nem

Barang

thgiew

20%
10%
0%

1

2

3

4

5

6

7

Figure 3. Weights for the three pelog pathet. Lima and nem favour the 1 2 3 5 6 set, barang the 2 3 5 6 7 set; tone 4 stays
rare in all three. Designed, not measured.

These numbers are deliberately moderate. They make the strong tones clearly more likely without
making the avoided tones vanish, which is the behaviour the pathet literature describes. With the
Focus control in the module (section 7.3) you can sharpen or flatten them by ear.
Slendro and pelog in 12-tone gear
Neither tuning fits 12-TET. On Marbles or any standard quantizer, slendro is usually approximated as a
major or minor pentatonic scale and pelog as a Phrygian-like five-note set (for example 1 ♭2 ♭3 5 ♭6). These
approximations keep the interval pattern but lose the characteristic tuning. A dedicated module with a cents
table can do both: use the real tuning, or snap to 12-TET when you want to play with your other gear.

Gamelan Weights and Seleh · 6

6. The Seleh module: concept
You already make melodies by sending random CV into a quantizer. A normal quantizer gives every
allowed note an equal share of the voltage range, so with an evenly spread random source every
scale note comes up equally often. That is the "flat" distribution from part 1: about as unlike real
melodies as possible, which is one reason random-into-quantizer patterns can sound aimless.
Seleh replaces the equal shares with weighted ones. You choose:
1. a root (C to B, or any tone of a gamelan tuning),
2. a scale (major, minor, the modes, pentatonics, slendro, pelog), which decides which notes are
allowed,
3. a profile, which decides how likely each allowed note is. Profiles come from the research: folk,
classical, rock, downbeat and the gamelan pathet.
A Focus control then lets you dial the hierarchy from flat (normal quantizer) to the research values
to an exaggerated version. An Accent input lets a different "landing" profile take over on strong
beats.
CV in

Normalise

Weight table

Bins / Gravity

Tuning

random voltage

to 0–1 per octave

scale × profile ^ Focus

pick a degree

12-TET or cents table

↑ Root · Scale · Profile · Focus

↑ Accent · Step · Mode

↓ V/oct · Trig · Degree · Stability

Figure 6. Signal flow inside the module. Everything to the left of Bins/Gravity is recalculated only when a control changes;
the per-sample work is a single table lookup.

7. How it picks a note
This section explains the core idea in plain language. It is the part that makes the module different
from an ordinary quantizer, so it is worth understanding even if someone else writes the code.

7.1 Building the weight table
Whenever you change root, scale, profile or Focus, the module builds a small table with one row
per allowed note:
1. Start from the profile, which gives a weight for each scale degree relative to the root (for
example the rock minor profile: 1 = 0.317, ♭3 = 0.159, 5 = 0.131 and so on).
2. Remove any degree the chosen scale does not contain. If the scale contains a degree the profile
has no data for, give it a small default weight (for example 2%), so it can still occur.
3. Raise every weight to the power of Focus (section 7.3).
4. Rescale so the weights add up to 1.

Gamelan Weights and Seleh · 7

That table is all the module needs. Building it only happens when a control moves, so it costs
nothing while the module is running.

7.2 Two ways to turn voltage into a note
Mode A: Bins (recommended default)
Each octave of input voltage is divided into slices, one per allowed note, in pitch order. Each slice is
as wide as that note's weight. A voltage that falls into a slice outputs that note, in the octave the
voltage is in. Figure 4 shows the result for natural minor.
How one octave of input voltage (0–1 V) is shared out, natural minor
1

Plain quantizer

2

♭3

1

Seleh, Focus 1

4
2

1

Seleh, Focus 2

5
♭3

4
2

0V

♭6

♭7

5
♭3

4

♭7

5

♭7
1V

Figure 4. A plain quantizer gives every scale note an equal slice of each volt. Seleh makes each slice as wide as the note's
weight (here the measured rock minor-melody profile), so a random voltage lands on the root about a third of the time.
Raising Focus to 2 squares the weights and makes the strong notes stronger still. Higher voltage still means higher note, so
the melodic contour survives.

Two good properties follow from this design:
Higher voltage always gives a higher or equal note. The up-and-down shape of your random CV
survives, so slow random curves still make rising and falling melodies, and a sequencer's
contour is kept.
With an evenly spread random input, each note comes up exactly as often as its weight says.
The Bins mode is a direct implementation of the research distributions.
One catch: many random sources are not evenly spread. Marbles, for example, can produce bellshaped distributions, and a slewed random voltage spends more time in the middle of its range.
With such a source the middle notes of each octave will be favoured on top of the weights. That is
not wrong, just different, and it can be useful. For the most faithful result, use sample-and-hold of
white noise or a Random8-style uniform output spanning a whole number of octaves.

Mode B: Gravity
The voltage is first quantized to the nearest allowed note, as in an ordinary quantizer. Then, with a
probability set by Focus, it is pulled to the strongest note within a short distance (for example
within three semitones) if that note is stronger. Strong notes act like magnets for nearby voltages.
Gravity mode keeps the input much closer to its original pitch and suits a sequencer or keyboard
input where you want gentle "correction" towards the strong notes, not a full re-weighting.

Gamelan Weights and Seleh · 8

7.3 Focus: one knob for the hierarchy
The Focus control: same profile, three settings
60%

Focus 0 (flat)

Focus 1 (research)

Focus 2 (sharpened)

eton hcae fo ecnahc

40%
20%
0%

1

2

♭3

4

5

♭6

♭7

Figure 5. Focus raises every weight to a power. At 0 all notes are equal (a normal quantizer); at 1 you get the research
profile; above 1 the hierarchy sharpens towards the root. It works like Marbles' STEPS, but smoothly and without notes
disappearing abruptly.

Focus raises each weight to a power before rescaling. At 0, every weight becomes 1 and the module
acts as a normal quantizer. At 1 you hear the profile exactly as measured or designed. At 2 or 3 the
strong tones take over and the melody becomes more anchored and repetitive. In terms of part 1,
Focus is a continuous entropy control: turning it down raises entropy and makes the line wander;
turning it up lowers entropy and grounds it. With CV on Focus, the arrangement map from part 1
(anchored intro, open middle, unsettled breakdown, grounded return) becomes a single slow
envelope.
An Invert switch (or negative Focus values) flips the hierarchy so that rare tones become common:
the avoided tone of a pathet, or 2, 4 and 7 in a major key. This gives you a deliberately unsettled,
Autechre-style mode with the same controls.

7.4 Accent: a separate profile for landing beats
The research shows two hierarchies: one for all notes, one for strong positions (downbeats in
Western music, the end of the gatra in gamelan). Seleh models this with an Accent gate input. When
the gate is high at the moment a new note is taken, the module uses a landing profile instead of the
main one. The landing profile is derived automatically from the main profile by boosting the top
tones (for example, the top two weights doubled before rescaling), or for the Western classical
profiles you can use the measured Prince & Schmuckler downbeat profile directly.
Patch a clock divider into Accent: every 4th clock for a Western bar downbeat, or every 4th clock
offset by three steps for a gamelan gatra ending. Or let the module do it with an internal counter: a
Length setting (2 to 16 steps) and a Strong step setting (which step of the cycle is the landing beat).
Selecting a gamelan profile could set Strong step to the last step automatically.

7.5 Step: preferring small intervals
Melodies in every culture studied move mostly by small steps, and gamelan cadences are stepwise
descents. A Step control multiplies each note's weight by a factor that falls with its distance from
the previous output note. At 0 there is no effect. As Step increases, nearby notes become more
likely, and big leaps rarer. Step works on top of the profile: the strong tones are still favoured, but

Gamelan Weights and Seleh · 9

the module tends to reach them by walking instead of jumping. (In Bins mode the slices are
recalculated around the previous note for each new sample; this is still cheap because there are at
most 12 notes.)

7.6 Repeat avoidance
Strong weighting, especially at high Focus, produces many repeated root notes. A small No-repeat
setting reduces the weight of the note just played for the next pick. Set it low for hypnotic techno
basslines (repeats are welcome) and higher for leads.

8. Controls, inputs and outputs
Control

Range

What it does

Root

12 positions (or
tuning tones)

Home note. Also a Root CV input in 1 V/oct steps, so a sequencer can transpose the
hierarchy.

Scale

about 14 entries Major, natural minor, Dorian, Phrygian, Lydian, Mixolydian, harmonic minor, major
pentatonic, minor pentatonic, blues, chromatic, slendro, pelog bem (1 2 3 5 6 + 4 7),
pelog barang (2 3 5 6 7 + 1 4). Scale CV input.

Profile

about 12 entries The weighting source (section 9). Profile CV input.

Focus

0 to 3, plus
Invert

Strength of the hierarchy. Focus CV input with attenuverter.

Step

0 to 1

Preference for small intervals.

Range

1 to 5 octaves

How many octaves the input voltage covers before it is folded back.

Mode

Bins / Gravity

How voltage becomes a note (section 7.2).

Length /
Strong step

2–16 / 1–16

Internal accent counter, used when the Accent input is not patched.

Tuning

12-TET, slendro, Menu setting. Gamelan scales default to their own tunings; a 12-TET snap option
pelog, Scala
lets them play with Western gear.

Socket

Type

Purpose

CV in

input

The random (or any) voltage to quantize. Polyphonic in VCV.

Trig in

input

Takes a new note on each trigger (built-in sample-and-hold). Unpatched:
quantize continuously.

Accent in

input

High = use the landing profile for this note.

Root / Scale / Profile /
Focus CV

inputs Modulation of the matching controls.

V/oct out

output The quantized pitch.

Trig out

output Fires when the output note changes.

Degree out

output The chosen scale degree as a stepped voltage (for example 1 V per degree), to
drive other modules.

Stability out

output 0–10 V, the weight of the chosen note. High on strong tones, low on weak ones.

Gamelan Weights and Seleh · 10

The Stability output is the most musical extra. Patch it to a filter, a VCA or an envelope's decay, and
strong tones will automatically sound brighter, louder or longer than passing tones. That is how a
performer would phrase the line: lean into the landing notes, glide over the passing ones. Patched
to a clock divider or a probability gate, it can also decide when other voices play: for example, only
trigger a pad chord when the melody lands on a strong tone.

9. Profile library
Profile

Source

Character

Flat

none

Ordinary quantizer, for comparison.

Perceived fit (major
/ minor)

Krumhansl & Kessler
1982, ratings

The felt hierarchy; strong but not extreme.

Folk (major / minor)

Aarden 2003, Essen
counts

5 most common, natural minor with ♭7.

Classical (major /
minor)

Albrecht & Shanahan
2013

Tonic-led; harmonic minor colour.

Classical downbeat

Prince & Schmuckler
2014

Ready-made landing profile.

Rock melody (major
/ minor)

Temperley & de
Clercq 2013

Pentatonic, bluesy minor with a little 3.

Detroit Dorian

designed (part 1)

1, ♭3, 5, ♭7 strong, natural 6 as colour.

Slendro nem /
sanga / manyura

designed from pathet
roles

Section 5.

Pelog lima / nem /
barang

designed from pathet
roles

Section 5; tone 4 rare.

User

yours

Set weights by hand in the menu, or learn them from a sequence
played into the module (as Marbles does with its scale recording).

The learn mode is worth having. Feed the module a few bars from the Hapax or Metropolix and it
counts how often each degree appears. This lets you capture the hierarchy of a riff you like, then
generate endless variations that keep its character. It is the corpus-study method from part 1,
applied to your own material.

10. Patching ideas
1. Basic generative line. Clock → Trig in. Uniform random → CV in, Range 2 octaves. Rock minor
profile, Focus 1, Step 0.4. V/oct to a voice; Stability out to the filter cutoff.
2. Arrangement by Focus. A very slow envelope or a CV from Live (CV Tools through the disting
NT) to Focus CV. Keep Focus low in the breakdown and high at the drop.
3. Gamelan gatra. Pelog barang, Strong step = 4, Length 4, pelog tuning. Clock at a slow tempo; a
second Seleh at four times the speed with Step high plays the elaborating part. Use the first
module's Trig out to retrigger a gong-like sound only on accented notes.

Gamelan Weights and Seleh · 11

4. Detroit chords. Use Degree out to choose the chord root for a chord module or for Hapax via CV,
with the Detroit Dorian profile. The harmony then favours i, ♭III and v but occasionally drifts.
5. Self-mutating pattern. A looping random sequence (Marbles with DÉJÀ VU locked) into CV in,
then slowly change Profile or Root by CV. The rhythm and contour stay, the hierarchy changes:
the "re-weighting" and "re-anchoring" mutations from part 1.

11. Ways to build it
Platform

Effort

Notes

VCV Rack
plugin

Medium

Best first version. Matches your other custom modules, supports polyphony, menus
for tunings and user profiles, and is easy to test against the charts.

disting NT
plugin

Medium,
after VCV

A port of the same core to the NT's C++ plugin format, like your Drift Ensemble
port. Brings it into the rack with CV I/O, and the NT can be the clock from Live.

disting NT Lua
script

Low

Quickest hardware prototype. The core is a short loop over a table of up to 12
weights. Fewer controls, but enough to test the idea.

Max for Live
MIDI device

Low to
medium

Outputs MIDI notes instead of CV; good for Live-only sketches and for driving the
Hapax.

Marbles, today

None

Already does a stepped version of this: record a weighted jam (part 1, section 6.1)
and use STEPS. Seleh adds smooth Focus, gamelan tunings, accents, Step and the
Stability output.

The core algorithm is short: the weight table is at most 12 numbers, and picking a note is a search
through cumulative sums. Most of the build work would be the panel, the menus, tunings and
testing. A sensible test is to feed in a million uniform random samples and check that the output
counts match the chosen profile, which also produces charts like the ones in this document.

12. Further reading
Barbour, J. M. (1963), as cited in Rahn (2016): pelog described as five small and two large steps.
Becker, J. (1980). Traditional Music in Modern Java: Gamelan in a Changing Society. University Press of Hawaii.
Hastanto, S. (1985). The Concept of Pathet in Central Javanese Gamelan Music. PhD thesis, University of Durham.
Hood, M. (1954; reissued 1977). The Nuclear Theme as a Determinant of Patet in Javanese Music. Groningen: Wolters;
reissued New York: Da Capo.
Kunst, J. (1949). Music in Java. The Hague: Nijhoff.
Lindsay, J. (1992). Javanese Gamelan. Oxford University Press.
McGraw, A. C. (1999). The development of the Gamelan Semara Dana and the expansion of the modal system in
Bali, Indonesia. Asian Music 31(1), 63–93.
Perlman, M. (2004). Unplayed Melodies: Javanese Gamelan and the Genesis of Music Theory. University of California
Press.
Rahn, J. (2016). Coordinating analyses of tunings with analyses of pieces. Analytical Approaches to World Music
5(1).
Sumarsam (1995). Gamelan: Cultural Interaction and Musical Development in Central Java. University of Chicago
Press.

Gamelan Weights and Seleh · 12

Supanggah, R. (1988). Balungan. Trans. M. Perlman. Balungan 3(2), American Gamelan Institute.
Surjodiningrat, W., Sudarjana, P. J. & Susanto, A. (1972). Tone Measurements of Outstanding Javanese Gamelans in
Jogjakarta and Surakarta. Gadjah Mada University Press.
Tenzer, M. (2000). Gamelan Gong Kebyar: The Art of Twentieth-Century Balinese Music. University of Chicago Press.
Vitale, W. (2002). Balinese kebyar music breaks the five-tone barrier. Perspectives of New Music 40(1), 5–69.
Wikipedia articles "Pathet", "Pelog" and "Slendro" (accessed November 2026), for Tables 1 and 2.

Gamelan Weights and Seleh · 13
```

## Sources
- Project file: gamelan-weights-and-seleh-quantizer.pdf (text reproduced above)

Related: [[Scale degree frequency in melodies]]
