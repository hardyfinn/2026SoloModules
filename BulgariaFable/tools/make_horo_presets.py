#!/usr/bin/env python3
# Writes presets/Horo/NN_Name.vcvm from the table in docs/HORO-BUILD-INSTRUCTIONS.md section 6.
import os, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
SLOTS = ["PRAVO", "PAIDUSHKO", "RUCHENITSA", "LESNOTO", "ELENINO", "DAICHOVO", "GRANCHARSKO", "KOPANITSA", "PETRUNINO",
         "KRIVO", "BUCHIMISH", "YOVE", "SANDANSKO", "SEDI DONKA", "USER 1", "USER 2", "USER 3", "USER 4"]
BARS = [1, 2, 3, 4, 6, 8, 12, 16]
PHRASE = {"LB": 0, "LL": 1, "LP": 2}
USER_DEFAULT = ["3+3+2", "2+3+3", "3+2+3", "2+2+2+2"]
# name, METRE, BARS, LILT, ACCENT, phrase, user 1 grouping (or None)
P = [
    ("Init (Ruchenitsa)", "RUCHENITSA", 4, 0, 0.8, "LB", None),
    ("Ruchenitsa Played", "RUCHENITSA", 4, 0.5, 0.9, "LB", None),
    ("Paidushko Limp", "PAIDUSHKO", 4, 0.6, 1, "LB", None),
    ("Lesnoto Slow", "LESNOTO", 2, 0.4, 0.7, "LB", None),
    ("Daichovo", "DAICHOVO", 4, 0.3, 0.8, "LB", None),
    ("Kopanitsa", "KOPANITSA", 4, 0.3, 0.8, "LL", None),
    ("Krivo", "KRIVO", 4, 0.2, 0.8, "LL", None),
    ("Buchimish", "BUCHIMISH", 2, 0.2, 0.8, "LB", None),
    ("Sedi Donka", "SEDI DONKA", 2, 0.2, 0.8, "LB", None),
    ("Machine Seven", "RUCHENITSA", 8, 0, 1, "LP", None),
    ("Squared Off", "KOPANITSA", 4, -1, 0.6, "LB", None),
    ("Techno 3 3 2", "USER 1", 8, 0.15, 1, "LB", "3+3+2"),
]
if __name__ == "__main__":
    outdir = os.path.join(ROOT, "presets", "Horo")
    os.makedirs(outdir, exist_ok=True)
    for f in os.listdir(outdir):
        if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
    for i, (name, metre, bars, lilt, acc, ph, u1) in enumerate(P):
        vals = [SLOTS.index(metre), BARS.index(bars), lilt, acc, 0]
        users = list(USER_DEFAULT)
        if u1: users[0] = u1
        data = {"user%d" % (j + 1): users[j] for j in range(4)}
        data.update(phraseMode=PHRASE[ph], changeAtPhrase=False)
        doc = {"plugin": "BulgariaFable", "model": "Horo", "version": "2.1.0",
               "params": [{"value": float(v), "id": j} for j, v in enumerate(vals)], "data": data}
        json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
    print("wrote", len(P), "presets")
