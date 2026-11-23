"""Builds docs/Doubles-Manual.pdf."""
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
    t = Table([[Paragraph(str(c), C) for c in r] for r in rows], colWidths=w)
    t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#2c2c2e')), ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                           ('GRID', (0, 0), (-1, -1), 0.3, colors.grey), ('VALIGN', (0, 0), (-1, -1), 'TOP')]))
    return t
s = []
s += [Paragraph('Doubles', H1), Paragraph('Beat juggling for Ableton Live (Max for Live audio effect)', P), Spacer(1, 6)]
s += [Image(os.path.join(ROOT, 'docs', 'panel-preview.png'), width=180 * mm, height=180 * mm * 338 / 2224), Spacer(1, 6)]
s += [Paragraph('What it is', H2), Paragraph(
    'Beat juggling is the hip-hop DJ craft of playing two copies of the same record on two decks and using the crossfader '
    'and both hands to loop, double, chase and rebuild a break. Doubles does this to a short piece you record into it. '
    'Both decks are modelled turntables. A record takes time to start, pulling it back takes time, and the hands that cue '
    'it are not perfect. The HANDS section sets how perfect they are.', P)]
s += [Paragraph('Signal flow', H2), Paragraph(
    'Input &rarr; RECORD (1 to 16 bars, starting on a bar line) &rarr; the record &rarr; deck A and deck B (two copies; B '
    'is a little worn, set by COPY) &rarr; crossfader &rarr; soft limiter &rarr; LEVEL &rarr; output. With no record, or while '
    'recording, the input passes straight through. Once a record exists, the device is the sound source and the input is ignored.', P)]
s += [Paragraph('Controls', H2), tbl([
    ['Section', 'Control', 'What it does'],
    ['RECORD', 'RECORD', 'Arms. Recording starts at the next bar line while Live is playing. FREE length stops at the bar line after you switch it off.'],
    ['', 'LENGTH', '1, 2, 4, 8 or 16 bars, or FREE.'],
    ['', 'CLEAR', 'Forgets the record (the file on disk is kept).'],
    ['', 'DROP AUDIO', 'Drop an audio file to use instead of recording. Its length in bars is guessed from the tempo.'],
    ['STRIP', 'waveform', 'Ticks are stickers (the hits). Yellow and red flags are the kick and snare a fill will use. Orange and blue lines are the needles of decks A and B. The bar under it shows which fader is open. LATE flashes when a hand misses.'],
    ['', 'FILL KICK / SNARE', '0 = AUTO: each fill uses the kick and snare of the bar it starts in. Otherwise the sticker number.'],
    ['MOVES', 'LOOP ... PHASE', 'On/off. The move starts at the next QUANT step and stops at the next step after you switch it off. The newest one wins.'],
    ['', 'TRIPLE, PAUSE, SPIN, DRAG, BABY', 'One-shots. TRIPLE: the next beat three times. PAUSE: hold the record, let go late, rejoin at the bar. SPIN: backspin through the last beat of the bar. DRAG: a finger on the platter, which slows the record and then catches up. BABY: the record that is not playing scratches the next hit, ending on the beat.'],
    ['', 'LOOP, DOUBLE, CHASE, PAUSE, SPIN menus', 'Loop length; how far behind the double runs; chase step; pause length; how far the spin goes back.'],
    ['', 'SHUFFLE, FILL PATTERN, QUANT', 'Swing of the shuffle strobe (0 to 60 %); which of 8 fill patterns; where a move button starts.'],
    ['PADS', '1 to 4 / 5 to 8', 'Play a sticker now: kick, snare, kick 2, snare 2. Pads 1 to 4 on deck A, 5 to 8 on deck B. A hit lasts until the next hit or one beat.'],
    ['HANDS', 'HANDS', 'Sets the four below to the same value. 0 is a machine, 50 a tight DJ (default), 100 a loose one.'],
    ['', 'TIMING', 'How far releases and cuts stray. A late hand stays late for a few moves. Fast passages get looser.'],
    ['', 'WEIGHT', 'Heavy hands: slower starts, a small push or drag on each release, slower pull-backs.'],
    ['', 'SLOP', 'Missed cue points (worse the further the pull), a sliver of pull-back through the fader, uneven levels, a trembling held record.'],
    ['', 'RISK', 'How often a hit is fluffed outright.'],
    ['', 'COPY', 'How worn the second copy is: a touch slower, duller and quieter. The decks drift apart and get nudged back.'],
    ['', 'SCRATCH', 'How often a baby scratch (one push through the hit, one pull back, fader open) is flicked in just before a record comes back. 0 is never, 30 an occasional habit (default), 100 every time there is room. A late hand never scratches, so loose hands scratch less than the dial says. Not moved by HANDS.'],
    ['', 'OVER', 'GROOVE: scratches are heard over the other record. SILENCE: the other record is cut while the scratch plays. MIX: about one in three is over silence (default).'],
    ['PLAY', 'MODE', 'HANDS-ON: you play the buttons. ROUTINE: the 16 slots play in turn. INSTINCT: the device chooses.'],
    ['', 'DENSITY, FUNK, ARC', 'Instinct: how busy, how syncopated, and over how many bars it builds up.'],
    ['', 'ROUTINE...', 'Opens the 16 slots, SLOT LENGTH (bar or half bar) and RESET.'],
    ['OUT', 'STRAIGHT', 'Level of the plain record between moves. At 0 you hear only the moves.'],
    ['', 'LEVEL, BRAKE', 'Output level. BRAKE: on stop, the record is heard slowing down.'],
], [22 * mm, 38 * mm, 115 * mm])]
s += [PageBreak(), Paragraph('The moves', H2), tbl([
    ['Move', 'What the DJ does', 'What you hear'],
    ['Loop', 'Plays a phrase on A, cuts to B at the same start while A is pulled back.', 'The phrase repeats.'],
    ['Double', 'B runs a little behind A. The fader flicks over after each hit.', 'Kick-kick, snare-snare.'],
    ['Triple', 'As double, with a third hit.', 'Three of each, as a fill.'],
    ['Chase', 'Both records advanced a hit at a time, alternating decks.', 'Half-time, each hit twice.'],
    ['Shuffle', 'A chase with swing.', 'Syncopated chase.'],
    ['Pause', 'A hand stops the record on a hit and lets go late.', 'A hole, then the beat displaced.'],
    ['Fill', 'Kick on one deck, snare on the other, cued back hit by hit.', 'A new drum pattern from two hits.'],
    ['Spin', 'The record spun back with the fader open.', 'The rewind.'],
    ['Phase', 'Both decks open, a few milliseconds apart, one slightly fast.', 'Flanging that drifts.'],
    ['Drag', 'A finger on the platter edge.', 'A brief slow-down that settles back.'],
], [22 * mm, 85 * mm, 68 * mm])]
s += [Paragraph('Recipes', H2)]
for t, b in [
    ('1. Extend a break', 'Record 2 bars of the break. Leave MODE on HANDS-ON. Turn LOOP on at 1/2 for the last bar of each 4, then DOUBLE at 1/8 for a bar. Turn it off before the drop.'),
    ('2. Half-time breakdown', 'CHASE at 1/4 for 4 bars, then SHUFFLE at 50 % for 2. With HANDS at 70 it loosens up like a real chase.'),
    ('3. Fill generator over the original', 'Keep the original clip playing on another track. Record it into Doubles, set STRAIGHT to 0, MODE to INSTINCT, DENSITY 40. Doubles only speaks during its moves.'),
    ('4. New beat from two hits', 'Map pads 1 and 2 to two keys or pads. Turn STRAIGHT down and play your own pattern from the kick and snare. FILL plays the written patterns the same way.'),
    ('5. Flanged bar', 'PHASE on for one bar before a drop. Raise COPY for a wider sweep.'),
    ('6. A written routine', 'Open ROUTINE... and set slots 1 to 16, for example STRAIGHT, LOOP, DOUBLE, CHASE, STRAIGHT, TRIPLE, FILL 2, SPIN. Set MODE to ROUTINE and press RESET a bar before you want it to start.'),
    ('7. Juggle something else', 'Record a bass line, a vocal phrase or a horn stab instead of drums. Doubles and chases work on anything rhythmic. Turn FILL KICK and SNARE to fixed stickers.'),
    ('8. Clean slicer', 'HANDS to 0 and COPY to 0: every cut is sample-exact and nothing drifts. Good for edits you then resample.'),
    ('9. Practice-room sound', 'HANDS to 100 with RISK at 30: late releases, slips and the odd fluffed hit.'),
    ('10. Loop with pick-ups', 'LOOP at 1/4 (one beat), SCRATCH at 100: every return gets a baby scratch. Set OVER to SILENCE for bare scratches between the beats.'),
    ('11. Scratch over the groove', 'Leave the record playing straight and tap BABY just before beats 2 and 4. OVER on GROOVE keeps the beat under it; SILENCE punches a hole for each scratch.'),
]:
    s += [Paragraph('<b>%s.</b> %s' % (t, b), P), Spacer(1, 3)]
s += [Paragraph('Playing it from a controller', H2), Paragraph(
    'An audio effect does not receive MIDI notes, so use Live\'s MIDI Map mode. Press Cmd-M, click a button or pad on '
    'Doubles, press a key or pad on your controller, then press Cmd-M again. The move buttons and pads can also be automated '
    'in an arrangement, which is how a routine can be drawn in.', P)]
s += [Paragraph('Good to know', H2), Paragraph(
    'Doubles assumes 4/4. Changing the tempo after recording repitches the record, like a turntable. The record is saved as a '
    'WAV next to the device, and the Live set remembers which one. If that file is moved, the strip shows RECORD MISSING. '
    'Several Doubles in one set each keep their own record.', P)]
s += [Paragraph('Version history', H2), Paragraph(
    '1.1.0: baby scratches (SCRATCH, OVER, BABY). A late record is now dropped in where the music has got to instead of '
    'being sped up to catch up; drift corrections are at most 3 %. Fixed: the hands dials always played fully loose; deck A '
    'ran at the wrong speed when Live\'s tempo differed from the recording tempo. Scripts are stored inside the device.<br/>'
    '1.0.0: first release.', P)]
SimpleDocTemplate(os.path.join(ROOT, 'docs', 'Doubles-Manual.pdf'), pagesize=A4, leftMargin=16 * mm, rightMargin=16 * mm,
                  topMargin=14 * mm, bottomMargin=14 * mm, title='Doubles Manual').build(s)
print('docs/Doubles-Manual.pdf')
