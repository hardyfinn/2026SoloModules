#!/usr/bin/env python3
# Writes the factory presets (presets/*/*.vcvm) and test/presets.h (the same settings for the C++ demo renders).
# Parameter order must match the enums in src/Essphor.cpp, src/EssphorRes.cpp and src/EssphorMod.cpp
# (check_enums() below reads them to make sure).
import os, json, math, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
VERSION = '2.0.0'

GRAIN_PARAMS = ['PITCH', 'RATE', 'SIZE', 'CONTOUR', 'SCAN', 'SPRAY', 'PATTERN', 'WET',
                'FEEDBACK', 'RAND_PITCH', 'RAND_RATE', 'RAND_SIZE', 'RAND_AMP', 'RAND_REV', 'AGE',
                'SCALE', 'TEMPO', 'LOCK', 'RATE_MODE', 'SCAN_MODE', 'SPRAY_MODE', 'WET_MODE', 'FB_MODE', 'RP_MODE']
RES_PARAMS = ['CUTOFF', 'RES', 'DECAY', 'PITCH', 'SLOPE', 'TONE', 'SCALE', 'WET', 'AGE', 'DECAY_MODE', 'WET_MODE']
MOD_PARAMS = ['W_RATE', 'W_AMOUNT', 'W_PHASE', 'W_OFFSET', 'W_SKEW', 'W_FOLD', 'W_CURVE', 'W_SPREAD', 'W_RATE_MODE', 'W_SHAPE',
              'R_RATE', 'R_AMOUNT', 'R_PHASE', 'R_OFFSET', 'R_LENGTH', 'R_VARIATION', 'R_SMOOTH', 'R_SPREAD', 'R_RATE_MODE',
              'F_DELAY', 'F_AMOUNT', 'F_RISE', 'F_FALL', 'F_INPUT', 'F_FILTER', 'F_HOLD', 'F_SPREAD',
              'A_ATTACK', 'A_DECAY', 'A_SUSTAIN', 'A_RELEASE', 'A_ATT_CURVE', 'A_DEC_CURVE', 'A_REL_CURVE', 'A_SPREAD', 'A_TRIG']

SCALES = ['CHROMATIC', 'MAJOR', 'MINOR', 'DORIAN', 'LYDIAN', 'SUPER LOCRIAN', 'MIXOLYDIAN', 'HEX AEOLIAN', 'HEX DORIAN', 'BLUES',
          'PENTATONIC', 'HIRAJOSHI', 'KUMOI', 'IWATO', 'WHOLE TONE', 'PELOG', 'TETRATONIC', 'FIFTHS', 'OCTAVES', 'FREE']
RATE_DIVS = ['4/1', '2/1', '1/1', '1/2', '1/4', '1/8', '1/16', '1/32', '1/64']
DELAY_DIVS = ['1/64', '1/32', '1/16', '1/8', '1/8D', '1/4', '1/4D', '1/2', '1/1', '2/1', '4/1']

# knob helpers (inverse of the mappings in src/dsp/Grains.hpp, at 120 BPM where seconds are given)
def rate_div(name): return RATE_DIVS.index(name) / 8
def rate_hz(hz): return math.log(hz / 0.25) / math.log(1000)
def size_beats(b): return math.log(b * 64) / math.log(512)
def size_sec(s, bpm=120): return size_beats(s * bpm / 60)
def delay_div(name): return DELAY_DIVS.index(name) / 10
def delay_sec(s): return math.sqrt(s / 3.95)
def stretch(v): return (v + 2) / 4
def cutoff(hz): return math.log(hz / 20) / math.log(1000)

GRAIN_DEFAULT = dict(PITCH=0, RATE=0.75, SIZE=4 / 9, CONTOUR=0, SCAN=0.625, SPRAY=0, PATTERN=0, WET=0.5,
                     FEEDBACK=0, RAND_PITCH=0, RAND_RATE=0, RAND_SIZE=0, RAND_AMP=0, RAND_REV=0, AGE=0,
                     SCALE=0, TEMPO=120, LOCK=0, RATE_MODE=0, SCAN_MODE=0, SPRAY_MODE=0, WET_MODE=0, FB_MODE=0, RP_MODE=0)
# modes: RATE_MODE 0 SYNC 1 FREE; SCAN_MODE 0 STRETCH 1 POSITION 2 DELAY SYNC 3 DELAY FREE;
# SPRAY_MODE 0 RANDOM 1 RANDOM MONO 2 WARP 3 WARP MONO; WET_MODE 0 MIX 1 SEND; FB_MODE 0 GRAIN 1 SOS; RP_MODE 0 DETUNE 1 RANDOM 2 RANDOM MONO
GRAIN_PRESETS = [
    ('01', 'Grain Delay Eighths', dict(SCAN_MODE=2, SCAN=delay_div('1/8D'), RATE=rate_div('1/16'), SIZE=size_beats(3 / 8), PITCH=12, FEEDBACK=0.35, WET=0.55), 0),
    ('02', 'Half Speed Cloud', dict(SCAN_MODE=0, SCAN=stretch(0.5), RATE=rate_div('1/32'), SIZE=size_beats(1), SPRAY=0.06, WET=1.0), 0),
    ('03', 'Chord To Arpeggio', dict(SCAN_MODE=2, SCAN=delay_div('1/4'), RATE=rate_div('1/16'), SIZE=size_beats(1 / 4), CONTOUR=-0.9, PATTERN=0.5, SCALE=SCALES.index('PENTATONIC'), WET=0.8), 0),
    ('04', 'Frozen Scan', dict(SCAN_MODE=1, SCAN=0.5, RATE=rate_div('1/32'), SIZE=size_beats(1 / 2), SPRAY=0.01, WET=1.0), 2),
    ('05', 'Backwards Glass', dict(SCAN_MODE=3, SCAN=delay_sec(0.6), RATE_MODE=1, RATE=rate_hz(7), SIZE=size_sec(0.45), RAND_REV=0.6, RP_MODE=1, RAND_PITCH=0.5, SCALE=SCALES.index('MAJOR'), SPRAY=0.1, WET=0.6), 0),
    ('06', 'Shimmer', dict(SCAN_MODE=3, SCAN=delay_sec(0.5), RATE_MODE=1, RATE=rate_hz(12), SIZE=size_sec(0.3), PITCH=12, SCALE=SCALES.index('OCTAVES'), FEEDBACK=0.7, SPRAY=0.05, WET=0.5), 0),
    ('07', 'Warped Tape', dict(SCAN_MODE=3, SCAN=delay_sec(1.2), RATE_MODE=1, RATE=rate_hz(10), SIZE=size_sec(0.6), SPRAY_MODE=2, SPRAY=0.15, RP_MODE=0, RAND_PITCH=0.08, WET=0.75), 0),
    ('08', 'Buzz Tone', dict(SCAN_MODE=0, SCAN=stretch(0.25), RATE_MODE=1, RATE=rate_hz(110), SIZE=size_sec(0.03), CONTOUR=-0.5, WET=1.0), 0),
    ('09', 'Scatter', dict(SCAN_MODE=2, SCAN=delay_div('1/4'), RATE=rate_div('1/16'), SIZE=size_beats(1 / 2), RAND_RATE=1.0, RAND_SIZE=0.8, RAND_AMP=0.8, RAND_REV=0.3, PITCH=-12, SPRAY=0.2, WET=0.7), 0),
    ('10', 'Stutter Sixteenths', dict(SCAN_MODE=2, SCAN=delay_div('1/16'), RATE=rate_div('1/16'), SIZE=size_beats(1 / 8), CONTOUR=-1.0, FB_MODE=1, FEEDBACK=0.5, WET=1.0), 0),
    ('11', 'Octave Ghost', dict(SCAN_MODE=2, SCAN=delay_div('1/2'), RATE=rate_div('1/8'), PITCH=-12, SIZE=size_beats(1 / 2), CONTOUR=0.8, WET=0.4), 0),
]
GRAIN_PRESETS.append(('12', 'Worn Sampler', dict(GRAIN_PRESETS[1][2], AGE=0.7), 0))

RES_DEFAULT = dict(CUTOFF=1.0, RES=0, DECAY=0, PITCH=0, SLOPE=-1, TONE=0, SCALE=0, WET=1.0, AGE=0, DECAY_MODE=0, WET_MODE=0)
RES_PRESETS = [
    ('01', 'Open Bank', dict(), 0),
    ('02', 'Low-pass Classic', dict(CUTOFF=cutoff(700), RES=0.6), 0),
    ('03', 'Pentatonic Ring', dict(SCALE=SCALES.index('PENTATONIC'), PITCH=9, DECAY=0.7, WET=0.6), 0),
    ('04', 'Fifths Drone', dict(SCALE=SCALES.index('FIFTHS'), PITCH=3, DECAY=0.85, WET=0.5), 0),
    ('05', 'Band-pass Shimmer', dict(SLOPE=0, CUTOFF=cutoff(900), RES=0.3, DECAY=0.55), 0),
    ('06', 'Choke Sweep', dict(DECAY_MODE=1, SCALE=SCALES.index('MINOR'), DECAY=0.8, CUTOFF=cutoff(1200)), 1),
    ('07', 'Bright Glass', dict(SCALE=SCALES.index('HIRAJOSHI'), TONE=0.6, DECAY=0.65, PITCH=12), 0),
]
RES_PRESETS.append(('08', 'Old Bank', dict(RES_PRESETS[2][2], AGE=0.7), 0))

MOD_DEFAULT = dict(W_RATE=0.5, W_AMOUNT=1, W_PHASE=0, W_OFFSET=0, W_SKEW=0, W_FOLD=0, W_CURVE=0, W_SPREAD=0, W_RATE_MODE=0, W_SHAPE=0,
                   R_RATE=6 / 9, R_AMOUNT=1, R_PHASE=0, R_OFFSET=0, R_LENGTH=8, R_VARIATION=0, R_SMOOTH=0, R_SPREAD=0, R_RATE_MODE=1,
                   F_DELAY=0, F_AMOUNT=1, F_RISE=math.sqrt(0.005), F_FALL=math.sqrt(0.15 / 4), F_INPUT=1, F_FILTER=0, F_HOLD=0, F_SPREAD=0,
                   A_ATTACK=None, A_DECAY=None, A_SUSTAIN=0.6, A_RELEASE=None, A_ATT_CURVE=0, A_DEC_CURVE=0, A_REL_CURVE=0, A_SPREAD=0, A_TRIG=0)
def env_knob(sec, top): return math.log(sec / 0.001) / math.log(top / 0.001)   # 1 ms .. top, exponential
MOD_DEFAULT.update(A_ATTACK=env_knob(0.005, 10), A_DECAY=env_knob(0.2, 10), A_RELEASE=env_knob(0.3, 20))
def free_hz(hz): return math.log(hz / 0.01) / math.log(10000)
MOD_PRESETS = [
    ('01', 'Slow Sine And Steps', dict(W_RATE=free_hz(0.1), R_RATE_MODE=1, R_RATE=5 / 9, R_SMOOTH=0.2), 0),
    ('02', 'Skewed Envelope Wave', dict(W_RATE_MODE=1, W_RATE=5 / 9, W_SKEW=0.9, W_AMOUNT=0.5, W_OFFSET=0.5), 0),
    ('03', 'Evolving Eight Steps', dict(R_LENGTH=8, R_VARIATION=0.2, R_SMOOTH=0.3, R_RATE_MODE=1, R_RATE=7 / 9, R_SPREAD=0.5), 0),
    ('04', 'Duck And Swell', dict(F_AMOUNT=-1, F_FALL=math.sqrt(0.25 / 4), A_ATTACK=env_knob(1.5, 10), A_DECAY=env_knob(2.0, 10), A_SUSTAIN=0.7, A_RELEASE=env_knob(4.0, 20)), 0),
]


def check_enums(cpp, names):
    p = os.path.join(ROOT, 'src', cpp)
    if not os.path.exists(p):
        print('WARNING: missing', cpp); return
    src = open(p).read()
    m = re.search(r'enum ParamIds\s*\{(.*?)NUM_PARAMS', src, re.S)
    if not m:
        print('WARNING: no ParamIds in', cpp); return
    got = [x.strip().replace('_PARAM', '') for x in m.group(1).replace('\n', ' ').split(',') if x.strip()]
    if got != names:
        raise SystemExit(f'{cpp}: param order differs from make_presets.py\n{got}\n{names}')


def write(model, folder, names, default, presets):
    d = os.path.join(ROOT, 'presets', folder); os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        if f.endswith('.vcvm'): os.remove(os.path.join(d, f))
    for num, name, vals, _ in presets:
        v = dict(default); v.update(vals)
        params = [{'value': round(float(v[n]), 6), 'id': i} for i, n in enumerate(names)]
        json.dump({'plugin': 'EssphorFable', 'model': model, 'version': VERSION, 'params': params},
                  open(os.path.join(d, f'{num}_{name}.vcvm'), 'w'), indent=1)


def cpp_grain(vals):
    v = dict(GRAIN_DEFAULT); v.update(vals)
    return (f'p.pitch = {v["PITCH"]}; p.sync = {"false" if v["RATE_MODE"] else "true"}; p.rateKnob = {v["RATE"]:.6f}; p.sizeKnob = {v["SIZE"]:.6f}; '
            f'p.contour = {v["CONTOUR"]}; p.scanMode = {v["SCAN_MODE"]}; p.scan = {v["SCAN"]:.6f}; p.sprayMode = {v["SPRAY_MODE"]}; p.spray = {v["SPRAY"]}; '
            f'p.pattern = {v["PATTERN"]}; p.wet = {v["WET"]}; p.send = {"true" if v["WET_MODE"] else "false"}; p.fbMode = {v["FB_MODE"]}; p.feedback = {v["FEEDBACK"]}; '
            f'p.rpMode = {v["RP_MODE"]}; p.randPitch = {v["RAND_PITCH"]}; p.randRate = {v["RAND_RATE"]}; p.randSize = {v["RAND_SIZE"]}; p.randAmp = {v["RAND_AMP"]}; '
            f'p.randRev = {v["RAND_REV"]}; p.age = {v["AGE"]}; p.scale = {v["SCALE"]};')


def cpp_res(vals):
    v = dict(RES_DEFAULT); v.update(vals)
    return (f'p.cutoff = {20 * 1000 ** v["CUTOFF"]:.4f}; p.res = {v["RES"]}; p.decay = {v["DECAY"]}; p.choke = {"true" if v["DECAY_MODE"] else "false"}; '
            f'p.pitch = {v["PITCH"]}; p.slope = {v["SLOPE"]}; p.tone = {v["TONE"]}; p.scale = {v["SCALE"]}; p.wet = {v["WET"]}; '
            f'p.send = {"true" if v["WET_MODE"] else "false"}; p.age = {v["AGE"]};')


def slug(name): return name.lower().replace(' ', '-')


if __name__ == '__main__':
    check_enums('Essphor.cpp', GRAIN_PARAMS)
    check_enums('EssphorRes.cpp', RES_PARAMS)
    check_enums('EssphorMod.cpp', MOD_PARAMS)
    write('Essphor', 'Essphor', GRAIN_PARAMS, GRAIN_DEFAULT, GRAIN_PRESETS)
    write('EssphorRes', 'EssphorRes', RES_PARAMS, RES_DEFAULT, RES_PRESETS)
    write('EssphorMod', 'EssphorMod', MOD_PARAMS, MOD_DEFAULT, MOD_PRESETS)
    h = ['// Generated by tools/make_presets.py. Do not edit.', '#pragma once',
         'struct GrainPreset { const char* file; double demoLockAt; void (*apply)(essphor::GrainParams&); };',
         'struct ResPreset { const char* file; int demoSweep; void (*apply)(essphor::ResParams&); };']
    h.append('static const GrainPreset GRAIN_PRESETS[] = {')
    for num, name, vals, lock in GRAIN_PRESETS:
        h.append(f'\t{{"{num}-{slug(name)}", {lock}, [](essphor::GrainParams& p) {{ {cpp_grain(vals)} }}}},')
    h.append('};')
    h.append(f'static const int NUM_GRAIN_PRESETS = {len(GRAIN_PRESETS)};')
    h.append('static const ResPreset RES_PRESETS[] = {')
    for num, name, vals, sweep in RES_PRESETS:
        h.append(f'\t{{"{num}-{slug(name)}", {sweep}, [](essphor::ResParams& p) {{ {cpp_res(vals)} }}}},')
    h.append('};')
    h.append(f'static const int NUM_RES_PRESETS = {len(RES_PRESETS)};')
    open(os.path.join(ROOT, 'test', 'presets.h'), 'w').write('\n'.join(h) + '\n')
    print('ok:', len(GRAIN_PRESETS), len(RES_PRESETS), len(MOD_PRESETS))
