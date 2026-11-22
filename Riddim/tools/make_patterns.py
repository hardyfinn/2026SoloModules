#!/usr/bin/env python3
"""Source of the 24 riddim patterns -> src/RiddimPatterns.hpp.   python3 tools/make_patterns.py
Patterns are reconstructions of each riddim's feel by ear, NOT transcriptions.
A step list is "step[:level]" with step 0-15 (16ths) and level 1 ghost, 2 normal, 3 accent.
TOM level means pitch (1 low, 2 mid, 3 high).
Kit numbers: 0 CASIO, 1 ROLAND 808, 2 ROLAND PCM, 3 LINN/OBERHEIM, 4 SIMMONS, 5 SAMPLER, 6 WORKSTATION, 7 LIVE."""
import os
TR = ["KICK", "SNARE", "RIM", "CLAP", "TOM", "CHAT", "OHAT", "COW", "PERC", "SHAKE"]

def steps(s):
    out = [0] * 16
    for tok in s.split():
        a, _, b = tok.partition(":")
        out[int(a)] = int(b) if b else 2
    return out

H8 = "0 2 4 6 8 10 12 14"
H8OFF = "2 6 10 14"
def h16(ghost=1):
    return " ".join(f"{i}:{2 if i % 2 == 0 else ghost}" for i in range(16))
def z16():
    return " ".join(f"{i}:1" for i in range(16))
FILLT = "12:1 13:2 14:3"

# name, year, producer, machine label, kit, confirmed, A dict, B additions dict
R = []
def riddim(name, year, producer, machine, kit, conf, A, B):
    R.append((name, year, producer, machine, kit, conf, A, B))

riddim("UNDER MI SLENG TENG", "1985", "KING JAMMY", "CASIO MT-40 ROCK", 0, True,
       dict(K="0:3 7 10", S="4 12", H=H8), dict(K="3:1", T=FILLT, S="15:1"))
riddim("RUMOURS (TELEPHONE LOVE)", "1988", "GUSSIE CLARKE", "OBERHEIM DX ?", 3, False,
       dict(K="0:3 5:1 8 11:1", S="4 12", R="6:1", C="12:1", H=h16(1), O="10"), dict(P="2 7 10 15", T="14:1 15:2"))
riddim("PUNAANY", "1987", "JAMMYS / STEELY+CLEVIE", "ROLAND TR ?", 1, False,
       dict(K="0:3 3 10", S="8:3", R="4:1 12:1", H=H8, O="6 14", Z=z16()), dict(K="6:1", T="12:1 13:1 14:2 15:3"))
riddim("FISH MARKET", "1989", "STEELY + CLEVIE", "ROLAND TR ?", 1, False,
       dict(K="0 4 8 12", S="4:3 12:3", R="14:1", H=H8OFF, W="0:1 8:1"), dict(S="7:1 15:1", T="13:1 14:2 15:3"))
riddim("DEM BOW", "1990", "SHABBA RANKS RIDDIM", "ROLAND R-8 ?", 2, False,
       dict(K="0 4 8 12", S="3 6 11 14", H=H8, O="2:1 10:1"), dict(C="3 11", T="13:1 14:2 15:3"))
riddim("BAM BAM", "1992", "SLY DUNBAR", "SIMMONS ?", 4, False,
       dict(K="0:3 6 8 14:1", S="4 12", R="10", W="2 10", H=h16(1)), dict(T="11:1 12:1 13:2 14:2 15:3"))
riddim("STALAG 17", "1973", "WINSTON RILEY", "LIVE (80s: CASIO ?)", 7, True,
       dict(K="0:3 8 10:1", S="4 12", H=H8, Z=z16()), dict(S="14:1 15:1", T="13:1 14:1 15:2"))
riddim("REAL ROCK", "1967", "STUDIO ONE", "LIVE", 7, True,
       dict(K="8:3", R="8:3", H=H8, O="6 14", Z="0:1 4:1 8:1 12:1"), dict(K="0:1", R="4:1 12:1", Z="2:1 6:1 10:1 14:1"))
riddim("SHOWTIME", "1997", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0:3 3:1 8 11 14:1", S="4 12", C="12", T="6:1 7:1 10:2", P="2 5 9 13", H="0 4 8 12"), dict(S="15", T="13:2 14:3 15:3"))
riddim("JOYRIDE", "c.1997-2000", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0 4 8 12", S="6 14", C="6:1 14:1", H=h16(1), W="3 11"), dict(T=FILLT, S="10:1"))
riddim("BOGLE", "c.1997-2000", "DAVE KELLY", "SAMPLER ?", 5, False,
       dict(K="0:3 5 8 13:1", S="4 12", R="2:1 10:1", P="6 7 14 15", H=H8), dict(S="11:1", T="13:1 14:2 15:3"))
riddim("PEPPER SEED", "c.1997-2000", "DAVE KELLY", "SAMPLER ?", 5, False,
       dict(K="0:3 10", S="4 12 15:1", C="4:1", W="0 3 6 9 12", H=h16(1)), dict(K="7:1", T="13:2 14:2 15:3"))
riddim("THE BUG", "1999", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0 3 8 11", S="4 12", R="6 14", Z=z16(), O="10"), dict(T="12:1 13:2 14:2 15:3", K="14:1"))
riddim("HEARTBEAT", "c.1999-2001", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0:3 3 8:3 11", S="6 14", C="14:1", H=H8), dict(T="12:3 13:2 14:1 15:1"))
riddim("BOUNCE", "2000", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0 4 8 12", S="2:1 4 10:1 12", C="12:1", H=H8, O="14"), dict(S="15:1", T="13:1 14:2 15:3"))
riddim("FIESTA", "2003", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0:3 6 8 14:1", S="4 12", P="0 3 6 9 12 15", W="2 10", Z=z16()), dict(T=FILLT, P="1:1 13:1"))
riddim("ARAB", "c.2000-2003", "DAVE KELLY", "SAMPLER ?", 5, False,
       dict(K="0:3 5 8 13", S="4 12", P="2 3 6 10 11 14", R="7:1", H=H8), dict(S="15:1", T="13:2 14:2 15:3"))
riddim("DIWALI", "2002", "LENKY", "SAMPLER ?", 5, False,
       dict(K="0:3 8 10", C="3 6 11 14", H=h16(1), W="2 5 10 13"), dict(T=FILLT, K="14:1"))
riddim("MASTERPIECE", "c.2002-2003", "LENKY", "SAMPLER ?", 5, False,
       dict(K="0:3 7 8 10", S="4 12", C="4:1 12:1", H=H8OFF, W="0:1 6:1 12:1"), dict(T="12:1 13:2 14:3", S="15"))
riddim("TIME TRAVEL", "c.2000-2004", "LENKY", "SAMPLER ?", 5, False,
       dict(K="0 6 8 10", S="4 12", R="2 14", P="3 11", H=h16(1)), dict(T="13:3 14:2 15:1"))
riddim("EIGHTY-FIVE", "2006", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0:3 3 8:3 11 14:1", S="6 14", C="6:1", H=h16(1), O="2:1"), dict(T="12:1 13:2 14:3 15:3"))
riddim("MEDICINE", "c.2000-2005", "DAVE KELLY", "WORKSTATION ?", 6, False,
       dict(K="0 4 8 12", S="3 6 11 14", C="3:1 11:1", H=h16(1)), dict(C="14", T="13:1 14:2 15:3"))
riddim("STINK", "c.2000-2005", "DAVE KELLY", "SAMPLER ?", 5, False,
       dict(K="0:3 7 10", S="4 12", R="15:1", T="13:1 14:1", H=H8), dict(P="6 14", S="15:1"))
riddim("RAE RAE", "c.2000-2005", "DAVE KELLY", "SAMPLER ?", 5, False,
       dict(K="0 3 8 11", S="6 14 15:1", W=H8, H=H8OFF, O="12"), dict(T="12:1 13:1 14:2 15:3"))

assert len(R) == 24
KEYS = dict(K=0, S=1, R=2, C=3, T=4, H=5, O=6, W=7, P=8, Z=9)

def grid(d):
    g = [[0] * 16 for _ in range(10)]
    for k, v in d.items():
        g[KEYS[k]] = steps(v)
    return g

def addB(a, b):
    g = grid(a)
    for k, v in b.items():
        for i, x in enumerate(steps(v)):
            if x: g[KEYS[k]][i] = x
    return g

out = ["// Generated by tools/make_patterns.py. Do not edit by hand.", "#pragma once", "#include <cstdint>", "",
       "namespace riddim {", "static const int NUM_RIDDIMS = 24;", "static const int NUM_TRACKS = 10;", "static const int NUM_STEPS = 16;",
       "struct RiddimDef {", "\tconst char* name; const char* year; const char* producer; const char* machine;",
       "\tint kit; bool confirmed;", "\tuint8_t a[10][16]; uint8_t b[10][16];", "};", "static const RiddimDef RIDDIMS[NUM_RIDDIMS] = {"]
def cg(g):
    return "{" + ",".join("{" + ",".join(map(str, r)) + "}" for r in g) + "}"
for name, year, prod, mach, kit, conf, A, B in R:
    out.append(f'\t{{"{name}", "{year}", "{prod}", "{mach}", {kit}, {"true" if conf else "false"},\n\t {cg(grid(A))},\n\t {cg(addB(A, B))}}},')
out.append("};")
out.append("static const char* const TRACK_NAMES[10] = {" + ",".join(f'"{t}"' for t in TR) + "};")
out.append("}")
here = os.path.dirname(os.path.abspath(__file__))
open(os.path.join(here, "..", "src", "RiddimPatterns.hpp"), "w").write("\n".join(out) + "\n")
print("wrote", len(R), "riddims")
