"""Measurements used to compare a rendered sound with the reference: level in 8 octave bands over 7 stretches of time
after the first onset (dB, not normalised), and the overall level."""
import numpy as np
from scipy.signal import butter, sosfilt
BANDS = [(30, 90), (90, 180), (180, 360), (360, 720), (720, 1500), (1500, 3000), (3000, 6000), (6000, 12000)]
FRAMES = [(0, 10), (10, 30), (30, 60), (60, 120), (120, 250), (250, 500), (500, 1000)]
def features(x, sr):
    x = np.asarray(x, dtype=np.float64); st = int(np.argmax(np.abs(x) > 0.02 * np.abs(x).max())); y0 = x[st:]
    tab = []
    for lo, hi in BANDS:
        y = sosfilt(butter(4, [lo, hi], 'bandpass', fs=sr, output='sos'), y0)
        tab.append([round(float(10 * np.log10(np.mean(y[int(a * sr / 1000):max(int(a * sr / 1000) + 1, min(len(y), int(b * sr / 1000)))] ** 2) + 1e-12)), 2) for a, b in FRAMES])
    return dict(table=tab, rms=round(float(10 * np.log10(np.mean(x ** 2) + 1e-12)), 2), onset=st)
def compare(fa, fb, floor=-75.0):
    """mean and worst difference in dB over the cells that are above the floor in the reference"""
    a = np.array(fa['table']); b = np.array(fb['table']); m = a > floor
    d = np.abs(a - b)[m]; return float(d.mean()), float(d.max()), abs(fa['rms'] - fb['rms'])
