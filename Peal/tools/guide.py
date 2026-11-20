#!/usr/bin/env python3
# Builds docs/guide.html (and the PDF with tools/pdf.js) for Peal + Bell Foundry.
#   python3 tools/guide.py && node tools/pdf.js $PWD/docs/guide.html Peal-Guide.pdf
import os, base64
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
CH = "1234567890ET"


def parse(pn):
    def toks(s):
        out, acc = [], ""
        for c in s:
            if c in "x-":
                if acc: out.append(acc); acc = ""
                out.append("x")
            elif c == ".":
                if acc: out.append(acc); acc = ""
            else:
                acc += c
        if acc: out.append(acc)
        return out
    if "," in pn:
        a, b = pn.split(",")
        a = toks(a)
        return a + a[-2::-1] + toks(b)
    return toks(pn)


def apply(row, change):
    r = list(row); n = len(r)
    keep = set() if change == "x" else {CH.index(c) for c in change}
    i = 0
    while i < n:
        if i in keep: i += 1; continue
        if i + 1 < n and (i + 1) not in keep:
            r[i], r[i + 1] = r[i + 1], r[i]; i += 2
        else:
            i += 1
    return r


def ring(n, changes, rows_max):
    row = list(CH[:n]); rows = [row]
    i = 0
    while len(rows) < rows_max:
        row = apply(row, changes[i % len(changes)]); rows.append(row); i += 1
    return rows


def grid(rows, lines=("1",), cols=None, size=4.2, marks=None, notes=None, w_extra=0, zoom=5.2):
    """SVG of rows written out with the path of some bells drawn through them."""
    n = len(rows[0]); cw, rh = size, size * 1.05
    colours = {"1": "#d8402f", "2": "#2f6fd8", "6": "#2f6fd8", "8": "#2f6fd8"}
    W = n * cw + 6 + w_extra; H = len(rows) * rh + 4
    s = [f'<svg viewBox="0 0 {W} {H}" style="width:{W * zoom}px;height:{H * zoom}px" xmlns="http://www.w3.org/2000/svg">']
    for b in lines:
        pts = " ".join(f"{3 + cw * (r.index(b) + .5):.2f},{2 + rh * (i + .5):.2f}" for i, r in enumerate(rows))
        s.append(f'<polyline points="{pts}" fill="none" stroke="{colours.get(b, "#2f6fd8")}" stroke-width="0.55" stroke-linejoin="round" opacity="0.8"/>')
    for i, r in enumerate(rows):
        if marks and i in marks:
            s.append(f'<line x1="2" x2="{3 + cw * n + 1}" y1="{2 + rh * i}" y2="{2 + rh * i}" stroke="#999" stroke-width="0.25"/>')
        for k, c in enumerate(r):
            col = "#d8402f" if c == "1" else "#222"
            s.append(f'<text x="{3 + cw * (k + .5):.2f}" y="{2 + rh * (i + .5) + 1.25:.2f}" font-size="{size * 0.78:.2f}" text-anchor="middle" fill="{col}" font-family="DejaVu Sans Mono,monospace">{c}</text>')
        if notes and i in notes:
            s.append(f'<text x="{3 + cw * n + 1.5}" y="{2 + rh * (i + .5) + 1.1:.2f}" font-size="{size * 0.55:.2f}" fill="#8a5a12" font-family="Inter,Helvetica,sans-serif">{notes[i]}</text>')
    s.append("</svg>")
    return "".join(s)


img = base64.b64encode(open(os.path.join(ROOT, "panel-preview.png"), "rb").read()).decode()

# diagrams
hunt6 = ring(6, ["x", "16"], 13)
pb6 = ring(6, parse("x16x16x16x16x16x12"), 13)
pb6_bob = [list("123456")]
r = list("123456")
for c in parse("x16x16x16x16x16x14"):
    r = apply(r, c); pb6_bob.append(r)
gd5 = ring(5, parse("3.1.5.1.5.1.5.1.5.1"), 11)
st5 = ring(5, parse("3.1.5.3.1.3.1.3.5.1.3.1"), 13)
rounds_hand = ["1234", "1234", "gap", "1234", "1234"]

pages = []

pages.append(f"""
<h1>Peal <span class="amp">+</span> Bell Foundry</h1>
<div class="sub">Change ringing sequencer and twelve cast bells · VCV Rack · plugin Peal 2.0.0 · Peal 36 HP, Bell Foundry 24 HP</div>
<img src="data:image/png;base64,{img}">
<h2>Quick start</h2>
<ol>
<li>Clock into Peal <b>CLOCK</b>. Every pulse rings one bell, so a 16th-note clock is a good start.</li>
<li>Peal <b>GATES</b> to Bell Foundry <b>GATES</b>, and Peal <b>TUNES</b> to Bell Foundry <b>V/OCT</b>. Both are poly cables, one channel per bell.</li>
<li>Bell Foundry <b>OUT L/R</b> to your mixer. You'll hear rounds (bells in order, top to bottom) and then, from the next handstroke, Plain Bob Major.</li>
<li>Press <b>BOB</b> or <b>SINGLE</b> to steer it. Turn <b>CALLS</b> up so it calls itself. Press <b>GO</b> again for “That's all” (back to rounds).</li>
</ol>
<div class="box">The next four pages explain change ringing, the English way of ringing church bells that Peal is built on.
If you just want to patch, skip to page 6.</div>
""")

pages.append(f"""
<div class="theory">Change ringing theory · 1 of 4</div>
<h2>Bells that can't play tunes</h2>
<p>An English ringing tower has a ring of between 4 and 12 bells (sometimes more), tuned to a descending major scale. The highest is the
<b>treble</b> (bell 1) and the lowest is the <b>tenor</b>. Each bell weighs anything from a sack of flour to a few tonnes and hangs mouth-up,
swinging through a full circle every time it rings. One ringer per bell controls it with a rope.</p>
<p>Because a bell that heavy swings at its own pace, a ringer can only make it strike a little earlier or a little later than last time.
You can't play a melody. Instead the bells ring one after another in a fixed rhythm, and the music is the <b>order</b> they ring in.</p>
<h3>Rows, rounds and changes</h3>
<p>One blow of every bell is a <b>row</b> (also called a <b>change</b>). Ringing always starts and ends in <b>rounds</b>: 1 2 3 4 5 6 7 8, top to bottom,
a falling scale. From then on the ringers change the order every row, under one strict rule:</p>
<div class="rule">Between one row and the next, a bell may only move <b>one place</b> earlier or later, or stay where it is.</div>
<p>So the only thing that can happen is that pairs of neighbouring bells swap. The order drifts gradually, which is why change ringing sounds
like a pattern slowly turning inside out rather than a random shuffle. Peal works the same way: each clock pulse rings the next bell in the row,
and at the end of the row the order shifts.</p>
<h3>Handstroke, backstroke and the gap</h3>
<p>Each bell swings one way for the <b>handstroke</b> (the ringer pulls the woolly sally) and back the other way for the <b>backstroke</b>
(the tail end of the rope). Rows therefore come in pairs, handstroke then backstroke. Before every handstroke row, ringers leave a
gap of one beat, called the <b>open handstroke lead</b>. That gap gives tower bells their limping, breathing lilt:</p>
<div class="box mono">hand 1 2 3 4 5 6 · back 1 2 3 4 5 6 · <i>(gap)</i> · hand 1 2 3 4 5 6 · back 1 2 3 4 5 6 · <i>(gap)</i></div>
<p>Peal's <b>GAP</b> knob sets how many clock pulses that rest lasts (0 to 4). With 6 bells and a gap of 1, a whole pull is 13 pulses long,
which drifts against a 16-step bar for a long time before it lines up again.</p>
<h3>The names of the stages</h3>
<table class="t"><tr><th>Bells</th><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td></tr>
<tr><th>Stage</th><td>Minimus</td><td>Doubles</td><td>Minor</td><td>Triples</td><td>Major</td><td>Caters</td><td>Royal</td><td>Cinques</td><td>Maximus</td></tr>
<tr><th>Rows possible</th><td>24</td><td>120</td><td>720</td><td>5,040</td><td>40,320</td><td>362,880</td><td>3.6 m</td><td>40 m</td><td>479 m</td></tr></table>
<p>The bottom line is <i>n</i>! (n factorial), the number of different orders n bells can ring in. Ringing them all is an <b>extent</b>.
A <b>peal</b> is at least 5,000 rows without ringing any row twice. That's about three hours of non-stop ringing, and on seven bells
the 5,040-row extent is exactly a peal. A <b>quarter peal</b> (1,260 rows or more) takes about 45 minutes.</p>
""")

pages.append(f"""
<div class="theory">Change ringing theory · 2 of 4</div>
<h2>Plain hunt and place notation</h2>
<div class="cols"><div>
<p>The simplest way to obey the one-place rule is to swap <i>every</i> pair, then every pair except the bells at each end, and repeat.
This is <b>plain hunt</b>. Each bell weaves steadily from the front of the row to the back and back again, and after 2 × n rows
the bells are in rounds again.</p>
<p>Ringers learn a method by the path one bell traces down the page, called its <b>line</b>. Here the treble is in red and the 2 is in blue.
The treble's straight zig-zag is the <b>hunt</b>.</p>
<h3>Place notation</h3>
<p>A change is written down by listing the places (positions) where the bell does <i>not</i> move. Every other bell swaps with its neighbour.</p>
<table class="t2">
<tr><td class="mono">x</td><td>no places made: every pair swaps (on even stages only)</td></tr>
<tr><td class="mono">16</td><td>the bells in 1st and 6th place stay; 2-3 and 4-5 swap</td></tr>
<tr><td class="mono">12</td><td>1st and 2nd stay; 3-4 and 5-6 swap</td></tr>
<tr><td class="mono">5</td><td>on 5 bells: 5th stays; 1-2 and 3-4 swap</td></tr>
</table>
<p>So plain hunt on six is <span class="mono">x 16 x 16 x 16…</span>, and on five it's <span class="mono">5 1 5 1 5 1…</span>.
A whole method is just a list of these. For example, Cambridge Surprise Minor is written as
<span class="mono">x36x14x12x36x14x56,12</span>. The comma means “run the first part forwards, then backwards, then 12”.
Peal stores every method this way and applies one change at the end of each row.</p>
<h3>Hunting, dodging and making places</h3>
<p>Every bell's line is built from three moves:</p>
<ul><li><b>Hunting:</b> moving one place each row, steadily up or down.</li>
<li><b>Dodging:</b> stepping back one place and then on again, so two bells swap and swap back.</li>
<li><b>Making a place:</b> staying in the same position for two rows.</li></ul>
<p>Methods are different recipes of these three moves. The display on Peal draws the lines of the treble and the 2 through the last rows
so you can watch them.</p>
</div><div class="fig">{grid(hunt6, lines=("1", "2"), notes={0: "rounds", 6: "treble at the back", 12: "rounds again"}, w_extra=22)}
<div class="cap">Plain hunt on six: 12 changes then back to rounds</div></div></div>
""")

pages.append(f"""
<div class="theory">Change ringing theory · 3 of 4</div>
<h2>Methods: getting past rounds</h2>
<p>Plain hunt comes round after only 2n rows. To ring longer without repeating a row, a method changes one thing at the
<b>lead end</b> (the moment the treble leads), so the other bells start the next <b>lead</b> in a new order. Each lead is a variation,
and the set of leads before rounds comes back is the <b>plain course</b>.</p>
<div class="cols3">
<div class="fig">{grid(pb6, lines=("1", "2"), marks={12}, notes={12: "lead head"}, size=3.8, w_extra=14, zoom=4.4)}<div class="cap">Plain Bob Minor, first lead</div></div>
<div class="fig">{grid(gd5, lines=("1", "2"), size=3.8, zoom=4.4)}<div class="cap">Grandsire Doubles, first lead</div></div>
<div class="fig">{grid(st5, lines=("1", "2"), size=3.8, zoom=4.4)}<div class="cap">Stedman Doubles, one lead (two sixes)</div></div>
</div>
<table class="t2">
<tr><td>Plain Hunt</td><td>Every bell hunts. Comes round after 2n rows. It's the base for everything else.</td></tr>
<tr><td>Plain Bob</td><td>Plain hunt, but at the lead end the bells in 1st and 2nd place stay (<span class="mono">12</span>) while the rest dodge.
The oldest common method, from the 1600s. Plain course: 5 leads (60 rows) on six, 7 leads (112) on eight.</td></tr>
<tr><td>Little Bob</td><td>Like Plain Bob, but the treble only hunts as far as 4th place, so leads are short (8 rows) and busy at the front.</td></tr>
<tr><td>Grandsire</td><td>Two hunt bells (treble and 2) and odd stages. The 3 is made in every lead head, which gives the method its
famous lurch. In Peal, on even bells the tenor rings behind as a <b>cover</b>.</td></tr>
<tr><td>Stedman</td><td>From Fabian Stedman, whose <i>Tintinnalogia</i> (1668) and <i>Campanalogia</i> (1677) are the first books on ringing. There's no hunt
bell. The bells work in <b>sixes</b> (groups of six rows) that alternate quick and slow, so the sound tumbles rather than flows. Odd stages, with a cover on even.</td></tr>
<tr><td>Cambridge Surprise</td><td>A <b>surprise</b> method: places are made inside the row (not just at the ends) as the treble passes, which makes the
lines complex. Plain course 120 rows on six, 224 on eight, 528 on twelve. On fewer than six bells Peal rings Plain Bob instead and marks it with *.</td></tr>
<tr><td>Wander</td><td>Not a real method. Peal starts from plain hunt and, at each lead end, the <b>CALLS</b> knob sets the chance that one change in
the lead is rewritten at random (still obeying the one-place rule). The lead repeats with small mutations, so the pattern keeps
evolving. It may never come round.</td></tr></table>
<p class="small">Peal's engine is checked against the known plain-course lengths of every method above at every stage it rings,
and each course is checked to be <b>true</b> (no row rung twice).</p>
""")

pages.append(f"""
<div class="theory">Change ringing theory · 4 of 4</div>
<h2>Calls and conducting</h2>
<div class="cols"><div>
<p>One plain course is never enough for a peal. To ring longer, the <b>conductor</b> (one of the ringers) shouts <b>calls</b>.
A call swaps the place notation of the lead end for a different change, so the bells come out of that lead in a new order and go on into new rows.</p>
<table class="t2">
<tr><td>Bob</td><td>In Plain Bob, <span class="mono">14</span> instead of <span class="mono">12</span>. Three bells change what they'd have done and the lead head moves on differently.</td></tr>
<tr><td>Single</td><td><span class="mono">1234</span>: only two bells swap work. Singles let a composition reach rows that bobs alone can't.</td></tr>
<tr><td>In Grandsire</td><td>Bob = <span class="mono">3.1</span> at the end of the lead, single = <span class="mono">3.123</span>.</td></tr>
<tr><td>In Stedman</td><td>Calls come at the end of a six, by making a place nearer the back (bob) or three places (single).</td></tr></table>
<p>Ringers name calls by where the tenor is when the call is made, such as <b>Home</b> (the tenor makes the bob at the back), <b>Wrong</b>,
<b>Before</b> and <b>Middle</b>. A <b>composition</b> is a list of where to call bobs and singles so that thousands of rows go by,
none repeats, and the last row is rounds. That is the conductor's real art.</p>
<h3>The words of a ringing session</h3>
<table class="t2">
<tr><td>“Look to”</td><td>Get ready. “Treble's going… she's gone.” Rounds begin.</td></tr>
<tr><td>“Go Plain Bob”</td><td>The method starts at the next handstroke. In Peal, press <b>GO</b>.</td></tr>
<tr><td>“Bob!” / “Single!”</td><td>Called just before the lead end it applies to. In Peal, use the <b>BOB</b> / <b>SINGLE</b> buttons, triggers or the CALLS knob.</td></tr>
<tr><td>“That's all”</td><td>Back to rounds (normally when it has come round). Press <b>GO</b> again.</td></tr>
<tr><td>“Stand”</td><td>Stop. In Peal, just stop the clock.</td></tr></table>
</div><div class="fig">{grid(pb6_bob, lines=("1", "2"), marks={12}, notes={12: "bob: 14 not 12"}, w_extra=20)}
<div class="cap">Plain Bob Minor, first lead with a bob. Compare the last row with the plain lead on page 4.</div></div></div>
<h3>Why this is good for electronic music</h3>
<p>Change ringing is a 400-year-old generative system: a few strict rules, a pattern that keeps evolving, and a human who steers it
with calls rather than writing every note. Every row has the same notes, just reordered, so harmony stays fixed while the
melody and rhythm keep shifting. It always comes home to rounds. Plain courses are long but finite, so they give an arrangement natural
sections. Calls are your fills and breaks, and a lead end is a phrase boundary, so patch LEAD and CALL into whatever drives your arrangement.</p>
""")

pages.append("""
<h2>Peal: controls</h2>
<table class="t2">
<tr><td>Screen</td><td>The last 8 rows written out like a ringer's diagram (treble red, the 2 blue, the bell just rung lit, a line above each lead head, a mark beside rows started by a call).
On the right: method, stage (with “+ COVER” when the tenor covers), lead and change in the lead, gap and scale, and the last event (GO!, BOB!, SINGLE!, THAT'S ALL, ROUNDS).</td></tr>
<tr><td>GO</td><td>Lit while ringing the method. Off = ring rounds. Turning it on starts the method at the next handstroke. Turning it off is “That's all” (straight back to rounds at the end of the row). The GO input is a trigger that does the same.</td></tr>
<tr><td>RESET</td><td>Rounds, handstroke, start of the lead, right now.</td></tr>
<tr><td>BOB / SINGLE</td><td>Arm a call for the next lead end (light on while it's waiting and while it happens). The inputs are triggers.</td></tr>
<tr><td>BELLS</td><td>4 to 12. A change takes effect at the end of the row and restarts in rounds. CV: 1 V per bell.</td></tr>
<tr><td>METHOD</td><td>Hunt, Plain Bob, Little Bob, Grandsire, Stedman, Cambridge, Wander. Changes at the next lead end, as a conductor would. CV: 1 V per method.</td></tr>
<tr><td>CALLS</td><td>Chance that a lead end gets a call by itself (3 bobs to 1 single). For Wander, it's the chance that the lead mutates. CV: 10 V = 100 %.</td></tr>
<tr><td>GAP</td><td>Rest pulses before every handstroke row (0 to 4, normally 1). CV: 1 V per pulse.</td></tr>
<tr><td>GATE</td><td>Gate length as a share of the clock period, from a blip to tied. CV: 10 V = full.</td></tr>
<tr><td>SCALE</td><td>Major, minor, Dorian, Mixolydian, pentatonic major and minor, whole tone, chromatic. The tenor is 0 V (C4) and the treble is at the top. CV: 1 V per scale.</td></tr>
<tr><td>TUNE</td><td>A poly V/OCT chord replaces the scale. Channel 1 is the tenor, upwards from there, and it repeats an octave up if there are more bells than notes. A Hapax chord fits here.</td></tr>
<tr><td>V/OCT</td><td>Transposes everything.</td></tr></table>
<h2>Peal: outputs</h2>
<table class="t2">
<tr><td>1 to 12</td><td>A gate for each bell (1 = treble). Bells above the stage stay silent.</td></tr>
<tr><td>GATES · TUNES</td><td>The same gates as one poly cable, and each bell's pitch as poly (channel = bell). Patch both to Bell Foundry, or any poly voice.</td></tr>
<tr><td>PITCH · PLACE</td><td>Pitch of the bell just rung (held). Place in the row it rang from: 0 V leading, 10 V at the back.</td></tr>
<tr><td>ROW</td><td>Poly: the current row as pitches in ringing order, so channel 1 is whoever is leading. Changes once per row, which makes it a slowly turning chord.</td></tr>
<tr><td>STRIKE</td><td>A trigger on every blow (not in the gap).</td></tr>
<tr><td>HAND</td><td>High through handstroke rows and the gap, low through backstrokes. Use it for a swing, accent or filter.</td></tr>
<tr><td>CHANGE · LEAD</td><td>Triggers at the start of every row and at every lead end.</td></tr>
<tr><td>ROUNDS</td><td>Trigger when the method comes round to rounds by itself (the end of a course).</td></tr>
<tr><td>CALL</td><td>Trigger on the row a bob or single starts. It fires for auto calls too, so other modules can follow the conductor.</td></tr></table>
""")

pages.append("""
<h2>Bell Foundry</h2>
<p>Twelve bells, one per poly channel. Each bell is a set of eight ringing partials, each with a slightly detuned twin. Real bells are never
perfectly round, which gives them that slow warble. Every bell is cast a little differently, and <b>RECAST</b> pours a new set (saved with the patch).
With no V/OCT cable the bells tune themselves to a major scale down to C3.</p>
<table class="t2">
<tr><td>PROFILE</td><td>Church bell to handbell to gamelan gong/bonang. A church bell's partials are the <b>hum</b> (an octave down), <b>prime</b>, <b>tierce</b> (a minor third, the sad colour of bells),
<b>quint</b>, <b>nominal</b> (octave) and higher. A handbell is mostly fundamental plus a tuned twelfth. The gamelan setting has a strong fundamental and inharmonic upper partials. The screen draws them.</td></tr>
<tr><td>METAL</td><td>Brass (darker, shorter) to bell bronze (in the middle, longest and richest) to steel (bright, clangy, stretched partials).</td></tr>
<tr><td>THICK</td><td>A thin bell pulls the upper partials down and rings shorter. A thick one pushes them up and rings longer.</td></tr>
<tr><td>SIZE</td><td>±2 octaves. Bigger and lower bells ring longer.</td></tr>
<tr><td>CRACK</td><td>A hairline crack makes the twins beat. More gives a buzzing rattle, and the top partials die early. At full it's a broken bell.</td></tr>
<tr><td>CLAPPER</td><td>Soft leather gives a dull thump and low partials. Iron gives a sharp click and bright upper partials. VEL (poly) sets how hard each bell is struck.</td></tr>
<tr><td>RING · SPREAD</td><td>Ring time (×0.15 to ×3). Stereo spread puts the bells round a circle as they hang in the tower.</td></tr>
<tr><td>TUNING</td><td><b>TRUE</b>: Victorian true-harmonic tuning, everything in tune. <b>OLD</b>: older bells, where the hum, prime and tierce wander by tens of cents and every bell is sour in its own way.
<b>OMBAK</b>: every partial has a twin a few Hz away, like a gamelan pair, so it beats steadily.</td></tr>
<tr><td>LISTEN</td><td><b>CHAMBER</b>: up close, bright and dry. <b>CHURCHYARD</b>: through the louvres, darker, with some air. <b>TOWN</b>: far away, muffled and late, mostly reverb.</td></tr></table>
<h2>Patch ideas</h2>
<ol>
<li><b>Tower bells, properly:</b> 8 bells, Plain Bob, GAP 1, clock about 6 Hz, Bell Foundry on TRUE and CHURCHYARD. Call bobs by hand.</li>
<li><b>Polymetric techno:</b> clock on 16ths, 6 bells, GAP 1 (13 pulses per whole pull). Send bells 1, 3 and 6 to drum voices (Boohm, 808 Metal, SimpleKits). The pattern stays in its own cycle against the bar, and CALLS around 25 % keeps it from settling.</li>
<li><b>Self-mutating line:</b> Wander, CALLS 40 %, PITCH and STRIKE to a mono synth, TUNE from a Hapax chord. LEAD into an envelope for a phrase-length swell.</li>
<li><b>Slow chords:</b> ROW (poly) into a pad with a long attack. It changes once per row, so the chord slowly re-voices itself.</li>
<li><b>Steered by structure:</b> Long Form or any arranger sends triggers to BOB at section changes. ROUNDS ends a section by itself. METHOD CV from a slow stepped LFO changes method at lead ends only.</li>
<li><b>Gamelan tower:</b> Stedman Triples, Bell Foundry on PROFILE gamelan, TUNING OMBAK, SCALE pentatonic minor, LISTEN chamber.</li>
</ol>
""")

css = """
@page{size:A4;margin:12mm 13mm}
body{font-family:Inter,Helvetica,Arial,sans-serif;color:#231e1a;font-size:10pt;line-height:1.42;margin:0}
.page{page-break-after:always;padding:0}
h1{font-size:26pt;margin:0}.amp{color:#c48a2a}
h2{font-size:14pt;margin:10px 0 4px;color:#a5651a;border-bottom:1px solid #ddd}
h3{font-size:11pt;margin:10px 0 2px}
.sub{color:#a5651a;margin:2px 0 8px} img{width:100%;border-radius:4px}
.theory{font-size:8.5pt;letter-spacing:.08em;text-transform:uppercase;color:#888}
.box{background:#f4efe6;padding:6px 10px;border-radius:4px;margin:6px 0}
.rule{border-left:3px solid #d8402f;padding:4px 10px;margin:6px 0;background:#fbf3f1}
.mono{font-family:'DejaVu Sans Mono',monospace;font-size:9pt}
table.t{border-collapse:collapse;font-size:9pt;margin:4px 0} table.t td,table.t th{border:1px solid #ddd;padding:2px 5px;text-align:center}
table.t th{background:#f4efe6;text-align:left}
table.t2{border-collapse:collapse;margin:4px 0} table.t2 td{vertical-align:top;padding:2px 8px 3px 0} table.t2 td:first-child{font-weight:700;white-space:nowrap}
.cols{display:flex;gap:14px}.cols>div:first-child{flex:1}
.cols3{display:flex;gap:10px;justify-content:space-between;align-items:flex-start}
.fig{text-align:center}.cap{font-size:8.5pt;color:#666;max-width:260px;margin:2px auto}
.small{font-size:8.5pt;color:#666} ul,ol{margin:4px 0;padding-left:20px}
"""
html = ("<!doctype html><html><head><meta charset='utf-8'><title>Peal guide</title><style>" + css + "</style></head><body>"
        + "".join(f"<div class='page'>{p}</div>" for p in pages) + "</body></html>")
open(os.path.join(ROOT, "docs", "guide.html"), "w").write(html)
print("ok")
