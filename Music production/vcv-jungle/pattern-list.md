# Jungle sequencer: 32 preset chops (draft for review)

Each preset is a two-bar re-ordering of whatever break is loaded in the player. The player cuts the break (trimmed to one bar) into **16 equal slices**, so on most breaks slice 1 is the downbeat kick, slice 5 the beat-2 snare and slice D (13) the beat-4 snare.

**How to read a row:** one character per 16th step. `1`-`9` and `A`-`G` = slice 1 to 16. `-` = let the previous slice keep playing. `.` = silence.
The `mods` line lists step changes: `r` reverse, `s2`/`s3`/`s4` stutter (retrigger 2, 3 or 4 times inside the step), `+n`/`-n` pitch in semitones.

**3/8 patterns:** 25 to 28 use the 3+3+2 grouping (kicks or snares on steps 1, 4 and 7). The LENGTH knob (1 to 32 steps) also allows true 12-step or 24-step bars for 3/8 or 6/8 time.

**How these were made:** they are my own versions of common jungle editing moves (named by technique, not after records), not transcriptions. All 32 can be edited and saved over, and you can program your own. Mark any you want changed, dropped or added.

### 01 Straight
the break as recorded, the reference
```
bar 1  123456789ABCDEFG
bar 2  123456789ABCDEFG
```
### 02 Two-step
kick on 1 and the and-of-3, snares on 2 and 4: the basic jungle edit
```
bar 1  12345678129ADEFG
bar 2  12345678129ADEFD
```
### 03 Snare roll end
straight bar, then the last beat becomes a 16th snare roll
```
bar 1  123456789ABCDEFG
bar 2  123456789ABCDDDD
mods   b2s13:+2, b2s14:+4, b2s15:+7
```
### 04 Reverse tail
bar 2 plays beat 4 backwards into the downbeat
```
bar 1  123456789ABCDEFG
bar 2  123456789ABCGFED
mods   b2s13:r, b2s14:r, b2s15:r, b2s16:r
```
### 05 Stutter kick
kick slice retriggers in 32nds at the start of each bar
```
bar 1  1-3456789ABCDEFG
bar 2  1-3456789AB1DEFG
mods   b1s1:s4, b2s1:s4, b2s12:s2
```
### 06 Rinse out
beat 2 chopped and repeated, classic 1993 rush
```
bar 1  1234555578ABCDEF
bar 2  12345555DDDDCDEF
mods   b2s5:s2, b2s9:s2, b2s10:s3, b2s11:s4
```
### 07 Ghost shuffle
ghost-note slices moved onto off 16ths for a rolling feel
```
bar 1  1274563A9BCED6FG
bar 2  12745632B9CED6FA
```
### 08 Half-time drag
each slice held for two steps: the break at half speed
```
bar 1  1-2-3-4-5-6-7-8-
bar 2  9-A-B-C-D-E-F-G-
```
### 09 Double time
slices played twice as dense, 8th-note chop
```
bar 1  13579BDF13579BDF
bar 2  13579BDF1357DDDD
```
### 10 Pitch drop
bar 2 drops each beat a semitone further
```
bar 1  123456789ABCDEFG
bar 2  123456789ABCDEFG
mods   b2s5:-1, b2s6:-1, b2s7:-1, b2s8:-1, b2s9:-2, b2s10:-2, b2s11:-2, b2s12:-2, b2s13:-3, b2s14:-3, b2s15:-3, b2s16:-3
```
### 11 Ragga skank
sparse kick-snare with gaps for a bassline
```
bar 1  1..5..B.D..5.D..
bar 2  1..5..B.D..5DDDD
```
### 12 Amen switch
bar 2 swaps beats 3 and 4, a common 1994 edit
```
bar 1  123456789ABCDEFG
bar 2  12345678DEFG9ABC
```
### 13 Snare flam
every snare slice stuttered twice for a flam
```
bar 1  1234567899ABCDEF
bar 2  1234567899ABCDDF
mods   b1s5:s2, b1s13:s2, b2s5:s2, b2s13:s2
```
### 14 Backspin
last two beats reversed and pitched up, like a rewind
```
bar 1  123456789ABCDEFG
bar 2  12345678GFEDCBA9
mods   b2s9:r, b2s10:r, b2s11:r, b2s12:r, b2s13:r, b2s14:r, b2s15:r, b2s16:r
```
### 15 Choppage
hard random-feeling chop: kick and snare slices only
```
bar 1  1D15D1B5D5B1DD15
bar 2  1D15DB1D5D1BD555
```
### 16 Roller
two-step kick placement with continuous hats, 1995 style
```
bar 1  1234D6789A1CDEFG
bar 2  1234D678BA1CDEDG
```
### 17 Techstep
stiff, heavier: kick and snare slices re-ordered, fewer ghosts
```
bar 1  1..45..B1.D..5..
bar 2  1..45..B1.D.DDDD
```
### 18 Think shuffle
for funk breaks like Think: swung ghost slices
```
bar 1  1324576A98BCDEGF
bar 2  1324576A98BCDDGF
```
### 19 Apache stomp
for Apache-type breaks: bongo slices moved forward
```
bar 1  1235467A89BCDEFG
bar 2  1235467A89BCD5FG
```
### 20 Funky drummer
for Funky Drummer-type breaks: kick doubled
```
bar 1  1134567899BCDEFG
bar 2  1134567899BCDEDD
```
### 21 Breakdown
kick only, snare drops out, builds back in bar 2
```
bar 1  1...1...1...1...
bar 2  1...1...1.D.DDDD
mods   b2s13:s2, b2s14:s3, b2s15:s4, b2s16:s4
```
### 22 Stutter build
beat 1 stuttered harder each beat
```
bar 1  1111222233334444
bar 2  5555555555555555
mods   b1s1:s2, b1s5:s2, b1s9:s3, b1s13:s4, b2s1:s2, b2s5:s3, b2s9:s4, b2s13:s4
```
### 23 Reverse snare
snares play reversed, kicks forward
```
bar 1  1234567899ABCDEF
bar 2  123456789ABCDEFG
mods   b1s5:r, b1s13:r, b2s5:r, b2s13:r
```
### 24 Snare rush
last bar all snare slices, pitched upward
```
bar 1  123456789ABCDEFG
bar 2  5D5D5D5D5D5DDDDD
mods   b2s9:+1, b2s10:+2, b2s11:+3, b2s12:+4, b2s13:+5, b2s14:+6, b2s15:+7, b2s16:+8
```
### 25 3-3-2 kick
kick slice on steps 1, 4 and 7 (the classic 3+3+2 / dotted-8th jungle kick), snares on 2 and 4
```
bar 1  123123125678DEFG
bar 2  12312312567DDDDD
mods   b2s13:s2, b2s14:s3, b2s15:s4, b2s16:s4
```
### 26 3-3-2 snare
snare slice pushed onto the 3+3+2 grid, kick on the one
```
bar 1  123523589A5CD5FG
bar 2  123523589A5CDD55
```
### 27 Tresillo two-step
3+3+2 kicks under a two-step snare, the rolling 1994 feel
```
bar 1  1231235612312DEF
bar 2  12312356123DDDDD
mods   b2s12:s2, b2s13:s3
```
### 28 Three over four
a three-slice cell looping against the bar, lands back on 1 every 3 bars
```
bar 1  1231231231231231
bar 2  2312312312312312
```
### 29 Tail reverse
whole bar 2 backwards
```
bar 1  123456789ABCDEFG
bar 2  GFEDCBA987654321
mods   b2s1:r, b2s2:r, b2s3:r, b2s4:r, b2s5:r, b2s6:r, b2s7:r, b2s8:r, b2s9:r, b2s10:r, b2s11:r, b2s12:r, b2s13:r, b2s14:r, b2s15:r, b2s16:r
```
### 30 Hats only
break with kick and snare taken out, for layering
```
bar 1  .2.4..7.9A..CEFG
bar 2  .2.4..7.9A..CEFG
```
### 31 Gap edit
drops a beat of silence for a bass hit
```
bar 1  123456789ABC....
bar 2  123456789ABCDEFG
```
### 32 Jungle classic
straight bar, two-step bar, snare-roll fill: a full 1994 phrase in two bars
```
bar 1  12345678129ADEFG
bar 2  1234567812DDDDDD
mods   b2s11:s2, b2s12:s2, b2s13:s3, b2s14:s3, b2s15:s4, b2s16:s4
```
