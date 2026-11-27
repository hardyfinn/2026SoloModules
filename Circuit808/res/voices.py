# The 808 Circuits voice specs (all modules except 808 Kick, which has its own files).
# One source of truth: res/make_voice_panels.py draws each panel from this AND writes
# src/VoiceSpecs.hpp, which the module code and the engines read.
#
# knob: (ID, LABEL, tooltip, stock position 0..1, display, lo, hi, unit, row, col[, kind, norand])
#   display: pct | exp (lo*(hi/lo)^k) | lin (lo+(hi-lo)k) | int (rounded lin) | sw (switch)
#   kind: knob (default) | sw3 | sw2 ; switch labels go in unit, bottom first, '|' separated
# cv: (LABEL, knob ID or 'VOCT')
import math

def pos_exp(v, lo, hi): return math.log(v / lo) / math.log(hi / lo)
def pos_lin(v, lo, hi): return (v - lo) / (hi - lo)

COMMON_STOCK = lambda c_acc, c_lvl, c_age: [
    ("ACCENT", "ACCENT", "Accent level (how far an accented hit goes from 4 V towards 14 V)", 0.6, "pct", 0, 100, "%", 0, c_acc),
    ("LEVEL", "LEVEL", "Level", 1.0, "pct", 0, 100, "%", 0, c_lvl, "knob", True),
    ("AGE", "AGE", "Age (0 = new unit on spec; up = this unit's part tolerances, hit-to-hit drift, worn caps at the top)", 0.0, "pct", 0, 100, "%", 0, c_age, "knob", True),
]

VOICES = [
  dict(slug="Snare808", cls="Snare", title="808 SNARE", sub="TR-808 snare drum circuit", color="#F08A24",
    sections=[[("STOCK 808", 0, 6)], [("SHELLS", 0, 3), ("OUT", 4, 4)], [("SNAPPY", 0, 2)]],
    flow=["TRIG", "2 SHELLS", "+ NOISE", "VCA", "OUT"],
    note="two tuned shells (TONE) plus filtered white noise (SNAPPY)",
    knobs=[
      ("TUNE", "TUNE", "Tune (low head; the 808 sits near 180 Hz)", pos_exp(180, 90, 360), "exp", 90, 360, " Hz", 0, 0),
      ("TONE", "TONE", "Tone (balance of the two shells, as on the 808: low <-> high)", 0.5, "pct", 0, 100, "%", 0, 1),
      ("SNAPPY", "SNAPPY", "Snappy (noise level)", 0.5, "pct", 0, 100, "%", 0, 2),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("RATIO", "RATIO", "Ratio (high shell / low shell; stock about 1.83)", pos_lin(330.0 / 180.0, 1.2, 3.0), "lin", 1.2, 3.0, "x", 1, 0),
      ("HISS", "HISS", "Hiss (low-pass on the snappy noise)", 0.5, "exp", 1000, 12000, " Hz", 2, 2),
      ("DECAY", "DECAY", "Decay (shell ring time)", pos_exp(220, 40, 2000), "exp", 40, 2000, " ms", 1, 1),
      ("SWEEP", "SWEEP", "Sweep (attack pitch jump of the shells; not on the 808)", 0.0, "lin", 0, 2, " oct", 1, 2),
      ("STRIKE", "STRIKE", "Strike (below 1 ms: shorter, harder pulse; above: rounded edges, softer; level held)", 0.5, "exp", 0.1, 10, " ms", 1, 3),
      ("SNAPDECAY", "SNAP DEC", "Snap decay (length of the noise)", pos_exp(250, 30, 1500), "exp", 30, 1500, " ms", 2, 0),
      ("NOISECOL", "COLOUR", "Noise colour (high-pass corner on the noise)", pos_exp(1000, 200, 8000), "exp", 200, 8000, " Hz", 2, 1),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 4),
    ],
    cv=[("V/OCT", "VOCT"), ("TONE", "TONE"), ("SNAPPY", "SNAPPY"), ("RATIO", "RATIO"), ("DECAY", "DECAY"), ("SNAP DEC", "SNAPDECAY"), ("HISS", "HISS")]),

  dict(slug="Clap808", cls="Clap", title="808 CLAP", sub="TR-808 hand clap circuit", color="#E0533D",
    sections=[[("FILTER", 0, 2), ("STOCK 808", 3, 6)], [("CLAPS", 0, 5)], []],
    flow=["NOISE", "BAND-PASS", "BURSTS + TAIL", "TONE", "OUT"],
    note="sawtooth bursts plus a reverb envelope on band-passed noise",
    knobs=[
      ("BAND", "BAND", "Band (band-pass centre: big hands <-> small, papery)", pos_exp(1000, 400, 4000), "exp", 400, 4000, " Hz", 0, 0),
      ("RES", "RES", "Res (band-pass Q)", pos_lin(1.5, 0.5, 8), "lin", 0.5, 8, "", 0, 1),
      ("TONE", "TONE", "Tone (output low-pass)", 0.75, "exp", 1000, 16000, " Hz", 0, 2),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("BURSTS", "BURSTS", "Bursts (claps per hit: 1 = a snap, 8 = a small crowd)", pos_lin(4, 1, 8), "int", 1, 8, "", 1, 0),
      ("SPREAD", "SPREAD", "Spread (time between bursts: tight machine <-> loose hands)", pos_exp(10, 2, 60), "exp", 2, 60, " ms", 1, 1),
      ("SNAP", "SNAP", "Snap (decay of each burst)", pos_exp(4, 1, 30), "exp", 1, 30, " ms", 1, 2),
      ("TAIL", "TAIL", "Tail (reverb envelope length)", pos_exp(250, 30, 3000), "exp", 30, 3000, " ms", 1, 3),
      ("MIX", "MIX", "Mix (bursts <-> tail)", 0.5, "pct", 0, 100, "%", 1, 4),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("BAND", "BAND"), ("SPREAD", "SPREAD"), ("TAIL", "TAIL"), ("BURSTS", "BURSTS"), ("MIX", "MIX"), ("DRIVE", "DRIVE")]),

  dict(slug="TomConga808", cls="Tom", title="808 TOM / CONGA", sub="TR-808 tom and conga circuit", color="#E8C547",
    sections=[[("STOCK 808", 0, 6)], [("DRUM", 0, 5)], [("NOISE", 0, 0), ("OUT", 1, 1)]],
    flow=["TRIG", "SHAPER", "BRIDGED-T", "+ NOISE", "OUT"],
    note="dots show the stock tom; a stock conga has SLAP at zero",
    knobs=[
      ("TUNE", "TUNE", "Tune (around the 808's pitch for this range)", 0.5, "lin", -1, 1, " oct", 0, 0),
      ("RANGE", "RANGE", "Range", 1.0, "sw", 0, 2, "LOW|MID|HIGH", 0, 1, "sw3"),
      ("MODE", "MODE", "Mode", 0.0, "sw", 0, 1, "TOM|CONGA", 0, 2, "sw2"),
    ] + COMMON_STOCK(3, 4, 5) + [
      ("DECAY", "DECAY", "Decay (times the 808's ring for this drum)", 0.5, "exp", 0.2, 5, "x", 1, 0),
      ("BEND", "BEND", "Bend (downward pitch sweep: tom drop <-> Syndrum zap)", 0.05, "lin", 0, 2, " oct", 1, 1),
      ("STRIKE", "STRIKE", "Strike (below 1 ms: shorter, harder pulse; above: rounded edges, softer; level held)", 0.5, "exp", 0.1, 10, " ms", 1, 2),
      ("MUTE", "MUTE", "Mute (a hand resting on the head)", 0.0, "pct", 0, 100, "%", 1, 3),
      ("DOUBLE", "DOUBLE", "Double (second, detuned head: beating)", 0.0, "pct", 0, 100, "%", 1, 4),
      ("TONE", "TONE", "Tone (output low-pass)", 0.7, "exp", 300, 12000, " Hz", 1, 5),
      ("SLAP", "SLAP", "Slap (pink-noise layer; stock tom about 30 %, conga 0)", 0.3, "pct", 0, 100, "%", 2, 0),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 2, 1),
    ],
    cv=[("V/OCT", "VOCT"), ("DECAY", "DECAY"), ("BEND", "BEND"), ("MUTE", "MUTE"), ("DOUBLE", "DOUBLE"), ("SLAP", "SLAP"), ("DRIVE", "DRIVE")]),

  dict(slug="Rim808", cls="Rim", title="808 RIM", sub="TR-808 rim shot circuit", color="#F08A24",
    sections=[[("STOCK 808", 0, 6)], [("RESONATORS + VCA", 0, 5)], []],
    flow=["TRIG", "2x BRIDGED-T", "VCA", "HIGH-PASS", "OUT"],
    note="two high resonators gated by a fast transistor VCA",
    knobs=[
      ("TUNE", "TUNE", "Tune (both resonators; the 808 sits near 455 Hz and 1.67 kHz)", 0.5, "lin", -1, 1, " oct", 0, 0),
    ] + COMMON_STOCK(1, 2, 3) + [
      ("RATIO", "RATIO", "Ratio (upper / lower resonator; stock about 3.66)", pos_lin(1667.0 / 455.0, 1.5, 6), "lin", 1.5, 6, "x", 1, 0),
      ("RING", "RING", "Ring (resonator decay)", pos_exp(40, 5, 800), "exp", 5, 800, " ms", 1, 1),
      ("GATE", "GATE", "Gate (how long the VCA stays open)", pos_exp(8, 1, 100), "exp", 1, 100, " ms", 1, 2),
      ("SNAP", "SNAP", "Snap (high-pass corner on the crack)", pos_exp(1000, 100, 5000), "exp", 100, 5000, " Hz", 1, 3),
      ("STRIKE", "STRIKE", "Strike (below 1 ms: shorter, harder pulse; above: rounded edges, softer; level held)", 0.5, "exp", 0.1, 10, " ms", 1, 4),
      ("DRIVE", "DRIVE", "Drive (VCA and op-amp into saturation)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("V/OCT", "VOCT"), ("RATIO", "RATIO"), ("RING", "RING"), ("GATE", "GATE"), ("SNAP", "SNAP"), ("DRIVE", "DRIVE")]),

  dict(slug="Clave808", cls="Clave", title="808 CLAVE", sub="TR-808 claves circuit", color="#ECE6D8",
    sections=[[("STOCK 808", 0, 6)], [("RESONATOR", 0, 6)], []],
    flow=["TRIG", "SHAPER", "BRIDGED-T", "(+ OVERTONE)", "OUT"],
    note="one high bridged-T near 2.5 kHz with a very short ring",
    knobs=[
      ("TUNE", "TUNE", "Tune (the 808 sits near 2.5 kHz)", pos_lin(0, -2, 1), "lin", -2, 1, " oct", 0, 0),
    ] + COMMON_STOCK(1, 2, 3) + [
      ("RING", "RING", "Ring (decay, far past the stock click)", pos_exp(30, 5, 1500), "exp", 5, 1500, " ms", 1, 0),
      ("STRIKE", "STRIKE", "Strike (hard stick <-> soft mallet: rounded pulse edges; level held)", 0.5, "exp", 0.1, 10, " ms", 1, 1),
      ("BEND", "BEND", "Bend (downward pitch drop: log drum, water drop)", 0.0, "lin", 0, 2, " oct", 1, 2),
      ("OVERTONE", "OVERTONE", "Overtone (level of a second resonator)", 0.0, "pct", 0, 100, "%", 1, 3),
      ("RATIO", "RATIO", "Ratio (overtone / fundamental)", pos_lin(2.4, 1.2, 6), "lin", 1.2, 6, "x", 1, 4),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("V/OCT", "VOCT"), ("RING", "RING"), ("STRIKE", "STRIKE"), ("BEND", "BEND"), ("OVERTONE", "OVERTONE"), ("RATIO", "RATIO"), ("DRIVE", "DRIVE")]),

  dict(slug="Maracas808", cls="Maracas", title="808 MARACAS", sub="TR-808 maracas circuit", color="#E8C547",
    sections=[[("STOCK 808", 0, 6)], [("SHAKER", 0, 5)], []],
    flow=["NOISE", "HIGH-PASS", "VCA", "OUT"],
    note="high-passed white noise through a short envelope",
    knobs=COMMON_STOCK(0, 1, 2) + [
      ("LENGTH", "LENGTH", "Length (short tick <-> long swish)", pos_exp(60, 5, 1000), "exp", 5, 1000, " ms", 1, 0),
      ("CUT", "CUT", "Cut (high-pass corner)", pos_exp(5000, 500, 12000), "exp", 500, 12000, " Hz", 1, 1),
      ("GRAINS", "GRAINS", "Grains (smooth noise <-> sparse separate beads)", 0.0, "pct", 0, 100, "%", 1, 2),
      ("SHAKE", "SHAKE", "Shake (a second, softer hit later: back-and-forth)", 0.0, "pct", 0, 100, "%", 1, 3),
      ("ATTACK", "ATTACK", "Attack (envelope rise)", pos_exp(1.5, 0.1, 50), "exp", 0.1, 50, " ms", 1, 4),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("LENGTH", "LENGTH"), ("CUT", "CUT"), ("GRAINS", "GRAINS"), ("SHAKE", "SHAKE"), ("ATTACK", "ATTACK"), ("DRIVE", "DRIVE")]),

  dict(slug="Cowbell808", cls="Cowbell", title="808 COWBELL", sub="TR-808 cowbell circuit", color="#ECE6D8",
    sections=[[("STOCK 808", 0, 6)], [("OSCILLATORS + FILTER + VCA", 0, 5)], []],
    flow=["2x SQUARE", "BAND-PASS", "2-STAGE VCA", "OUT"],
    note="the 808's 540 Hz and 800 Hz squares, after Werner et al. (AES 2014)",
    knobs=[
      ("TUNE", "TUNE", "Tune (both oscillators; the 808 runs 540 Hz and 800 Hz)", 0.5, "lin", -1, 1, " oct", 0, 0),
      ("DECAY", "DECAY", "Decay (the long second stage)", pos_exp(400, 40, 3000), "exp", 40, 3000, " ms", 0, 1),
    ] + COMMON_STOCK(2, 3, 4) + [
      ("INTERVAL", "INTERVAL", "Interval (upper / lower oscillator; stock 800/540)", pos_lin(800.0 / 540.0, 1.0, 2.5), "lin", 1.0, 2.5, "x", 1, 0),
      ("WIDTH", "WIDTH", "Width (pulse width: hollow <-> nasal)", 0.5, "lin", 5, 95, "%", 1, 1),
      ("FILTER", "FILTER", "Filter (band-pass centre)", pos_exp(2640, 500, 8000), "exp", 500, 8000, " Hz", 1, 2),
      ("RES", "RES", "Res (band-pass Q)", pos_lin(2.0, 0.5, 10), "lin", 0.5, 10, "", 1, 3),
      ("SHAPE", "SHAPE", "Shape (where the envelope breaks: short clank <-> long ring)", 0.5, "pct", 0, 100, "%", 1, 4),
      ("DRIVE", "DRIVE", "Drive (output op-amp into its rails)", 0.0, "pct", 0, 100, "%", 1, 5),
    ],
    cv=[("V/OCT", "VOCT"), ("DECAY", "DECAY"), ("INTERVAL", "INTERVAL"), ("WIDTH", "WIDTH"), ("FILTER", "FILTER"), ("SHAPE", "SHAPE"), ("DRIVE", "DRIVE")]),
]
