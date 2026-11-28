#!/usr/bin/env python3
"""Random scenarios for the newer engines: firmware simulator vs the JavaScript port.
usage: fuzz.py <indexed|discrete|phaseflux|fractal> [count] [seed]
Writes ref/<engine>/sN.txt, runs both, compares note-for-note (+-1 tick)."""
import os, random, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BIN = os.path.join(ROOT, "src/xformer/build-sim/src/tests/unit/sequencer/TestXfRef")
engine = sys.argv[1]
count = int(sys.argv[2]) if len(sys.argv) > 2 else 50
rnd = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)
r = rnd.randrange
def maybe(p=0.5): return rnd.random() < p


def indexed():
    L = ["engine indexed", "ticks %d" % rnd.choice([6000, 12000])]
    n = r(1, 13)
    L += ["scale %d" % r(14), "rootNote %d" % r(12), "activeLength %d" % n]
    if maybe(0.3): L.append("scaleRotate %d" % r(4))
    if maybe(0.5): L.append("runMode %d" % r(6))
    if maybe(0.3): L.append("loop 0")
    if maybe(0.4): L.append("firstStep %d" % r(n))
    if maybe(0.3): L.append("divisor %d" % rnd.choice([1, 2, 3, 4]))
    if maybe(0.3): L.append("clockMult %d" % r(50, 151))
    if maybe(0.3): L += ["syncMode 1", "resetMeasure %d" % r(1, 4)]
    L += ["octave %d" % r(-1, 2), "transpose %d" % r(-5, 6)]
    for i in range(n):
        d = 0 if maybe(0.1) else rnd.choice([24, 48, 96, 144, 192, r(1, 300)])
        L += ["step %d duration %d" % (i, d), "step %d gateLength %d" % (i, r(0, d + 20) if d else 0),
              "step %d noteIndex %d" % (i, r(-14, 15))]
        if maybe(0.2): L.append("step %d slide 1" % i)
    return L


def discrete():
    L = ["engine discrete", "ticks %d" % rnd.choice([6000, 12000])]
    L += ["scale %d" % r(14), "rootNote %d" % r(12)]
    L.append("clockSource %d" % r(2))
    if maybe(0.5): L.append("playMode 0")
    # the simulator only takes Input/Scanner through routing, so external mode is tested at 0 V
    if maybe(0.15): L += ["clockSource 2"]
    L.append("divisor %d" % rnd.choice([12, 24, 48, 96, 192]))
    if maybe(0.3): L.append("clockMult %d" % r(50, 151))
    L.append("gateLength %d" % rnd.choice([0, 0, 25, 50, 90, r(101)]))
    if maybe(0.3): L.append("loop 0")
    if maybe(0.4): L += ["syncMode 1", "resetMeasure %d" % r(1, 4)]
    else: L.append("resetMeasure %d" % rnd.choice([0, 1, 2, 8]))
    if maybe(0.3): L.append("thresholdMode 1")
    if maybe(0.3): L.append("rangeHigh %d" % r(-500, 501))
    if maybe(0.3): L.append("rangeLow %d" % r(-500, 501))
    L += ["octave %d" % r(-1, 2), "transpose %d" % r(-5, 6)]
    for i in range(32):
        if maybe(0.4):
            L += ["stage %d direction %d" % (i, r(4)), "stage %d noteIndex %d" % (i, r(-14, 15))]
            if maybe(0.5): L.append("stage %d threshold %d" % (i, r(-100, 101)))
    return L


def phaseflux():
    L = ["engine phaseflux", "ticks %d" % rnd.choice([6000, 12000])]
    L += ["scale %d" % r(14), "rootNote %d" % r(12)]
    L.append("divisor %d" % rnd.choice([3, 6, 12, 12, 24]))
    if maybe(0.3): L.append("clockMult %d" % r(50, 151))
    if maybe(0.3): L.append("cycleLength 1")
    if maybe(0.3): L.append("traversal %d" % r(8))
    if maybe(0.3): L.append("pitchMode 1")
    if maybe(0.3): L.append("pitchRate %d" % r(17))
    if maybe(0.2): L.append("globalPhase %d" % r(100))
    for k in ["warpNudge", "responseNudge", "cyclePhaseWarp"]:
        if maybe(0.15): L.append("%s %d" % (k, r(-40, 41)))
    if maybe(0.15): L.append("pulseNudge %d" % r(-3, 4))
    if maybe(0.15): L.append("lenNudge %d" % r(-2, 3))
    if maybe(0.2): L.append("resetMeasure %d" % r(1, 4))
    if maybe(0.2): L += ["firstStage %d" % r(3), "lastStage %d" % r(3, 16)]
    for pre in ["noteAcc", "pulseAcc"]:
        if maybe(0.4):
            L += ["%sScope %d" % (pre, r(2)), "%sOrder %d" % (pre, r(4)), "%sPolarity %d" % (pre, r(2)),
                  "%sReset %d" % (pre, r(4)), "%sPos %d" % (pre, r(1, 12)), "%sNeg %d" % (pre, r(1, 12))]
    L += ["octave %d" % r(-1, 2), "transpose %d" % r(-5, 6)]
    for i in range(16):
        if i >= 4 and not maybe(0.3): continue
        S = lambda f, v: L.append("stage %d %s %d" % (i, f, v))
        S("skip", 0 if maybe(0.85) else 1)
        S("pulseCount", r(0, 9))
        S("length", rnd.choice([1, 2, 4, 4, 8, r(1, 20)]))
        if maybe(0.5): S("basePitch", r(-10, 11))
        if maybe(0.4): S("pitchRange", r(4))
        if maybe(0.4): S("pitchDirection", r(3))
        if maybe(0.4): S("pitchCurve", r(4))
        if maybe(0.2): S("pitchFlipV", 1)
        if maybe(0.2): S("pitchFlipH", 1)
        if maybe(0.3): S("pitchWarp", r(-60, 61))
        if maybe(0.3): S("pitchResponse", r(-60, 61))
        if maybe(0.4): S("temporalCurve", r(3))
        if maybe(0.2): S("temporalFlipV", 1)
        if maybe(0.2): S("temporalFlipH", 1)
        if maybe(0.3): S("temporalWarp", r(-60, 61))
        if maybe(0.3): S("temporalResponse", r(-60, 61))
        if maybe(0.2): S("maskMelody", r(101))
        if maybe(0.2): S("tiltMelody", r(101))
        if maybe(0.2): S("phaseShift", r(8))
        if maybe(0.3): S("mask", r(8))
        if maybe(0.2): S("maskShift", r(8))
        if maybe(0.3): S("accumulatorStep", r(-5, 6))
        if maybe(0.3): S("pulseAccumStep", r(-3, 4))
        if maybe(0.2): S("accumulatorTrigger", 1)
        if maybe(0.2): S("pulseAccumTrigger", 1)
        if maybe(0.5): S("gateLength", rnd.choice([0, 1, 10, 50, 50, 90, 100]))
        if maybe(0.2): S("temporalRepeat", r(4))
        if maybe(0.2): S("pitchRepeat", r(4))
        if maybe(0.2): S("temporalWindow", r(5))
        if maybe(0.2): S("pitchWindow", r(5))
    return L


def fractal():
    # the source: one of the Note-track scenarios, on track 0
    src = os.path.join(ROOT, "ref/no/s%d.txt" % r(1, 101))
    L = ["engine fractal", "ticks %d" % rnd.choice([12000, 24000]), "sourceA 0"]
    L += ["p " + l for l in open(src).read().split("\n") if l and not l.startswith(("engine", "ticks"))]
    L += ["scale %d" % r(14), "rootNote %d" % r(12), "record %d" % (0 if maybe(0.1) else 1)]
    L.append("divisor %d" % rnd.choice([6, 12, 12, 24, 48]))
    if maybe(0.3): L.append("clockMult %d" % r(50, 151))
    if maybe(0.3): L.append("resetMeasure %d" % r(1, 5))
    lf = r(0, 8) if maybe(0.3) else 0
    L += ["loopFirst %d" % lf, "loopLast %d" % (lf + rnd.choice([3, 7, 15, r(0, 30)]))]
    if maybe(0.3): L.append("rotate %d" % r(-8, 9))
    if maybe(0.4): L.append("orderMode %d" % r(7))
    if maybe(0.3): L.append("recordSkip %d" % r(4))
    if maybe(0.3):
        rf = r(0, 8); L += ["recordFirst %d" % rf, "recordLast %d" % (rf + r(0, 24))]
    if maybe(0.3): L += ["ornFirst %d" % r(0, 8), "ornLast %d" % r(4, 32)]
    L.append("recordMode %d" % r(2))
    L.append("cadence %d" % r(2))
    L.append("fidelity %d" % r(2))
    if maybe(0.5):
        L += ["branchCount %d" % r(0, 8), "path %d" % r(256), "branchSeed %d" % r(65536)]
        if maybe(0.5): L.append("branchPool %d" % r(256))
    if maybe(0.6): L += ["ornRate %d" % rnd.choice([20, 50, 100, r(101)]), "ornIntensity %d" % r(101)]
    if maybe(0.15): L.append("lock 1")
    if maybe(0.3): L.append("quantize 1")
    if maybe(0.3): L.append("trackDelay %d" % r(0, 6))
    if maybe(0.3): L.append("scaleRotate %d" % r(4))
    L += ["octave %d" % r(-1, 2), "transpose %d" % r(-5, 6)]
    return L


GEN = {"indexed": indexed, "discrete": discrete, "phaseflux": phaseflux, "fractal": fractal}
out = os.path.join(ROOT, "ref", engine)
os.makedirs(out, exist_ok=True)
env = dict(os.environ, XF_FROZEN_US="1")
ok = bad = 0
for i in range(count):
    f = os.path.join(out, "s%d.txt" % i)
    open(f, "w").write("\n".join(GEN[engine]()) + "\n")
    env["XF_SCENARIO"], env["XF_OUT"] = f, f + ".fw"
    p = subprocess.run([BIN], env=env, capture_output=True, text=True)
    unk = [l for l in p.stdout.splitlines() if l.startswith("unknown")]
    if unk: print("harness:", f, unk[:3])
    j = subprocess.run(["node", os.path.join(ROOT, "m4l/test/trace.js"), engine, f, f + ".js"], capture_output=True, text=True)
    if j.returncode:
        print("JS ERROR", f, j.stderr[-400:]); bad += 1; continue
    c = subprocess.run(["python3", os.path.join(ROOT, "m4l/test/cmp.py"), f + ".fw", f + ".js", "1"], capture_output=True, text=True)
    if c.returncode == 0: ok += 1
    else:
        bad += 1
        print("FAIL", f, c.stdout.splitlines()[:2])
print("pass %d fail %d" % (ok, bad))
