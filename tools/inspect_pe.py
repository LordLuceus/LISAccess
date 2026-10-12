"""Read-only PE/import/string survey for the installed original game."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
data = args.binary.read_bytes()
u16 = lambda o: struct.unpack_from('<H', data, o)[0]
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
assert data[:2] == b'MZ'
pe = u32(0x3c)
assert data[pe:pe + 4] == b'PE\0\0'
machine, nsections = u16(pe + 4), u16(pe + 6)
opt = pe + 24
magic = u16(opt)
assert magic in (0x10b, 0x20b)
base = u32(opt + 28) if magic == 0x10b else struct.unpack_from('<Q', data, opt + 24)[0]
directories = opt + (96 if magic == 0x10b else 112)
sections = []
for i in range(nsections):
    off = opt + u16(pe + 20) + i * 40
    sections.append(dict(name=data[off:off + 8].rstrip(b'\0').decode('ascii'),
                         virtual_size=u32(off + 8), rva=u32(off + 12),
                         raw_size=u32(off + 16), raw_offset=u32(off + 20)))

def offset(rva):
    for s in sections:
        if s['rva'] <= rva < s['rva'] + max(s['virtual_size'], s['raw_size']):
            return s['raw_offset'] + rva - s['rva']
    raise ValueError(f'Unmapped RVA {rva:x}')

def address(file_offset):
    for s in sections:
        if s['raw_offset'] <= file_offset < s['raw_offset'] + s['raw_size']:
            return hex(base + s['rva'] + file_offset - s['raw_offset'])
    return None

def string(off):
    return data[off:data.index(b'\0', off)].decode('ascii', errors='replace')

imports = []
import_rva = u32(directories + 8)
if import_rva:
    pos = offset(import_rva)
    while any(data[pos:pos + 20]):
        thunk, _, _, name, first = struct.unpack_from('<5I', data, pos)
        entry = dict(dll=string(offset(name)), functions=[])
        ptr = offset(thunk or first)
        width = 4 if magic == 0x10b else 8
        while True:
            value = struct.unpack_from('<I' if width == 4 else '<Q', data, ptr)[0]
            if not value: break
            entry['functions'].append(f'ordinal:{value & 0xffff}' if value >> (width * 8 - 1) else string(offset(value) + 2))
            ptr += width
        imports.append(entry)
        pos += 20

pattern = re.compile(r'ProcessEvent|GObjects|GNames|GFx|Scaleform|UnrealScript|Dialogue|DialogChoice|Interact|Rewind|Subtitle|Navigation|NavMesh|PlayerController|FName|UObject|Unreal Engine', re.I)
strings = []
for encoding, regex in [('ascii', rb'[\x20-\x7e]{5,}'), ('utf-16-le', rb'(?:[\x20-\x7e]\x00){5,}')]:
    for match in re.finditer(regex, data):
        value = match.group().decode(encoding)
        if pattern.search(value):
            strings.append(dict(offset=hex(match.start()), address=address(match.start()), encoding=encoding, value=value))
result = dict(path=str(args.binary), size=len(data), sha256=hashlib.sha256(data).hexdigest(),
              machine=hex(machine), image_base=hex(base), managed=bool(u32(directories + 14 * 8)),
              sections=sections, imports=imports, matching_strings=strings)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2), encoding='utf8')
print(json.dumps({k: v for k, v in result.items() if k not in ('imports', 'matching_strings')}, indent=2))
print('Imported DLLs:', ', '.join(i['dll'] for i in imports))
print('Relevant byte strings:', len(strings), '(not runtime observations)')
