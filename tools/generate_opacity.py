"""Original linear leaf silhouette: elliptical leaves on a diagonal stem with vein holes."""
from pathlib import Path
path=Path('assets/leaf-opacity.ppm')
if path.exists(): raise SystemExit('Refusing to overwrite opacity asset')
with path.open('w') as f:
    f.write('P3\n128 128\n255\n')
    for y in range(128):
        for x in range(128):
            u=(x+.5)/128; v=1-(y+.5)/128
            covered=abs(u-(.3+.4*v))<.015 and .08<v<.92
            for cx,cy in [(.3,.25),(.63,.4),(.42,.58),(.73,.72)]:
                dx=u-cx; dy=v-cy
                a=(dx+dy)*.707; b=(dy-dx)*.707
                covered |= (a/.22)**2+(b/.095)**2<1 and abs(b)>.006
            value=255 if covered else 0
            f.write(f'{value} {value} {value}\n')
