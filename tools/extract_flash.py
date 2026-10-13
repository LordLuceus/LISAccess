"""Extract one identified SWF/GFx export from the supported LiS UE3 package.

Read-only game access. Metadata must come from inspect_ue3_tables.py for these
exact package bytes. Output is proprietary research data, not a release asset.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import struct
import zlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('metadata', type=Path)
p.add_argument('movie', help='Qualified SwfMovie export name')
p.add_argument('output', type=Path)
p.add_argument('--lzo', type=Path, required=True)
a = p.parse_args()
meta = json.loads(a.metadata.read_text(encoding='utf-8'))
data = Path(meta['path']).read_bytes()
if hashlib.sha256(data).hexdigest().lower() != meta['sha256'].lower():
    raise ValueError('Metadata does not match package bytes')
if struct.unpack_from('<IHH', data) != (0x9e2a83c1, 893, 21):
    raise ValueError('Unsupported package format')
exports = [e for e in meta['exports'] if e['class'] == 'SwfMovie' and e['qualified_name'] == a.movie]
if len(exports) != 1:
    raise ValueError('Movie export is absent or ambiguous')
e = exports[0]
start, length = e['serial_offset'], e['serial_size']
if not (0 <= start and 8 <= length <= 32 * 1024 * 1024):
    raise ValueError('Unreasonable export range')
pos = 12
n = struct.unpack_from('<i', data, pos)[0]
pos += 4 + (n if n > 0 else -n * 2) + 4 + 6 * 4 + 4 + 32
n = struct.unpack_from('<i', data, pos)[0]
if not 0 <= n <= 100:
    raise ValueError('Invalid generation table')
pos += 4 + 12 * n
_, _, compression, count = struct.unpack_from('<4i', data, pos)
pos += 16
if compression != 2 or not 0 < count < 10000:
    raise ValueError('Unsupported compression/chunk table')
spec = importlib.util.spec_from_file_location('research_lzo', a.lzo)
lzo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lzo)
out, covered = bytearray(length), bytearray(length)
for i in range(count):
    virtual, size, physical, _ = struct.unpack_from('<4i', data, pos + 16 * i)
    if virtual >= start + length or virtual + size <= start:
        continue
    tag, block_size, _, total = struct.unpack_from('<4I', data, physical)
    if tag != 0x9e2a83c1 or total != size or not 0 < block_size <= 1024 * 1024:
        raise ValueError('Invalid compressed chunk')
    blocks = (size + block_size - 1) // block_size
    source = physical + 16 + blocks * 8
    for j in range(blocks):
        compressed, expanded = struct.unpack_from('<2I', data, physical + 16 + j * 8)
        if not 0 < expanded <= block_size or source + compressed > len(data):
            raise ValueError('Invalid compressed block')
        if virtual < start + length and virtual + expanded > start:
            block = lzo.decompress(data[source:source + compressed], header=False, buflen=expanded)
            if len(block) != expanded:
                raise ValueError('Incomplete decompression')
            lo, hi = max(start, virtual), min(start + length, virtual + expanded)
            out[lo - start:hi - start] = block[lo - virtual:hi - virtual]
            covered[lo - start:hi - start] = b'\1' * (hi - lo)
        source += compressed
        virtual += expanded
if not all(covered):
    raise ValueError('Export not fully covered by package chunks')
movies = []
for m in re.finditer(b'(?:FWS|GFX|CWS|CFX)', out):
    offset = m.start()
    if offset + 8 > len(out):
        continue
    size = struct.unpack_from('<I', out, offset + 4)[0]
    if not 8 < size <= 32 * 1024 * 1024 or out[offset + 3] >= 40:
        continue
    if m.group() in (b'FWS', b'GFX') and offset + size <= len(out):
        movies.append(bytes(out[offset:offset + size]))
    elif m.group() in (b'CWS', b'CFX'):
        decoder = zlib.decompressobj()
        try:
            body = decoder.decompress(out[offset + 8:], size)
        except zlib.error:
            continue
        if decoder.eof and len(body) == size - 8:
            used = len(out) - offset - 8 - len(decoder.unused_data)
            movies.append(bytes(out[offset:offset + 8 + used]))
if len(movies) != 1:
    raise ValueError('No unique validated movie payload')
a.output.parent.mkdir(parents=True, exist_ok=True)
with a.output.open('xb') as f:
    f.write(movies[0])
print(json.dumps({'movie': a.movie, 'package_sha256': meta['sha256'],
                  'output': str(a.output), 'bytes': len(movies[0]),
                  'sha256': hashlib.sha256(movies[0]).hexdigest()}))
