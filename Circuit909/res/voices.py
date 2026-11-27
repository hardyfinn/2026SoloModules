# The 909 Circuits voice specs. One source of truth: res/make_voice_panels.py draws each
# panel from this AND writes src/VoiceSpecs.hpp, which the module code and engines read.
#
# knob: (ID, LABEL, tooltip, stock position 0..1, display, lo, hi, unit, row, col[, kind, norand])
#   display: pct | exp (lo*(hi/lo)^k) | lin (lo+(hi-lo)k) | int (rounded lin) | sw (switch)
#   kind: knob (default) | sw3 | sw2 ; switch labels go in unit, bottom first, '|' separated
# cv: (LABEL, knob ID or 'VOCT'); since 2.3.0 only V/OCT (Hardy: V/OCT, TRIG, ACCENT, OUT)
#
# Stock values are fitted to the 909's published behaviour (service-notes block diagram,
# commonly quoted pitches and times); they are not read from component values.
import math

def pos_exp(v, lo, hi): return math.log(v / lo) / math.log(hi / lo)
def pos_lin(v, lo, hi): return (v - lo) / (hi - lo)

COMMON_STOCK = lambda c_acc, c_lvl, c_age: [
    ("ACCENT", "ACCENT", "Accent (the 909's TOTAL ACCENT: how much louder an accented hit is)", 0.6, "pct", 0, 100, "%", 0, c_acc),
    ("LEVEL", "LEVEL", "Level", 1.0, "pct", 0, 100, "%", 0, c_lvl, "knob", True),
    ("AGE", "AGE", "Age (0 = new unit on spec; up = this unit's part tolerances, hit-to-hit drift, worn caps at the top)", 0.0, "pct", 0, 100, "%", 0, c_age, "knob", True),
]

ORANGE = "#E5631F"

VOICES = [
  dict(slug="Kick909", cls="Kick", title="909 KICK", sub="TR-909 bass drum circuit", color=ORANGE,
    sections=[[("STOCK 909", 0, 6)], [("PITCH", 0, 2), ("CLICK", 3, 6)], [("BODY", 0, 1)]],
    flow=["PITCH ENV", "VCO", "SHAPER", "VCA", "+ CLICK"],
    note="click = pulse + filtered noise (ATTACK)   ·   VCO reset on every hit",
    knobs=[
      ("TUNE", "TUNE", "Tune (the 909's TUNE: how long the pitch sweep takes)", pos_exp(16, 4, 64), "exp", 4, 64, " ms", 0, 0),
      ("ATTACK", "ATTACK", "Attack (the 909's ATTACK: level of the click)", 0.5, "pct", 0, 100, "%", 0, 1),
      ("DECAY", "DECAY", "Decay (the 909's DECAY: body length)", pos_exp(500, 60, 4000), "exp", 60, 4000, " ms", 0, 2),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("BASE", "BASE", "Base (the note the sweep lands on; a trimmer on the 909)", pos_exp(52, 25, 140), "exp", 25, 140, " Hz", 1, 0),
      ("SWEEP", "SWEEP", "Sweep (how far above BASE the hit starts)", pos_lin(1.8, 0, 4), "lin", 0, 4, " oct", 1, 1),
      ("BEND", "BEND", "Bend (a slower last drop into the note)", pos_lin(0.15, 0, 1.5), "lin", 0, 1.5, " oct", 1, 2),
      ("PULSE", "PULSE", "Pulse (the click's square pulse)", 0.5, "pct", 0, 100, "%", 1, 3),
      ("NOISE", "NOISE", "Noise (the click's noise burst)", 0.5, "pct", 0, 100, "%", 1, 4),
      ("NOISELP", "NOISE LP", "Noise LP (low-pass on the click noise)", pos_exp(5000, 800, 16000), "exp", 800, 16000, " Hz", 1, 5),
      ("SNAP", "SNAP", "Snap (length of the click: the noise burst, and the pulse up to 1 ms)", pos_exp(5, 0.5, 40), "exp", 0.5, 40, " ms", 1, 6),
      ("SHAPE", "SHAPE", "Shape (the waveshaper: sine <-> triangle <-> raw ramp)", 0.15, "pct", 0, 100, "%", 2, 0),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 2, 1),
    ],
    cv=[("V/OCT", "VOCT")]),

  dict(slug="Snare909", cls="Snare", title="909 SNARE", sub="TR-909 snare drum circuit", color=ORANGE,
    sections=[[("STOCK 909", 0, 6)], [("OSCILLATORS", 0, 5)], [("NOISE", 0, 1), ("OUT", 2, 2)]],
    flow=["TRIG", "2x VCO", "VCA", "+ NOISE", "OUT"],
    note="two swept VCOs plus filtered noise on its own VCA (SNAPPY)",
    knobs=[
      ("TUNE", "TUNE", "Tune (both oscillators; the lower sits near 185 Hz)", pos_exp(185, 90, 400), "exp", 90, 400, " Hz", 0, 0),
      ("TONE", "TONE", "Tone (low-pass on the snare noise)", pos_exp(5500, 1200, 16000), "exp", 1200, 16000, " Hz", 0, 1),
      ("SNAPPY", "SNAPPY", "Snappy (noise level)", 0.5, "pct", 0, 100, "%", 0, 2),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("RATIO", "RATIO", "Ratio (upper / lower oscillator; stock about 1.78)", pos_lin(1.78, 1.2, 3.0), "lin", 1.2, 3.0, "x", 1, 0),
      ("BALANCE", "BALANCE", "Balance (lower <-> upper oscillator)", 0.45, "pct", 0, 100, "%", 1, 1),
      ("DECAY", "DECAY", "Decay (oscillator body length)", pos_exp(170, 30, 2000), "exp", 30, 2000, " ms", 1, 2),
      ("SWEEP", "SWEEP", "Sweep (how far above pitch each hit starts)", pos_lin(0.4, 0, 2), "lin", 0, 2, " oct", 1, 3),
      ("BEND", "BEND", "Bend (how long the sweep takes)", pos_exp(10, 2, 120), "exp", 2, 120, " ms", 1, 4),
      ("SHAPE", "SHAPE", "Shape (the waveshaper: sine <-> triangle <-> raw ramp)", 0.3, "pct", 0, 100, "%", 1, 5),
      ("SNAPDECAY", "SNAP DEC", "Snap decay (length of the noise)", pos_exp(240, 30, 1500), "exp", 30, 1500, " ms", 2, 0),
      ("NOISECOL", "COLOUR", "Colour (high-pass corner on the noise)", pos_exp(700, 150, 8000), "exp", 150, 8000, " Hz", 2, 1),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 2, 2),
    ],
    cv=[("V/OCT", "VOCT")]),

  dict(slug="Tom909", cls="Tom", title="909 TOM", sub="TR-909 tom circuits", color=ORANGE,
    sections=[[("STOCK 909", 0, 6)], [("DRUM", 0, 5)], [("SKIN", 0, 0), ("OUT", 1, 1)]],
    flow=["PITCH ENV", "VCO", "VCA", "+ NOISE", "OUT"],
    note="LOW / MID / HIGH are the 909's three tom circuits",
    knobs=[
      ("TUNE", "TUNE", "Tune (around the 909's pitch for this tom)", 0.5, "lin", -1, 1, " oct", 0, 0),
      ("DECAY", "DECAY", "Decay (times the 909's ring for this tom)", 0.5, "exp", 0.2, 5, "x", 0, 1),
      ("RANGE", "RANGE", "Tom", 1.0, "sw", 0, 2, "LOW|MID|HIGH", 0, 2, "sw3"),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("SWEEP", "SWEEP", "Sweep (how far above pitch each hit starts)", pos_lin(0.5, 0, 2.5), "lin", 0, 2.5, " oct", 1, 0),
      ("BEND", "BEND", "Bend (how long the sweep takes: tom drop <-> long zap)", pos_exp(45, 5, 600), "exp", 5, 600, " ms", 1, 1),
      ("SHAPE", "SHAPE", "Shape (the waveshaper: sine <-> triangle <-> raw ramp)", 0.15, "pct", 0, 100, "%", 1, 2),
      ("NOISE", "NOISE", "Noise (the skin noise under the tone)", 0.25, "pct", 0, 100, "%", 1, 3),
      ("DOUBLE", "DOUBLE", "Double (second, detuned VCO: beating)", 0.0, "pct", 0, 100, "%", 1, 4),
      ("TONE", "TONE", "Tone (output low-pass)", 0.75, "exp", 300, 14000, " Hz", 1, 5),
      ("MUTE", "MUTE", "Mute (a hand resting on the head)", 0.0, "pct", 0, 100, "%", 2, 0),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 2, 1),
    ],
    cv=[("V/OCT", "VOCT")]),

  dict(slug="Rim909", cls="Rim", title="909 RIM", sub="TR-909 rim shot circuit", color=ORANGE,
    sections=[[("STOCK 909", 0, 6)], [("RESONATORS + VCA", 0, 5)], []],
    flow=["TRIG", "3x BRIDGED-T", "VCA", "HIGH-PASS", "OUT"],
    note="three high resonators gated by a fast transistor VCA",
    knobs=[
      ("TUNE", "TUNE", "Tune (all three resonators; the lowest sits near 500 Hz)", 0.5, "lin", -1, 1, " oct", 0, 0),
    ] + COMMON_STOCK(1, 2, 3) + [
      ("RATIO", "RATIO", "Ratio (spread of the upper two resonators; stock 2.2x)", pos_lin(2.2, 1.3, 5), "lin", 1.3, 5, "x", 1, 0),
      ("RING", "RING", "Ring (resonator decay)", pos_exp(30, 5, 800), "exp", 5, 800, " ms", 1, 1),
      ("GATE", "GATE", "Gate (how long the VCA stays open)", pos_exp(6, 1, 100), "exp", 1, 100, " ms", 1, 2),
      ("SNAP", "SNAP", "Snap (high-pass corner on the crack; moves with TUNE)", pos_exp(700, 100, 2500), "exp", 100, 2500, " Hz", 1, 3),
      ("STRIKE", "STRIKE", "Strike (below 1 ms: harder trigger; above: soft mallet)", 0.5, "exp", 0.1, 10, " ms", 1, 4),
      ("DRIVE", "DRIVE", "Drive (VCA and op-amp into saturation)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("V/OCT", "VOCT")]),

  dict(slug="Clap909", cls="Clap", title="909 CLAP", sub="TR-909 hand clap circuit", color=ORANGE,
    sections=[[("FILTER", 0, 2), ("STOCK 909", 3, 6)], [("CLAPS", 0, 5)], []],
    flow=["NOISE", "BAND-PASS", "BURSTS + TAIL", "TONE", "OUT"],
    note="sawtooth bursts plus a reverb envelope on band-passed noise",
    knobs=[
      ("BAND", "BAND", "Band (band-pass centre: big hands <-> small, papery)", pos_exp(1200, 400, 5000), "exp", 400, 5000, " Hz", 0, 0),
      ("RES", "RES", "Res (band-pass Q)", pos_lin(1.3, 0.5, 8), "lin", 0.5, 8, "", 0, 1),
      ("TONE", "TONE", "Tone (output low-pass)", pos_exp(9000, 1000, 16000), "exp", 1000, 16000, " Hz", 0, 2),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("BURSTS", "BURSTS", "Bursts (claps per hit: 1 = a snap, 8 = a small crowd)", pos_lin(4, 1, 8), "int", 1, 8, "", 1, 0),
      ("SPREAD", "SPREAD", "Spread (time between bursts: tight machine <-> loose hands)", pos_exp(9, 2, 60), "exp", 2, 60, " ms", 1, 1),
      ("SNAP", "SNAP", "Snap (decay of each burst)", pos_exp(3.5, 1, 30), "exp", 1, 30, " ms", 1, 2),
      ("TAIL", "TAIL", "Tail (reverb envelope length)", pos_exp(320, 30, 3000), "exp", 30, 3000, " ms", 1, 3),
      ("MIX", "MIX", "Mix (bursts <-> tail)", 0.55, "pct", 0, 100, "%", 1, 4),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[]),

  dict(slug="Cymbals909", cls="Cymbals", title="909 CYMBALS", sub="TR-909 6-bit hat and cymbal path", color=ORANGE,
    sections=[[("STOCK 909", 0, 6)], [("SOURCE", 0, 1), ("DIGITAL", 2, 3), ("VCA + CUT", 4, 5)], [("OUT", 0, 0)]],
    flow=["SOURCE", "6-BIT DAC", "RECON", "VCA", "OUT"],
    note="built-in metal source or your own WAV (right-click)",
    extra_trig="OPEN",
    knobs=[
      ("TUNE", "TUNE", "Tune (the sample clock: pitch and grit together)", 0.5, "lin", -1, 1, " oct", 0, 0),
      ("DECAY", "DECAY", "Decay (times this type's stock length: open hat 0.7 s, crash 2.2 s, ride 1.6 s)", pos_exp(1.0, 0.1, 5.0), "exp", 0.1, 5.0, "x", 0, 1),
      ("TYPE", "TYPE", "Type", 0.0, "sw", 0, 2, "HAT|CRASH|RIDE", 0, 2, "sw3"),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("METAL", "METAL", "Metal (pitch of the built-in metal source)", 0.5, "lin", -1, 1, " oct", 1, 0),
      ("NOISE", "NOISE", "Noise (noise in the built-in source)", 0.35, "pct", 0, 100, "%", 1, 1),
      ("BITS", "BITS", "Bits (the 909 used 6)", pos_lin(6, 2, 12), "int", 2, 12, "", 1, 2),
      ("RECON", "RECON", "Recon (the DAC's reconstruction low-pass)", pos_exp(11000, 1500, 20000), "exp", 1500, 20000, " Hz", 1, 3),
      ("CHDECAY", "CH DEC", "Closed-hat decay", pos_exp(45, 5, 400), "exp", 5, 400, " ms", 1, 4),
      ("CUT", "CUT", "Cut (output high-pass)", pos_exp(250, 30, 8000), "exp", 30, 8000, " Hz", 1, 5),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 2, 0),
    ],
    cv=[("V/OCT", "VOCT")]),
]

# ---- 16 HP layout (2.3.0, Hardy picked "panel B"): rows 0-1 are big knobs in 3 columns
# (row 1 col 2 is HIT), rows 2-4 small knobs in 4 columns. RAND and NEAR STOCK sit in the
# last two columns of the first small row that has both free. BAND labels the big rows.
PLACE = {
  "Kick909": dict(band="STOCK 909", big=[["TUNE", "ATTACK", "DECAY"], ["ACCENT", "LEVEL"]],
                  small=[["BASE", "SWEEP", "BEND", "SHAPE"], ["PULSE", "NOISE", "NOISELP", "SNAP"], ["DRIVE", "AGE"]]),
  "Snare909": dict(band="STOCK 909", big=[["TUNE", "TONE", "SNAPPY"], ["ACCENT", "LEVEL"]],
                   small=[["RATIO", "BALANCE", "DECAY", "SHAPE"], ["SWEEP", "BEND", "SNAPDECAY", "NOISECOL"], ["DRIVE", "AGE"]]),
  "Tom909": dict(band="STOCK 909", big=[["TUNE", "DECAY", "RANGE"], ["ACCENT", "LEVEL"]],
                 small=[["SWEEP", "BEND", "SHAPE", "NOISE"], ["DOUBLE", "TONE", "MUTE", "DRIVE"], ["AGE"]]),
  "Rim909": dict(band="MAIN", big=[["TUNE", "RING", "GATE"], ["ACCENT", "LEVEL"]],
                 small=[["RATIO", "SNAP", "STRIKE", "DRIVE"], ["AGE"]]),
  "Clap909": dict(band="MAIN", big=[["BAND", "TONE", "TAIL"], ["ACCENT", "LEVEL"]],
                  small=[["RES", "BURSTS", "SPREAD", "SNAP"], ["MIX", "DRIVE"], ["AGE"]]),
  "Cymbals909": dict(band="STOCK 909", big=[["TUNE", "DECAY", "TYPE"], ["ACCENT", "LEVEL"]],
                     small=[["CHDECAY", "METAL", "NOISE", "BITS"], ["RECON", "CUT", "DRIVE", "AGE"]]),
}
for _v in VOICES:
    _p = PLACE[_v["slug"]]
    _rc = {}
    for _r, _row in enumerate(_p["big"]):
        for _c, _id in enumerate(_row): _rc[_id] = (_r, _c)
    for _r, _row in enumerate(_p["small"]):
        for _c, _id in enumerate(_row): _rc[_id] = (2 + _r, _c)
    _ids = [k[0] for k in _v["knobs"]]
    assert sorted(_ids) == sorted(_rc), (_v["slug"], set(_ids) ^ set(_rc))
    _new = []
    for _k in _v["knobs"]:
        _k = list(_k); _k[8], _k[9] = _rc[_k[0]]; _new.append(tuple(_k))
    _v["knobs"] = _new
    # RAND / NEAR STOCK: last two columns of the first small row that has both free
    _used = {}
    for _id, (_r, _c) in _rc.items(): _used.setdefault(_r, set()).add(_c)
    _v["rand_row"] = next(_r for _r in (2, 3, 4) if not ({2, 3} & _used.get(_r, set())))
    _v["band"] = _p["band"]
