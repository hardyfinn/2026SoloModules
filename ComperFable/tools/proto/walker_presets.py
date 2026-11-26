"""Walker presets, paired by number and name with Comper's twelve (presets.py). line = BP fields, tone = Tone fields."""
from walker_ref import WALK, TWO, BALLAD, BOSSA, SOUL, HOUSE
PRESETS = [
    ('01_Medium Swing',      dict(feel=WALK, activity=0.5),                                    dict()),
    ('02_Fast Bebop Shells', dict(feel=WALK, activity=0.35, ahead=0.6),                        dict(sustain=0.4)),
    ('03_Ballad',            dict(feel=BALLAD, activity=0.4, loose=1.1),                       dict(sustain=0.75)),
    ('04_Bossa',             dict(feel=BOSSA, activity=0.5, repeat=0.8),                       dict(bright=0.45)),
    ('05_Modal Quartal',     dict(feel=WALK, activity=0.6, roam=0.8),                          dict()),
    ('06_Open Spread',       dict(feel=TWO, activity=0.45, register=38.0),                     dict(sustain=0.65)),
    ('07_Blues Shells',      dict(feel=WALK, activity=0.65, out=0.35, loose=1.2),              dict(growl=0.5)),
    ('08_Soul Sixteenths',   dict(feel=SOUL, activity=0.55, repeat=0.8, loose=0.9),            dict(bright=0.65, pickup=0.5)),
    ('09_House Stabs',       dict(feel=HOUSE, activity=0.5, repeat=0.95, loose=0.4),           dict(bright=0.6, pickup=0.6, sustain=0.35)),
    ('10_Out And Sparse',    dict(feel=WALK, activity=0.25, out=0.8, roam=0.7, loose=1.3),     dict(age=0.45)),
    ('11_Solo Piano Roots',  dict(feel=BALLAD, activity=0.3, loose=1.2),                       dict(sustain=0.8, age=0.5, growl=0.45)),
    ('12_Duo Listening',     dict(feel=WALK, activity=0.55),                                   dict(pickup=0.35)),
]
INIT = (dict(), dict())     # WALK with every default; on the panel FEEL starts at FOLLOW
