"""Factory presets, written in real units and converted to knob values.  The C++ presets must match."""
from cicada_ref import *
def P_(mech, hz, q, stroke, rate, chirp, gap=0.2, song=STEADY, length=-1, body=0.0, open_=0.5, loose=0.4, dist=0.15, temp=0.5):
    st = k_ribs(stroke) if mech == TYMBAL else (k_duty(stroke) if mech == FILE else stroke)
    return dict(MECH=mech, PITCH=k_pitch(hz), PURE=k_q(q), STROKE=st, RATE=k_rate(rate), CHIRP=k_chirp(chirp), GAP=k_gap(gap),
                SONG=song, LENGTH=k_length(length), BODY=body, OPEN=open_, LOOSE=loose, DISTANCE=dist, TEMP=temp)
PRESETS = {
 # name                 mech    Hz     Q   stroke rate chirp gap
 'Field Cricket':    P_(FILE,   4541, 22, 0.58, 25.4, 5, 0.175, loose=0.5),
 'Tree Cricket':     P_(FILE,   3116, 22, 0.50, 64.6, 0, loose=0.25),
 'Ground Cricket':   P_(FILE,   2343, 20, 0.55, 24.2, 21, 0.60, loose=0.45),
 'Katydid':          P_(FILE,   3900, 5,  0.42, 5.4, 3, 0.62, loose=0.6, body=0.5, open_=0.72),
 'Meadow Katydid':   P_(FILE,  11500, 5,  0.55, 62, 0, song=TICKBUZZ, length=6.0, loose=0.4, body=0.4, open_=0.72, dist=0.08),
 'Grasshopper':      P_(FILE,   6200, 3.7, 0.64, 14.1, 0, song=RISE, length=2.1, loose=0.5, body=0.4, open_=0.8),
 'Dog-Day Cicada':   P_(TYMBAL, 6500, 8,  3, 220, 0, song=PULSATE, length=11, body=0.65, open_=0.44, loose=0.35),
 'Pharaoh Cicada':   P_(TYMBAL, 1340, 14, 7, 132, 0, song=SLUR, length=2.6, loose=0.15),
 'Tick-Buzz Cicada': P_(TYMBAL, 5600, 6,  3, 190, 0, song=TICKBUZZ, length=4.2, loose=0.4),
 'Evening Cicada':   P_(TYMBAL, 4100, 10, 4, 160, 13, 0.13, song=SINK, length=4.6, body=0.75, open_=0.53, loose=0.3, dist=0.3),
 'Min-Min Cicada':   P_(TYMBAL, 4300, 5,  3, 175, 30, 0.25, song=TALK, length=5.5, body=0.9, open_=0.64, loose=0.45, dist=0.2),
 'Rasp Cicada':      P_(TYMBAL, 4900, 4.5, 2, 210, 16, 0.14, loose=0.9),
 'Greengrocer':      P_(TYMBAL, 4300, 12, 4, 240, 0, song=SWELL, length=14, body=0.5, open_=0.5, loose=0.25),
 'Mosquito':         P_(WING,   1040, 3,  0.5, 520, 0, loose=0.5, dist=0.05),
 'Bumblebee':        P_(WING,    520, 2.1, 0.5, 172, 0, loose=0.9, dist=0.05),
 # stretched
 'Cricket Hats':     P_(FILE,   7200, 14, 0.5, 45, 2, loose=0.1, dist=0.1),          # TRIG in, 16ths
 'Tymbal Bass':      P_(TYMBAL,  700, 7,  3, 55, 0, loose=0.02, dist=0.0),           # V/OCT to RATE, envelope to PITCH
 'Leaf Shaker':      P_(FILE,   9000, 4,  0.7, 30, 4, 0.1, loose=0.3, dist=0.2),     # CLOCK in
 'Giant':            P_(TYMBAL,  420, 9,  5, 38, 9, 0.35, song=SINK, length=6, body=0.7, open_=0.55, loose=0.4, dist=0.25),
 'Sine Night':       P_(FILE,   1200, 30, 0.5, 12, 3, 0.4, loose=0.2, dist=0.35),
}
PRESETS['Tymbal Bass']['RATE'] = 0.5      # V/OCT switch at RATE: the knob is a transposer, centre = none
PRESETS['Tymbal Bass']['TRACK'] = 1
