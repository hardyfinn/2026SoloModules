"""Rider presets, paired by number and name with Comper's and Walker's twelve. player = RP fields, kit = kit controls (see rider_kit_ref.KitParams)."""
from rider_ref import SWING, BOSSA, BALLAD, SOUL, HOUSE
PRESETS = [
    ('01_Medium Swing',      dict(feel=SWING, energy=0.5, comp=0.5),                                        dict()),
    ('02_Fast Bebop Shells', dict(feel=SWING, energy=0.6, comp=0.45, feather=0.3, ahead=0.6),               dict(decay=0.45)),
    ('03_Ballad',            dict(feel=BALLAD, energy=0.4, comp=0.5, loose=1.1),                            dict()),
    ('04_Bossa',             dict(feel=BOSSA, energy=0.45, repeat=0.8),                                     dict(decay=0.45)),
    ('05_Modal Quartal',     dict(feel=SWING, energy=0.7, comp=0.65, fills=0.6, feather=0.2),               dict(bright=0.55)),
    ('06_Open Spread',       dict(feel=SWING, energy=0.35, comp=0.35, feather=0.6),                         dict(decay=0.6)),
    ('07_Blues Shells',      dict(feel=SWING, energy=0.55, comp=0.55, fills=0.7, loose=1.2),                dict(age=0.3)),
    ('08_Soul Sixteenths',   dict(feel=SOUL, energy=0.55, comp=0.5, repeat=0.8, loose=0.9),                 dict(decay=0.4, tune=0.4)),
    ('09_House Stabs',       dict(feel=HOUSE, energy=0.6, comp=0.4, fills=0.4, loose=0.4),                  dict(decay=0.35, tune=0.35, bright=0.55)),
    ('10_Out And Sparse',    dict(feel=SWING, energy=0.3, comp=0.7, repeat=0.2, feather=0.2, loose=1.3),    dict(age=0.45)),
    ('11_Solo Piano Roots',  dict(feel=BALLAD, energy=0.3, comp=0.3, loose=1.2),                            dict(age=0.5)),
    ('12_Duo Listening',     dict(feel=SWING, energy=0.45, comp=0.6, listen=1.0),                           dict()),
]
INIT = (dict(), dict())     # every default; on the panel FEEL starts at FOLLOW
