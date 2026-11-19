"""Original analytic linear HDR sky with a bright finite sun patch. No source photography."""
import math,struct
from pathlib import Path
p=Path('assets/studio-hdr.pfm')
if p.exists(): raise SystemExit('Refusing to overwrite environment')
w,h=128,64
with p.open('wb') as f:
    f.write(f'PF\n{w} {h}\n-1.0\n'.encode())
    for y in reversed(range(h)):
        theta=math.pi*(y+.5)/h
        for x in range(w):
            phi=2*math.pi*(x+.5)/w
            sky=max(0,math.cos(theta))
            base=[.08+.14*sky,.11+.22*sky,.16+.35*sky]
            # High luminance lobe spans multiple texels, all values are linear radiance.
            distance=math.acos(max(-1,min(1,math.cos(theta)*math.cos(.65)+math.sin(theta)*math.sin(.65)*math.cos(phi-2.3))))
            sun=70*math.exp(-(distance/.075)**2)
            values=[base[i]+sun*c for i,c in enumerate([1,.8,.5])]
            f.write(struct.pack('<3f',*values))
