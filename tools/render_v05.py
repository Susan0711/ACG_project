"""Sequential v0.5 scene-only evidence; rejects existing output paths through CLI."""
import subprocess,json
from pathlib import Path
logs=json.loads(Path("build-release/v05-renders.json").read_text()) if Path("build-release/v05-renders.json").exists() else []
def run(tag,scene,w,h,spp,rotation,intensity,threads=4):
    command=['./build-release/path_tracer','--scene',scene,'--resource-root','assets','--environment','studio-hdr.pfm',
             '--env-rotation',str(rotation),'--env-intensity',str(intensity),'--mode','mis','--width',str(w),
             '--height',str(h),'--spp',str(spp),'--depth','12','--seed','19','--threads',str(threads),
             '--output',f'build-release/v05-{tag}.ppm']
    r=subprocess.run(command,capture_output=True,text=True,check=True)
    print(r.stdout,flush=True);logs.append({'command':command,'result':r.stdout})
    Path('build-release/v05-renders.json').write_text(json.dumps(logs,indent=2))
if __name__=='__main__':
    run('candidate-a','forest-v05',320,180,8,30,.7)
    run('candidate-b','forest-v05',320,180,8,115,1.1)
