"""Comper: the 12 factory presets and the built-in progressions. Part of the design.
Chart text uses the parse_chart syntax; each preset says how many beats one chart step lasts.
Any parameter not named keeps its default (see Params in comper_ref.py)."""

# name, beats per step, chart (one entry per step, max 16 steps)
PROGRESSIONS = [
    ('ii-V-I major',        4, 'Dm7 G7 Cmaj7 Cmaj7'),
    ('ii-V-i minor',        4, 'Dm7b5 G7alt Cm6 Cm6'),
    ('Major then minor',    4, 'Dm7 G7 Cmaj7 Cmaj7 Dm7b5 G7alt Cm6 Cm6'),
    ('Jazz blues in F',     2, 'F7 F7 Bb7 Bb7 F7 F7 Cm7 F7 Bb7 Bb7 Bdim7 Bdim7 F7 F7 Am7 D7'),   # first 8 bars
    ('Blues (12 steps)',    4, 'F7 Bb7 F7 F7 Bb7 Bdim7 F7 D7 Gm7 C7 F7 C7'),
    ('Falling fifths',      4, 'Cm7 F7 Bbmaj7 Ebmaj7 Am7b5 D7alt Gm6 Gm6'),
    ('Turnaround I-vi-ii-V',2, 'Cmaj7 Am7 Dm7 G7'),
    ('Rhythm bridge',       4, 'D7 D7 G7 G7 C7 C7 F7 F7'),
    ('Modal two-chord',     4, 'Dm7 Dm7 Dm7 Dm7 Ebm7 Ebm7 Dm7 Dm7'),
    ('Bossa',               4, 'Fmaj7 Fmaj7 G7 G7 Gm7 C7 Fmaj7 C7'),
    ('Ballad',              2, 'Ebmaj7 Cm7 Fm7 Bb7 Gm7 C7alt Fm7 Bb7 Ebmaj7 Ebmaj7 Am7b5 D7alt Gm7 C7 Fm7 Bb7sus'),
    ('Soul vamp',           4, 'Em7 A7 Em7 A7 Cmaj7 Bm7 Am7 B7alt'),
    ('House vamp',          4, 'Am7 Am7 Fmaj7 Em7 Dm7 Dm7 Fmaj7 E7alt'),
    ('One chord',           4, 'Dm7'),
]

# style: 0 SHELL 1 ROOTLESS 2 QUARTAL 3 SPREAD 4 MIXED;  groove: 0 SWING 1 BOSSA 2 BALLAD 3 SOUL 4 HOUSE
PRESETS = [
    ('01_Medium Swing',      dict(prog='Falling fifths', tempo=132, groove=0, style=1, density=0.5,  loose=1.0)),
    ('02_Fast Bebop Shells', dict(prog='Rhythm bridge', tempo=208, groove=0, style=0, density=0.6,  loose=0.9, voices=3, register=67, spread=0.0)),
    ('03_Ballad',            dict(prog='Ballad', tempo=62, groove=2, style=3, density=0.4,  loose=1.2, pedal=0.8, spread=0.5, register=71)),
    ('04_Bossa',             dict(prog='Bossa', tempo=126, groove=1, style=1, density=0.5,  loose=0.9, repeat=0.8)),
    ('05_Modal Quartal',     dict(prog='Modal two-chord', tempo=176, groove=0, style=2, density=0.75, loose=1.0, register=72, repeat=0.45)),
    ('06_Open Spread',       dict(prog='Major then minor', tempo=116, groove=0, style=3, density=0.4,  loose=1.1, spread=0.7, register=72, out=0.2)),
    ('07_Blues Shells',      dict(prog='Blues (12 steps)', tempo=120, groove=0, style=0, density=0.55, loose=1.2, out=0.25, voices=3)),
    ('08_Soul Sixteenths',   dict(prog='Soul vamp', tempo=94, groove=3, style=1, density=0.55, loose=0.9, repeat=0.8)),
    ('09_House Stabs',       dict(prog='House vamp', tempo=122, groove=4, style=3, density=0.5,  loose=0.4, repeat=0.95, push=0.0, spread=0.3)),
    ('10_Out And Sparse',    dict(prog='ii-V-i minor', tempo=138, groove=0, style=4, density=0.25, loose=1.3, out=0.9, repeat=0.3)),
    ('11_Solo Piano Roots',  dict(prog='Turnaround I-vi-ii-V', tempo=88, groove=2, style=3, density=0.55, loose=1.2, roots=2, pedal=0.6, spread=0.6)),
    ('12_Duo Listening',     dict(prog='Falling fifths', tempo=132, groove=0, style=1, density=0.6,  loose=1.0, listen=1.0)),
]
INIT = dict(prog='ii-V-I major', tempo=120, groove=0, style=1, density=0.5, loose=1.0)
