"""Read-only memory survey. OpenProcess requests QUERY_INFORMATION|VM_READ only."""
import ctypes as c
from ctypes import wintypes as w
import json,struct,sys
from pathlib import Path
K=c.WinDLL('kernel32',use_last_error=True)
K.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];K.OpenProcess.restype=w.HANDLE
K.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
K.ReadProcessMemory.restype=w.BOOL;K.CloseHandle.argtypes=[w.HANDLE]
class Memory:
    def __init__(self,pid):
        self.handle=K.OpenProcess(0x410,False,pid)
        if not self.handle:raise c.WinError(c.get_last_error())
    def read(self,address,size):
        if not 0x10000<=address<0x100000000 or not 0<=size<=32*1024*1024:return b''
        b=c.create_string_buffer(size);n=c.c_size_t()
        if not K.ReadProcessMemory(self.handle,address,b,size,c.byref(n)):return b''
        return b.raw[:n.value]
    def u32(self,address):
        b=self.read(address,4);return struct.unpack('<I',b)[0] if len(b)==4 else 0
    def close(self):K.CloseHandle(self.handle)
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def discover(mem,base,survey):
    arrays=[]
    for section in survey['sections']:
        if section['name']!='.data':continue
        start=base+section['rva'];data=mem.read(start,section['virtual_size'])
        for off in range(0,len(data)-12,4):
            ptr,n,cap=struct.unpack_from('<III',data,off)
            if 0x10000<=ptr<0xf0000000 and ptr%4==0 and 512<=n<=1000000 and n<=cap<=n*2+4096:
                arrays.append(dict(address=start+off,data=ptr,count=n,capacity=cap))
    matches=[]
    for a in arrays:
        b=mem.read(mem.u32(a['data']),128)
        for off in [8,12,16,20,24]:
            if b[off:off+5]==b'None\0':matches.append({**a,'text_offset':off,'wide':False})
            if b[off:off+10]==b'N\0o\0n\0e\0\0\0':matches.append({**a,'text_offset':off,'wide':True})
    if len(matches)!=1:return dict(array_candidates=len(arrays),name_candidates=matches)
    names=matches[0];cache={}
    def name(i):
        if i in cache:return cache[i]
        if not 0<=i<names['count']:return ''
        ptr=mem.u32(names['data']+i*4);b=mem.read(ptr+names['text_offset'],192)
        text=(b.decode('utf-16-le',errors='replace') if names['wide'] else b.decode('latin-1')).split('\0')[0]
        cache[i]=text;return text
    objects=[]
    for a in arrays:
        ps=mem.read(a['data'],min(a['count'],40)*4)
        samples=[mem.read(u32(ps,i),128) for i in range(0,len(ps),4)]
        samples=[b for b in samples if len(b)==128]
        if len(samples)<20:continue
        for off in range(4,80,4):
            ns=[name(u32(b,off)) for b in samples if u32(b,off)<names['count'] and u32(b,off+4)<100000]
            valid=[s for s in ns if s not in ('','None') and all(x.isalnum() or x in '_-' for x in s)]
            if len(valid)>=len(samples)*.8 and len(set(valid))>=12:objects.append({**a,'name_offset':off,'sample_names':valid[:20]})
    return dict(array_candidates=len(arrays),names=names,first_names=[name(i) for i in range(24)],objects=objects)
if __name__=='__main__':
    pid,base=int(sys.argv[1]),int(sys.argv[2],0);out=Path(sys.argv[3]);m=Memory(pid)
    try:
        r=discover(m,base,json.loads(Path('artifacts/pe-survey.json').read_text()));r.update(pid=pid,base=base)
        out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
    finally:m.close()
