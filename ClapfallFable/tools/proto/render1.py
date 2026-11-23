import numpy as np, soundfile as sf, os
from clapfall_proto import *
tabs = build_tables()
os.makedirs('out', exist_ok=True)
# one of every variant of the four hands, for measuring
for name in ['CLAP','PALMS','CUPPED','FINGERS','SNAP','STICKS','808']:
    x = np.concatenate([np.concatenate([v, np.zeros(SR//5)]) for v in tabs[name][:8]])
    sf.write(f'out/tab_{name}.wav', x, SR)
    os.makedirs(f'out/single/{name}', exist_ok=True)
    for i, v in enumerate(tabs[name][:6]): sf.write(f'out/single/{name}/{i}.wav', np.concatenate([np.zeros(64), v, np.zeros(2000)]), SR)
