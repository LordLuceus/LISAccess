"""Snapshot only reflected object metadata in the authorized game process."""
import json,struct,sys,re
from pathlib import Path
from live_inspect import Memory,u32
d=json.loads(Path('artifacts/live-discovery.json').read_text());m=Memory(d['pid'])
pages={}
def read(addr,n):
    page=addr&~4095
    if addr+n>page+4096:return m.read(addr,n)
    if page not in pages:pages[page]=m.read(page,4096)
    return pages[page][addr-page:addr-page+n]
def word(addr):
    b=read(addr,4);return u32(b,0) if len(b)==4 else 0
names=d['names'];nc={}
def name(i):
    if not 0<=i<names['count']:return ''
    if i not in nc:
        ptr=word(names['data']+i*4);b=read(ptr+names['text_offset'],192)
        nc[i]=b.split(b'\0')[0].decode('latin-1')
    return nc[i]
def objname(ptr):
    b=read(ptr+44,8)
    if len(b)!=8:return ''
    i,n=struct.unpack('<II',b)
    return name(i)+(f'_{n-1}' if n else '')
a=d['objects'][0];ps=m.read(a['data'],a['count']*4)
objects=[]
for i in range(a['count']):
    ptr=u32(ps,i*4);b=read(ptr,0xa0)
    if len(b)!=0xa0:continue
    objects.append(dict(address=ptr,index=i,name=objname(ptr),class_name=objname(u32(b,52)),outer=objname(u32(b,40)),words=[u32(b,o) for o in range(0,0xa0,4)]))
out=Path('artifacts/live-objects.json');out.write_text(json.dumps(objects,indent=2))
print('Registries:',{k:hex(v['address']-d['base']) for k,v in [('GNames',names),('GObjects',a)]})
print('Snapshot objects:',len(objects))
for o in objects:
    if o['class_name'] in ['OutGameSensenMenu','PauseMenu','SensenMenu','GFxMoviePlayerManager','GFxUIWidgetPlayer'] or (o['outer'] in ['OutGameSensenMenu','SensenMenuBase','GFxMoviePlayer','GFxObject'] and o['name'] in ['bOnTitlePage','MainMovieClip','pMovie','Value','OpeningState'] and 'Property' in o['class_name']):
        print(hex(o['address']),o['class_name'],o['outer'],o['name'],'words60+',[hex(v) for v in o['words'][15:36]])
m.close()
