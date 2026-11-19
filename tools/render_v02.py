"""Run v0.2 evidence renders from repository root; never overwrite existing outputs."""
from pathlib import Path
import subprocess, json
cases=[
 ('dof-off','dof',[]),
 ('dof-on','dof',['--lens-radius','.22','--focus-distance','6']),
 ('motion-off','motion',['--shutter-open','.5','--shutter-close','.5']),
 ('motion-on','motion',['--shutter-open','0','--shutter-close','1']),
 *[(name,name,[]) for name in ['alpha-zero','alpha-half','alpha-one','alpha-double','alpha-pattern']],
]
results=[]
for tag,scene,extra in cases:
    command=['./build-release/path_tracer','--scene',scene,'--width','320','--height','200','--spp','128','--depth','8','--seed','27',*extra,'--output',f'build-release/v02-{tag}.ppm']
    result=subprocess.run(command,text=True,capture_output=True,check=True)
    print(result.stdout,flush=True)
    results.append({'command':command,'result':result.stdout})
Path('build-release/v02-renders.json').write_text(json.dumps(results,indent=2))
