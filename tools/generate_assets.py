"""Original deterministic wood: warped growth lines, plank seams, elliptical knots and pores."""
from pathlib import Path
import math, random
rng=random.Random(19)
out=Path('assets/wood.ppm')
if out.exists(): raise SystemExit('Refusing to overwrite wood.ppm')
with out.open('w') as f:
 f.write('P3\n256 256\n255\n')
 for y in range(256):
  for x in range(256):
   u=x/256; v=y/256
   knot=math.exp(-((u-.36)**2/.003+(v-.63)**2/.025))
   grain=math.sin(180*u+5*math.sin(v*12)+18*knot)
   fine=math.sin(630*u+9*math.sin(v*31))
   seam=0.45 if x%64<2 else 1
   value=seam*(.65+.13*grain+.04*fine-.23*knot+rng.uniform(-.035,.035))
   f.write(' '.join(str(int(max(0,min(255,c*value)))) for c in (180,112,61))+'\n')
rock=Path('assets/rock.obj')
if rock.exists(): raise SystemExit('Refusing to overwrite rock.obj')
rock.write_text('''# Original octahedral rock, flat shaded
v -1 0 0
v 0 0 1
v 1 0 0
v 0 0 -1
v 0 1 0
v 0 -0.3 0
vt 0 0
vt 1 0
vt 0.5 1
vn 0 1 0
f 1/1/1 2/2/1 5/3/1
f 2/1 3/2 5/3
f 3 4 5
f -3 -6 -2
f 2 1 6
f 3 2 6
f 4 3 6
f 1 4 6
''')
