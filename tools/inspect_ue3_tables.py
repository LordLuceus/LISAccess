"""Read-only LiS UE3 893/21 package metadata, not a script decompiler.

Table layout cross-checked against UEViewer's UnPackage3.cpp. LZO decoding
uses a caller-supplied, separately licensed dissect.util lzo.py module.
Only chunks covering metadata tables are expanded; game files stay untouched.
"""
import argparse
import hashlib
import importlib.util
import json
import struct
from collections import Counter
from pathlib import Path

class Reader:
    def __init__(self, data, pos=0): self.data, self.pos = data, pos
    def number(self):
        value = struct.unpack_from('<i', self.data, self.pos)[0]
        self.pos += 4
        return value
    def skip(self, count):
        assert 0 <= self.pos + count <= len(self.data)
        self.pos += count
    def text(self):
        count = self.number()
        assert 0 < abs(count) < 1000000
        size = count if count > 0 else -count * 2
        # Positive lengths are legacy 8-bit names, not UTF-8. Preserve bytes
        # losslessly (including French identifiers); negative lengths are UTF-16.
        value = self.data[self.pos:self.pos + size].decode('latin-1' if count > 0 else 'utf-16-le')
        self.skip(size)
        assert value.endswith('\0')
        return value[:-1]
    def name(self, names):
        index, number = self.number(), self.number()
        assert 0 <= index < len(names) and 0 <= number < 1000000
        return names[index] + (f'_{number - 1}' if number else '')

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--lzo', type=Path, required=True)
args = parser.parse_args()
spec = importlib.util.spec_from_file_location('research_lzo', args.lzo)
lzo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lzo)
data = args.package.read_bytes()
assert struct.unpack_from('<IHH', data) == (0x9e2a83c1, 893, 21)
header_size = struct.unpack_from('<i', data, 8)[0]
r = Reader(data, 12)
folder = r.text()
flags = r.number()
name_count, name_offset, export_count, export_offset, import_count, import_offset = [r.number() for _ in range(6)]
depends_offset = r.number()
r.skip(16 + 16)  # four newer summary fields, then package GUID
generations = r.number()
assert 0 <= generations <= 100
r.skip(generations * 12)
engine, cooker, compression, chunk_count = [r.number() for _ in range(4)]
assert compression == 2 and 0 < chunk_count < 10000
chunks = [[r.number() for _ in range(4)] for _ in range(chunk_count)]
assert 0 < header_size < 32 * 1024 * 1024
expanded = bytearray(header_size)
expanded[:chunks[0][0]] = data[:chunks[0][0]]
for virtual, length, physical, compressed_length in chunks:
    if virtual >= header_size: continue
    tag, block_size, total_compressed, total_uncompressed = struct.unpack_from('<4I', data, physical)
    assert tag == 0x9e2a83c1 and total_uncompressed == length and 0 < block_size <= 1024 * 1024
    count = (length + block_size - 1) // block_size
    blocks = [struct.unpack_from('<2I', data, physical + 16 + i * 8) for i in range(count)]
    assert sum(c for c, u in blocks) == total_compressed
    assert sum(u for c, u in blocks) == length
    assert 16 + count * 8 + total_compressed == compressed_length
    source, destination = physical + 16 + count * 8, virtual
    for compressed, uncompressed in blocks:
        if destination >= header_size: break
        block = lzo.decompress(data[source:source + compressed], header=False, buflen=uncompressed)
        assert len(block) == uncompressed
        end = min(destination + uncompressed, header_size)
        expanded[destination:end] = block[:end - destination]
        source += compressed
        destination += uncompressed

r = Reader(expanded, name_offset)
names = []
for _ in range(name_count):
    names.append(r.text())
    r.skip(8)
assert r.pos == import_offset, (r.pos, import_offset)
imports = []
for _ in range(import_count):
    imports.append(dict(class_package=r.name(names), class_name=r.name(names), outer=r.number(), name=r.name(names)))
assert r.pos == export_offset
exports = []
for _ in range(export_count):
    class_index, super_index, outer = r.number(), r.number(), r.number()
    name = r.name(names)
    archetype = r.number()
    r.skip(8)
    serial_size, serial_offset, export_flags, net_count = [r.number() for _ in range(4)]
    assert 0 <= net_count < 10000 and serial_size >= 0 and serial_offset >= 0
    r.skip(net_count * 4 + 16 + 4)
    exports.append(dict(name=name, class_index=class_index, super_index=super_index, outer=outer,
                        serial_size=serial_size, serial_offset=serial_offset))
assert r.pos == depends_offset, (r.pos, depends_offset)

def reference(index):
    assert -len(imports) <= index <= len(exports)
    return exports[index - 1] if index > 0 else imports[-index - 1] if index < 0 else None

def qualified(entry, visited=None):
    visited = set() if visited is None else visited
    outer = entry['outer']
    assert outer not in visited
    parent = reference(outer)
    return (qualified(parent, visited | {outer}) + '.' if parent else '') + entry['name']

for e in exports:
    cls = reference(e['class_index'])
    e['class'] = cls['name'] if cls else 'Class'
    e['qualified_name'] = qualified(e)
result = dict(path=str(args.package), sha256=hashlib.sha256(data).hexdigest(),
              version=893, licensee_version=21, engine_version=engine, cooker_version=cooker,
              names=names, imports=imports, exports=exports)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2), encoding='utf8')
print(json.dumps(dict(package=args.package.name, names=len(names), imports=len(imports), exports=len(exports),
                      class_definitions=sum(e['class'] == 'Class' for e in exports),
                      function_definitions=sum(e['class'] == 'Function' for e in exports),
                      common_export_types=Counter(e['class'] for e in exports).most_common(8),
                      tables_validated=True), indent=2))
