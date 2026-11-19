"""Sequential v0.3 evidence runs. Run from repo root after Release build."""
from pathlib import Path
import subprocess, json
runs=[]
cases=[('bench-one','forest-v03',320,180,32,1),('bench-four','forest-v03',320,180,32,4),
       ('color','forest-v03',640,360,64,4),('gray','forest-v03-gray',640,360,32,4),
       ('detail','forest-v03-detail',480,320,64,4),('baseline','forest',480,270,32,4)]
for tag,scene,width,height,spp,threads in cases:
    command=['./build-release/path_tracer','--scene',scene,'--width',str(width),'--height',str(height),
             '--spp',str(spp),'--depth','10','--seed','19','--threads',str(threads),
             '--resource-root','assets','--output',f'build-release/v03-final-{tag}.ppm']
    result=subprocess.run(command,capture_output=True,text=True,check=True)
    print(result.stdout,flush=True)
    runs.append({'command':command,'result':result.stdout})
    Path('build-release/v03-final-renders.json').write_text(json.dumps(runs,indent=2))
assert Path('build-release/v03-final-bench-one.ppm').read_bytes()==Path('build-release/v03-final-bench-four.ppm').read_bytes()
print('PASS 1/4 thread image byte equality')
