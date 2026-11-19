"""Supplement: roughly equal render time, using saved v0.4 independent reference images."""
import struct,subprocess,statistics,math,re,json
from pathlib import Path
root=Path('build-release')
def read(path):
    with path.open('rb') as f:
        f.readline();w,h=map(int,f.readline().split());f.readline()
        return struct.unpack('<'+str(w*h*3)+'f',f.read())
a=read(root/'v04-final-reference.pfm');b=read(root/'v04-final-reference-check.pfm')
reference=[(x+y)*.5 for x,y in zip(a,b)]
errors=[];times=[];logs=[]
for seed in range(1,9):
    command=['./build-release/path_tracer','--scene','sampling','--environment','studio-hdr.pfm','--mode','bsdf',
             '--width','48','--height','30','--spp','512','--depth','12','--seed',str(seed),'--threads','4',
             '--output',str(root/f'v04-final-bsdf-budget512-{seed}.ppm'),'--linear-output',str(root/f'v04-final-bsdf-budget512-{seed}.pfm')]
    result=subprocess.run(command,capture_output=True,text=True,check=True)
    print(result.stdout,flush=True);logs.append({'command':command,'result':result.stdout})
    values=read(root/f'v04-final-bsdf-budget512-{seed}.pfm')
    errors.append(sum((v-r)**2 for v,r in zip(values,reference))/len(values))
    times.append(float(re.search(r'render_seconds=([\d.]+)',result.stdout).group(1)))
differences=[]
for seed in range(1,9):
    n=read(root/f'v04-final-nee-{seed}.pfm');m=read(root/f'v04-final-mis-{seed}.pfm')
    differences.append(sum((nv-r)**2-(mv-r)**2 for nv,mv,r in zip(n,m,reference))/len(reference))
report={'bsdf_512_rmse':math.sqrt(statistics.mean(errors)),'bsdf_512_seconds':statistics.mean(times),
        'nee_minus_mis_mse':statistics.mean(differences),'paired_mse_standard_error':statistics.stdev(differences)/math.sqrt(8),'runs':logs}
(root/'v04-final-equal-time-512.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k!='runs'})
