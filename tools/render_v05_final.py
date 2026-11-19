"""Final v0.5 sequential evidence, sharing candidate logger."""
import sys
sys.dont_write_bytecode=True
from render_v05 import run
from pathlib import Path
run('color','forest-v05',640,360,64,115,1.1)
run('gray','forest-v05-gray',640,360,32,115,1.1)
run('detail','forest-v05-detail',480,320,64,115,1.1)
run('baseline-v04-light','forest-v03',320,180,16,30,.7)
run('benchmark-v03','forest-v03',160,90,16,115,1.1)
run('benchmark-v05','forest-v05',160,90,16,115,1.1)
run('threads-one','forest-v05',80,45,16,115,1.1,1)
run('threads-four','forest-v05',80,45,16,115,1.1,4)
assert Path('build-release/v05-threads-one.ppm').read_bytes()==Path('build-release/v05-threads-four.ppm').read_bytes()
print('PASS new scene 1/4-thread PPM equality')
