"""Derive a quieter original timber map from the retained original wood; no external input."""
from pathlib import Path
source=Path('assets/wood.ppm').read_text().split()
values=list(map(int,source[4:]));w,h=map(int,source[1:3])
out=Path('assets/wood-soft-v05.ppm')
if out.exists(): raise SystemExit('Refusing overwrite')
with out.open('w') as f:
    f.write(f'P3\n{w} {h}\n255\n')
    for i in range(0,len(values),3):
        # Compress grain contrast around channel mean; less saturated weathered cedar.
        channels=[int(base+.25*(v-mean)) for v,base,mean in zip(values[i:i+3],[108,83,59],[116,72,39])]
        f.write(' '.join(map(str,channels))+'\n')
