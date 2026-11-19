"""Sequential linear HDR/MIS evidence, no display-space error estimates."""
from pathlib import Path
import subprocess,json,struct,statistics,math,re
root=Path('build-release')
logs=[]
def run(tag,mode,spp,seed,width=48,height=30,scene='sampling',extra=None):
    output=root/('v04-final-'+tag)
    command=['./build-release/path_tracer','--scene',scene,'--environment','studio-hdr.pfm',
             '--mode',mode,'--width',str(width),'--height',str(height),'--spp',str(spp),
             '--depth','12','--seed',str(seed),'--threads','4','--output',str(output)+'.ppm',
             '--linear-output',str(output)+'.pfm']+(extra or [])
    result=subprocess.run(command,capture_output=True,text=True,check=True)
    print(tag,result.stdout.strip(),flush=True)
    logs.append({'command':command,'result':result.stdout})
    (root/'v04-final-renders.json').write_text(json.dumps(logs,indent=2))
    with Path(str(output)+'.pfm').open('rb') as f:
        assert f.readline().strip()==b'PF'
        w,h=map(int,f.readline().split());assert float(f.readline())<0
        values=struct.unpack('<'+str(w*h*3)+'f',f.read())
    seconds=float(re.search(r'render_seconds=([\d.]+)',result.stdout).group(1))
    return values,seconds
ref,_=run('reference','mis',16384,900)
ref2,_=run('reference-check','mis',16384,901)
reference=[(a+b)*.5 for a,b in zip(ref,ref2)]
report={'reference_half_difference_rmse':math.sqrt(sum((a-b)**2 for a,b in zip(ref,ref2))/len(ref))/2,'modes':{}}
for mode in ['bsdf','nee','mis']:
    means=[];errors=[];times=[]
    for seed in range(1,9):
        values,seconds=run(mode+'-'+str(seed),mode,256,seed)
        means.append(sum(.2126*values[i]+.7152*values[i+1]+.0722*values[i+2] for i in range(0,len(values),3))/(len(values)/3))
        errors.append(sum((v-r)**2 for v,r in zip(values,reference))/len(values))
        times.append(seconds)
    report['modes'][mode]={'mean_luminance':statistics.mean(means),'mean_standard_error':statistics.stdev(means)/math.sqrt(len(means)),
                          'rgb_rmse':math.sqrt(statistics.mean(errors)),'render_seconds':statistics.mean(times),'seed_means':means}
report['reference_mean']=sum(.2126*reference[i]+.7152*reference[i+1]+.0722*reference[i+2] for i in range(0,len(reference),3))/(len(reference)/3)
(root/'v04-final-statistics.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2),flush=True)
run('rotation-0','mis',128,19,320,200,extra=['--env-rotation','0'])
run('rotation-90','mis',128,19,320,200,extra=['--env-rotation','90'])
run('intensity-2','mis',128,19,320,200,extra=['--env-rotation','0','--env-intensity','2'])
run('forest-hdr','mis',64,19,640,360,'forest-v03',['--env-rotation','30','--env-intensity','.7'])
