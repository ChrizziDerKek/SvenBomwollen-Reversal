"""Inventory every supplied original pack/core archive without modifying game data."""
from pathlib import Path
import collections
import hashlib
import json
import re
import struct
import xml.etree.ElementTree as ET
from backport_xxx_xs import archive_files

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'rebuild/research/compatibility'


def counts(values):
    return dict(sorted(collections.Counter(values).items()))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inspect_level(data):
    version, word4, flags, length = struct.unpack_from('<4i', data)
    assert version in (6, 7, 12), f'Unrecognized level version {version}'
    assert 0 <= length < 256
    base = 16 + length
    width, height, capacity, stride = (62, 47, 2048, 112) if version == 12 else (22, 17, 256, 100)
    grid_size = width * height * 24
    object_start = base + grid_size + 72
    object_end = object_start + capacity * stride
    assert len(data) == object_end + {6: 0, 7: 4096, 12: 5552}[version], (version, len(data), object_end)
    cells = [struct.unpack_from('<6i', data, base + i * 24) for i in range(width * height)]
    objects = [data[object_start+i*stride:object_start+(i+1)*stride] for i in range(capacity)]
    kinds = [struct.unpack_from('<i', r, 44)[0] for r in objects]
    active = [(i, k, r) for i, (k, r) in enumerate(zip(kinds, objects)) if k != -1]
    samples = {}
    for i, k, r in active:
        if k not in samples:
            samples[k] = {'slot': i, 'hex': r.hex(), 'words': list(struct.unpack('<'+'i'*(stride//4), r))}
    positions = [struct.unpack_from('<2f', r, 8) for _, _, r in active]
    source_border = [cells[y*width+x][0] for y in range(height) for x in range(width)
                     if x in (0, width-1) or y in (0, height-1)]
    linked = collections.defaultdict(collections.Counter)
    item_linked = collections.defaultdict(collections.Counter)
    for cell in cells:
        slot = cell[2]
        if 0 <= slot < capacity:
            linked[cell[0]][kinds[slot]] += 1
        item_slot = (cell[4] & 65535) if version == 12 else cell[4]
        if cell[3] & 255 and 0 <= item_slot < capacity:
            item_linked[cell[3]&255][kinds[item_slot]] += 1
    teleport_count = struct.unpack_from('<i', data, base + grid_size)[0]
    teleport_slots = list(struct.unpack_from('<16i', data, base + grid_size + 4))
    return {'version': version, 'header_word4': word4, 'flags': flags,
            'name': data[16:base].decode('latin1'), 'bytes': len(data), 'sha256': digest(data),
            'padded_grid': [width, height], 'capacity': capacity, 'object_stride': stride,
            'object_start': object_start, 'suffix_bytes': len(data)-object_end,
            'timer': struct.unpack_from('<f', data, object_start-4)[0],
            'object_kinds': counts(k for _, k, _ in active), 'object_samples': samples,
            'pass_low_bytes': counts(r[16] for _, _, r in active),
            'pass_words': counts(struct.unpack_from('<I', r, 16)[0] for _, _, r in active),
            'terrain': counts(c[0] for c in cells), 'border': counts(source_border),
            'item_words': counts(c[3] for c in cells), 'item_low16': counts(c[3]&65535 for c in cells),
            'spawner_words': counts(c[5] for c in cells), 'spawner_low16': counts(c[5]&65535 for c in cells),
            'spawner_markers': counts(data[base+i*24+18] if version==12 else (c[5]&65535)
                                     for i, c in enumerate(cells)),
            'terrain_actor_links': {k: dict(v) for k, v in linked.items()},
            'item_actor_links': {k: dict(v) for k, v in item_linked.items()},
            'world_bounds_of_active_origins': [min(x for x, y in positions), min(y for x, y in positions),
                                                max(x for x, y in positions), max(y for x, y in positions)],
            'player_slots': [i for i, k, _ in active if k == 0],
            'teleport_count': teleport_count, 'teleport_slots': teleport_slots,
            'suffix_sha256': digest(data[object_end:])}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'coverage': 'Every .pak and .dat immediately inside all three supplied games folders', 'games': {}}
    for game in ('sven2', 'svenxxx', 'sven004'):
        packs = []
        core = {'parameters': {}, 'sprites': {}, 'sounds': {}, 'xml_files': []}
        for archive in sorted((ROOT/'games'/game).iterdir()):
            if archive.suffix.lower() not in ('.pak', '.dat'):
                continue
            members = archive_files(archive)
            xmls = []
            for path, data in members.items():
                if path.lower().endswith('.xml'):
                    try:
                        root = ET.fromstring(data)
                    except (ET.ParseError, ValueError):
                        # Shipped legacy StringTable XML declares UTF-8 but uses
                        # Windows-1252 and unescaped ampersands. Original bytes
                        # are retained; normalization is for inventory parsing only.
                        try:
                            text = data.decode('utf-8-sig')
                        except UnicodeDecodeError:
                            text = data.decode('cp1252')
                        text = re.sub(r'&(?!#\d+;|#x[0-9a-fA-F]+;|[A-Za-z][\w]*;)', '&amp;', text)
                        text = re.sub(r'(\bvalue=")([^"]*)(")',
                                      lambda m: m[1]+m[2].replace('<', '&lt;').replace('>', '&gt;')+m[3], text)
                        root = ET.fromstring(text)
                    xmls.append((path, root))
                    if archive.suffix.lower() == '.dat':
                        core['xml_files'].append(path)
                        (OUT/'core-xml'/game).mkdir(parents=True, exist_ok=True)
                        (OUT/'core-xml'/game/Path(path).name).write_bytes(data)
                        for e in root.iter():
                            key = {'parameter': 'parameters', 'sprite': 'sprites', 'sound': 'sounds'}.get(e.tag.lower())
                            if key and e.get('name'):
                                core[key][e.get('name')] = e.attrib
            levels = {p: inspect_level(d) for p, d in members.items() if p.lower().endswith('.lvl')}
            descriptors = []
            referenced = set()
            for path, root in xmls:
                for package in root.iter('package'):
                    sets = []
                    for episode in package.findall('levelsets/levelset'):
                        entries = [dict(e.attrib) for e in episode.findall('level')]
                        sets.append({'name': episode.get('name'), 'levels': entries})
                        referenced.update(e['map'].replace('\\', '/').lower() for e in entries if 'map' in e)
                    descriptors.append({'path': path, 'root': root.tag, 'package': package.get('name'),
                                        'levelsets': sets, 'tilesets': [dict(t.attrib, tiles=[e.attrib for e in t.findall('tile')])
                                                                     for t in package.findall('tilesets/tileset')]})
            pack = {'archive': archive.relative_to(ROOT).as_posix(), 'sha256': digest(archive.read_bytes()),
                    'members': len(members), 'member_hashes': {p: digest(d) for p, d in members.items()},
                    'descriptors': descriptors, 'levels': levels,
                    'unreferenced_levels': sorted(p for p in levels if p.lower() not in referenced),
                    'missing_referenced_levels': sorted(referenced-set(p.lower() for p in levels))}
            packs.append(pack)
            print(game, archive.name, len(levels), 'levels', 'versions', counts(v['version'] for v in levels.values()), flush=True)
        summary = {}
        all_levels = [l for p in packs for l in p['levels'].values()]
        for field in ('object_kinds', 'terrain', 'item_low16', 'spawner_low16', 'spawner_markers'):
            tally = collections.Counter()
            for level in all_levels:
                tally.update(level[field])
            summary[field] = dict(sorted(tally.items()))
        summary['levels'] = len(all_levels)
        summary['levelsets'] = sum(len(d['levelsets']) for p in packs for d in p['descriptors'])
        summary['versions'] = counts(l['version'] for l in all_levels)
        summary['header_flags'] = counts(l['flags'] for l in all_levels)
        summary['player_count_histogram'] = counts(len(l['player_slots']) for l in all_levels)
        summary['unreferenced_levels'] = sum(len(p['unreferenced_levels']) for p in packs)
        summary['missing_references'] = sum(len(p['missing_referenced_levels']) for p in packs)
        report['games'][game] = {'summary': summary, 'archives': packs, 'core': core}
        print(game, 'TOTAL', summary, flush=True)
    (OUT/'inventory.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Wrote', OUT/'inventory.json')


if __name__ == '__main__':
    main()
