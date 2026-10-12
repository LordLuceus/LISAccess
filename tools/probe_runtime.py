"""Ask the prototype for one read-only menu value; no arbitrary native calls."""
import argparse,os,time,sys
from pathlib import Path
p=argparse.ArgumentParser()
sys.stdout.reconfigure(encoding='utf-8')
p.add_argument('kind',choices=['number','string','text'])
p.add_argument('path')
a=p.parse_args()
root=Path(os.environ['LOCALAPPDATA'])/'LISAccess'
assert root.is_dir(),'Prototype has not started'
assert a.path.startswith('_root.') and len(a.path)<256
response=root/'response.txt';request=root/'request.txt'
if request.exists():raise RuntimeError('A diagnostic request is already pending')
if response.exists():response.unlink()
temp=root/'request.tmp';temp.write_text(a.kind+' '+a.path,encoding='utf8');temp.replace(request)
deadline=time.monotonic()+10
while not response.exists():
    if time.monotonic()>deadline:raise TimeoutError('No menu response; inspect prototype.log')
    time.sleep(.1)
print(response.read_text(encoding='utf8'))
