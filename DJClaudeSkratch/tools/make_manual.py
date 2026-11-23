"""Builds docs/DJ-Claude-Skratch-Manual.pdf."""
import os
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ss = getSampleStyleSheet()
H1 = ParagraphStyle('h1', parent=ss['Heading1'], fontSize=20, spaceAfter=8)
H2 = ParagraphStyle('h2', parent=ss['Heading2'], fontSize=13, spaceBefore=10, spaceAfter=4)
P = ParagraphStyle('p', parent=ss['BodyText'], fontSize=9.5, leading=13)
C = ParagraphStyle('c', parent=P, fontSize=8.5, leading=11)
def tbl(rows, w):
    t = Table([[Paragraph(str(c), C) for c in r] for r in rows], colWidths=w, repeatRows=1)
    t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#2c2c2e')), ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                           ('GRID', (0, 0), (-1, -1), 0.3, colors.grey), ('VALIGN', (0, 0), (-1, -1), 'TOP')]))
    return t
s = []
s += [Paragraph('DJ Claude Skratch', H1), Paragraph('A scratching machine for Ableton Live (Max for Live instrument). Version 1.0.0.', P), Spacer(1, 6)]
s += [Image(os.path.join(ROOT, 'docs', 'panel-preview.png'), width=180 * mm, height=180 * mm * 338 / 2300), Spacer(1, 6)]
s += [Paragraph('What it is', H2), Paragraph(
    'One deck, one fader, one sound. A modelled record hand moves the record and a modelled fader hand opens and closes the fader, '
    'in time with Live. Every scratch (a "word") is a short script for the two hands, so a chirp sounds like a chirp because of what '
    'the hands do, not because a chirp sound is stored. The sound goes through a model of a cartridge and phono stage: a held record '
    'is silent, slow movement is quiet and dark, fast movement is loud and bright, and at playing speed you hear the sound as it is. '
    'It is the scratching half of Doubles, given its own panel and a vocabulary of sixteen words.', P)]
s += [Paragraph('Getting started', H2), Paragraph(
    '1. Put it on a MIDI track. It makes a sound straight away: a built-in test record (a vowel and a short word). '
    '2. Drop your own sound on DROP SOUND: a vocal stab, an "ahh", a spoken phrase, up to 20 seconds. '
    '3. Play C1 (Baby) and hold it. Hold D1 for stabs, D#1 for chirps. '
    '4. Set MODE to INSTINCT and press play in Live: it scratches by itself, in four-bar phrases. '
    'The device makes only the scratch; the beat is the rest of your set.', P)]
s += [Paragraph('The record and cue points', H2), Paragraph(
    'The strip shows the sound with its cue points, numbered 1 to 8, found where a sound starts. The orange mark is the current '
    'cue: every word starts there. Click a mark to make it current; drag it to move it; double-click empty space to add one; '
    'double-click a mark to remove it. CUE, the keys C3 to G3, a Routine slot or WALK change the cue point; the hand moves '
    'there in the next gap, with the fader closed. BUILT-IN goes back to the test record. The file path and cue points are '
    'saved with the Live set; if the file has moved, the built-in record is used and the status line says FILE MISSING.', P)]
s += [Paragraph('The sixteen words', H2), tbl([
    ['Key', 'Word', 'Record hand', 'Fader hand', 'One unit'],
    ['C1', 'BABY', 'Push, pull', 'Open', '2 strokes'],
    ['C#1', 'FORWARD', 'Let go, record plays', 'Cut at the end, pull back closed', '2 strokes'],
    ['D1', 'STAB', 'Fast push', 'Open for the first part', '1 stroke'],
    ['D#1', 'CHIRP', 'Push, pull', 'Closes on the push, opens on the pull', '2 strokes'],
    ['E1', '1-FLARE', 'Push, pull', 'One click shut in each stroke', '4 strokes'],
    ['F1', '2-FLARE', 'Push, pull', 'Two clicks in each stroke', '4 strokes'],
    ['F#1', 'CRAB', 'Push, pull', 'Four finger taps on each stroke', '4 strokes'],
    ['G1', 'TEAR', 'Push, then the pull in two halves', 'Open', '3 strokes and a rest'],
    ['G#1', 'SCRIBBLE', 'Tight shake, about 15 a second', 'Open', '2 strokes long'],
    ['A1', 'TRANSFORM', 'Slow drag out and back', 'Closed, tapped open in rhythm', '8 strokes long'],
    ['A#1', 'DRAG', 'Slow push and pull', 'Open', '4 strokes'],
    ['B1', 'TIP', 'Tiny strokes on the very front', 'Open', '2 strokes'],
    ['C2', 'LAZER', 'Hard flick, slow pull', 'Open', '2 strokes'],
    ['C#2', 'RELEASE', 'Let go: the sound plays', 'Open until the key comes up', 'while held'],
    ['D2', 'ECHO', 'Forwards', 'Each one quieter, up to six', '2 strokes'],
    ['D#2', 'REVERSE', 'Push silently, pull', 'Opens for the pull only', '2 strokes'],
], [12 * mm, 22 * mm, 50 * mm, 58 * mm, 30 * mm])]
s += [Spacer(1, 4), Paragraph(
    'A stroke lasts one SPEED step (1/8, 1/16, 1/16 triplet, 1/32). Hold a key and the word repeats unit after unit, on the grid; '
    'let go and the unit that is playing finishes. A new key takes over at the end of the current unit. A key starts on the next '
    'QUANT step. Velocity sets how far the hand moves and how loud the fader opens. A stroke shorter than 45 ms cannot be played, '
    'so a word that would need one uses the next slower speed. The panel buttons do the same as the keys. '
    'Works with the transport stopped.', P)]
s += [PageBreak(), Paragraph('What the hands do', H2), Paragraph(
    'Each row is one word: record position against time in beats, heavy where the fader is open. (Drawn from the reference engine.)', P),
    Image(os.path.join(ROOT, 'docs', 'word-charts.png'), width=240 * mm * 1210 / 4290, height=240 * mm)]
s += [PageBreak(), Paragraph('Controls', H2), tbl([
    ['Section', 'Control', 'What it does'],
    ['RECORD', 'strip, DROP SOUND, CUE, BUILT-IN', 'See above.'],
    ['WORDS', '16 buttons', 'Play a word, as the keys do.'],
    ['', 'ON switches', 'Which words ROUTINE and INSTINCT may use. A cell is used only if all its words are on.'],
    ['', 'SPEED', 'Stroke length for the keys and buttons. Routine and Instinct always use 16ths.'],
    ['', 'QUANT', 'When a key starts: next 1/16, 1/8, beat, or OFF (at once).'],
    ['PLAY', 'MODE', 'HANDS: only plays when played. ROUTINE: plays the 16-beat Routine. INSTINCT: improvises. A held key takes over; the mode carries on at the next bar line.'],
    ['', 'ROUTINE...', 'Opens the Routine: 16 slots of one beat, each a cell and a cue point (CUE SAME keeps the current one).'],
    ['', 'PHRASE ON / OFF', 'Scratch for ON bars, then rest for OFF bars. Leave room for a vocal or answer it.'],
    ['INSTINCT', 'DENSITY', 'How busy it is and how much it rests.'],
    ['', 'SKILL', 'Low: babies, forwards, drags. High: flares, crabs, tears, tips too.'],
    ['', 'MOTIF', 'How often the second bar repeats the first.'],
    ['', 'RELEASE', 'How often a phrase ends by letting the sound play (needs a beat of sound after the cue).'],
    ['', 'FUNK', 'Swing on the 16ths (50 % to 66 %) and how many triplet figures.'],
    ['', 'WALK', 'Chance per phrase of moving to another cue point.'],
    ['HANDS', 'HANDS', 'Moves all four below. 0 is a machine, 50 a tight DJ, 100 loose.'],
    ['', 'TIMING', 'Where strokes and fader moves land against the grid and against each other.'],
    ['', 'WEIGHT', 'How heavy the hand is: slower pick-up after a let-go, pushes that peak earlier.'],
    ['', 'SLOP', 'Uneven stroke lengths, missed cue points, a fader that closes late and lets a little through.'],
    ['', 'RISK', 'Outright fluffs: a click that does not close, a pull that goes back too far.'],
    ['OUT', 'LEVEL', 'Output level.'],
], [22 * mm, 34 * mm, 116 * mm])]
s += [Paragraph('Instinct', H2), Paragraph(
    'Instinct thinks in four-bar phrases: a bar, the same bar again (sometimes with one thing changed), a different bar, and an '
    'ending that often lets the sound play out. Each bar is made of one- and two-beat cells (a baby on the off-beat, three stabs, '
    'a chirp pair, a transform, and so on), picked by DENSITY and SKILL. That shape is why it sounds like someone scratching '
    'rather than random events.', P)]
s += [Paragraph('Recipes', H2), tbl([
    ['Recipe', 'Settings'],
    ['Answer a vocal', 'Instinct; switch off all but BABY, STAB and CHIRP; PHRASE 2 on, 2 off.'],
    ['Clean battle scratching', 'HANDS 20, SKILL 100, DENSITY 70, MOTIF 80.'],
    ['Bedroom practice', 'HANDS 85, SKILL 60. Listen for the late clicks.'],
    ['Transformer section', 'Hold A1 (TRANSFORM) at SPEED 1/16T over a half-time beat.'],
    ['Word cut-up', 'Drop a spoken phrase, WALK 80, RELEASE 30, so it hops between words.'],
], [45 * mm, 127 * mm])]
s += [Paragraph('Notes', H2), Paragraph(
    'Built offline and checked by measurement (the device engine against the reference engine the demos were approved on; '
    'playing speed flat within 0.3 dB to 16 kHz; a held record silent). It has not yet been run in Live. If it misbehaves, '
    'the Max Console (Max window) messages help most. Version history: 1.0.0, first release.', P)]
SimpleDocTemplate(os.path.join(ROOT, 'docs', 'DJ-Claude-Skratch-Manual.pdf'), pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                  topMargin=16 * mm, bottomMargin=16 * mm, title='DJ Claude Skratch').build(s)
print('docs/DJ-Claude-Skratch-Manual.pdf')
