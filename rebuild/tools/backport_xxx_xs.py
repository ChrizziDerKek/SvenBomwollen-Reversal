"""Create the explicitly adapted XXX XS -> Zwo v6 loose package. Original archives are read-only."""
from pathlib import Path, PurePosixPath
import argparse
import collections
import hashlib
import json
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'games/svenxxx/01_XS.pak'
OUT = ROOT / 'rebuild/backports/XXX_XS'
NAME = 'XXX_XS'
BODY_SIZE = 34648


def integer(data, offset):
    return struct.unpack_from('<i', data, offset)[0]


def put(data, offset, value):
    struct.pack_into('<i', data, offset, value)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def archive_files(path):
    """The same bounded MUDGE4.0 tree layout as decoder/Program.cs, in memory."""
    raw = path.read_bytes()
    assert raw[:8] == b'MUDGE4.0'
    payload_end = struct.unpack_from('<I', raw, 56)[0]
    files = {}

    def node(position, parent='', depth=0):
        assert depth < 64 and position + 9 <= len(raw)
        kind = raw[position]
        length = struct.unpack_from('<I', raw, position + 5)[0]
        position += 9
        assert length < 4096 and position + length <= len(raw)
        name = raw[position:position + length].decode('latin1').replace('\\', '/')
        position += length
        full = '/'.join(p for p in [parent, name] if p)
        assert not PurePosixPath(full).is_absolute() and '..' not in PurePosixPath(full).parts
        if kind == 1:
            count = struct.unpack_from('<I', raw, position)[0]
            position += 4
            assert count < 10000
            for _ in range(count):
                position = node(position, full, depth + 1)
        elif kind == 2:
            _, offset, size, _ = struct.unpack_from('<4I', raw, position)
            position += 16
            offset ^= 0xFFAA5533
            size ^= 0x3355AAFF
            assert offset + size <= payload_end and full not in files
            files[full] = bytes(b ^ 0x88 for b in raw[offset:offset + size])
        else:
            raise ValueError(f'Unknown archive node {kind}')
        return position

    node(payload_end + 17)
    return files


def convert(data):
    assert integer(data, 0) == 7
    base = 16 + integer(data, 12)
    assert 16 <= base <= 272 and len(data) == base + BODY_SIZE + 4096
    target = bytearray(data[:base + BODY_SIZE])
    put(target, 0, 6)
    # XXX bit 0 enables UFO presentation; Zwo's whole word instead hides it.
    put(target, 8, 0 if integer(data, 8) & 1 else 1)
    records = [base + 9048 + i * 100 for i in range(256)]
    players = [i for i, off in enumerate(records) if integer(data, off + 44) == 0]
    assert players == [0, 1], players
    edits = {'removed_player_slots': [1], 'turtle_to_goat_slots': [],
             'removed_mermaid_slots': [], 'removed_help_art_slots': [],
             'stripped_help_ids': [], 'source_flags': integer(data, 8)}
    put(target, records[1] + 44, -1)  # Keep slots stable; never shift backlinks.
    for slot, off in enumerate(records):
        kind = integer(data, off + 44)
        assert kind in {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16, 17, 18, 26, 28}
        if kind == 26:
            put(target, off + 44, 12)
            put(target, off + 16, 0)  # XXX byte +17 carries the animal family.
            tile = integer(data, off + 80)
            assert integer(data, base + tile * 24) == 13
            put(target, base + tile * 24, 8)  # Zwo goat occupancy.
            put(target, base + tile * 24 + 8, slot)
            edits['turtle_to_goat_slots'].append(slot)
        elif kind == 28:
            put(target, off + 44, -1)
            tile = integer(data, off + 80)
            assert integer(data, base + tile * 24) == 15
            put(target, base + tile * 24, 0)
            put(target, base + tile * 24 + 8, -1)
            edits['removed_mermaid_slots'].append(slot)
        elif kind == 1 and integer(data, off + 68) == 40:
            put(target, off + 44, -1)
            edits['removed_help_art_slots'].append(slot)
    for tile in range(374):
        off = base + tile * 24
        value = struct.unpack_from('<I', data, off + 20)[0]
        if value >> 16:
            edits['stripped_help_ids'].append({'tile': tile, 'help_id': value >> 16})
        put(target, off + 20, value & 0xffff)
        if integer(target, off) == 100:
            put(target, off, 0)
    x, y = struct.unpack_from('<2f', target, records[0])
    spawn = (int((y + 60) // 40) + 1) * 22 + int((x + 60) // 40) + 1
    assert integer(target, base + spawn * 24) == 0
    put(target, base + spawn * 24, 100)
    put(target, records[0] + 80, spawn)
    destinations = [integer(target, base + 8980 + i * 4) for i in range(16)]
    valid = [t for t in destinations if 0 <= t < 374 and integer(target, base + t * 24) == 10]
    assert len(valid) == integer(target, base + 8976) and len(set(valid)) == len(valid)
    for i, tile in enumerate(valid + [-1] * (16 - len(valid))):
        put(target, base + 8980 + i * 4, tile)
    validate(target)
    edits['changed_byte_ranges'] = []
    for off, (old, new) in enumerate(zip(data, target)):
        if old == new:
            continue
        ranges = edits['changed_byte_ranges']
        if ranges and ranges[-1]['end'] == off:
            ranges[-1]['end'] = off + 1
        else:
            ranges.append({'start': off, 'end': off + 1})
    return bytes(target), data[base + BODY_SIZE:], edits


def validate(data):
    base = 16 + integer(data, 12)
    assert integer(data, 0) == 6 and len(data) == base + BODY_SIZE
    kinds = collections.Counter()
    for slot in range(256):
        off = base + 9048 + slot * 100
        kind = integer(data, off + 44)
        kinds[kind] += 1
        assert kind in {-1, *range(19)} and kind not in (11, 13)
        if kind >= 2:
            tile = integer(data, off + 80)
            assert 0 <= tile < 374
            if kind in (2, 3, 4, 5, 6, 12):
                assert integer(data, base + tile * 24) == (8 if kind == 12 else kind)
            elif kind in (7, 8, 9, 10, 14, 15, 16, 17, 18):
                assert integer(data, base + tile * 24 + 16) == slot
                assert data[base + tile * 24 + 12] == {7:130, 8:131, 9:129, 10:128, 14:132, 15:133, 16:134, 17:135, 18:136}[kind]
    assert kinds[0] == 1 and kinds[2] + kinds[12] > 0
    spawners = collections.Counter()
    for tile in range(374):
        off = base + tile * 24
        assert integer(data, off) in {0, 1, 2, 3, 4, 5, 6, 8, 10, 11, 100}
        assert data[off + 12] in {0, *range(128, 137)}
        marker = integer(data, off + 20)
        assert marker in (0, 140, 141, 142)
        spawners[marker] += 1
    assert all(spawners[m] <= 16 for m in (140, 141, 142))
    count = integer(data, base + 8976)
    assert 0 <= count <= 16
    for i in range(count):
        tile = integer(data, base + 8980 + i * 4)
        assert 0 <= tile < 374 and integer(data, base + tile * 24) == 10


def build():
    source = archive_files(SOURCE)
    root = ET.fromstring(source['01_XS/01_XS.xml'])
    root.tag = 'sven2'
    package = root.find('package')
    package.set('name', NAME)
    package.find('dependencies/requires').set('file', 'Sven2.dat')
    files = {}
    report = {'source_archive_sha256': sha(SOURCE.read_bytes()), 'source_version': 7,
              'target_version': 6, 'package': NAME, 'levels': [], 'policy':
              'Single-player Zwo adaptation: turtles become goats, mermaids/help removed; Zwo actors, rules, bonus screens and audio. No multiplayer, alternate-player bonus modes or weather.'}
    for path, data in sorted(source.items()):
        relative = path.removeprefix('01_XS/')
        if relative.endswith('.xml'):
            continue
        if relative.endswith('.lvl'):
            converted, help_data, edits = convert(data)
            files[relative] = converted
            files['source-metadata/' + Path(relative).stem + '.v7-help.bin'] = help_data
            report['levels'].append({'map': relative, 'source_sha256': sha(data), 'target_sha256': sha(converted), **edits})
        else:
            files[relative] = data
    for element in root.iter():
        for key, value in list(element.attrib.items()):
            if value.startswith('01_XS/'):
                element.set(key, NAME + value[len('01_XS'):])
    sets = {s.get('name').lower(): s.get('name') for s in package.findall('tilesets/tileset')}
    levels = package.findall('levelsets/levelset/level')
    assert len(levels) == len(report['levels']) == 30
    for level in levels:
        level.set('tileset', sets[level.get('tileset').lower()])
    ET.indent(root)
    files[NAME + '.xml'] = ET.tostring(root, encoding='utf-8', xml_declaration=True) + b'\n'
    for e in root.iter():
        for key in ('map', 'background', 'music', 'file'):
            path = e.get(key)
            if path and path.startswith(NAME + '/'):
                assert path[len(NAME)+1:] in files, path
    report['files_sha256'] = {p: sha(d) for p, d in sorted(files.items())}
    files['manifest.json'] = (json.dumps(report, indent=2) + '\n').encode()
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    files = build()
    assert files == build(), 'Conversion is not deterministic'
    for name, data in files.items():
        path = OUT / name
        if args.check:
            assert path.read_bytes() == data, f'Stale output: {path}'
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    print(f'{NAME}: 30 converted levels, references and deterministic output verified.')


if __name__ == '__main__':
    main()
