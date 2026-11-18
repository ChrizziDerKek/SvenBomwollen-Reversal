"""Extract untouched XXX resources and inventory the original v7 data.

Run from any directory: python rebuild/tools/PrepareXxxAssets.py
Only rebuild/data/XXX and rebuild/research/xxx/asset-report.json are written.
The existing, import-safe MUDGE reader is the authoritative archive decoder.
"""
from pathlib import Path, PureWindowsPath
import collections
import hashlib
import importlib.util
import json
import re
import struct
import sys
import xml.etree.ElementTree as Xml

# Import the read-only decoder without leaving cache files beside its source.
sys.dont_write_bytecode = True


ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / 'rebuild/data/XXX'
REPORT = ROOT / 'rebuild/research/xxx/asset-report.json'
LEVEL_BODY_SIZE = 38744
OBJECT_OFFSET = 9048
OBJECT_COUNT = 256
OBJECT_SIZE = 100
TILE_COUNT = 374
TILE_SIZE = 24


def Sha256(data):
    return hashlib.sha256(data).hexdigest()


def SafePath(name):
    # Reject Windows aliases as well as POSIX traversal, even on another host.
    if not name or '\\' in name or PureWindowsPath(name).drive or name.startswith('/'):
        raise ValueError(f'Unsafe archive path: {name!r}')
    reservedNames = { 'CON', 'PRN', 'AUX', 'NUL', *('COM' + str(i) for i in range(1, 10)), *('LPT' + str(i) for i in range(1, 10)) }
    for part in name.split('/'):
        if not part or part in ('.', '..') or part.endswith((' ', '.')) or any(ord(c) < 32 or c in '<>:"|?*' for c in part) or part.split('.')[0].upper() in reservedNames:
            raise ValueError(f'Unsafe archive path: {name!r}')
    destination = DESTINATION / name
    if DESTINATION.resolve() not in destination.resolve().parents:
        raise ValueError(f'Archive path escapes destination: {name!r}')
    # Do not traverse an existing junction/symlink, including a redirected root.
    for parent in (DESTINATION, *destination.parents):
        if parent == ROOT:
            break
        if parent.is_symlink() or (parent.exists() and parent.resolve() != parent.absolute()):
            raise ValueError(f'Redirected extraction path: {parent}')
    if destination.is_symlink():
        raise ValueError(f'Redirected extraction file: {destination}')
    return destination


def XmlInventory(data):
    repairs = []
    try:
        text = data.decode('utf-8-sig')
        encoding = 'utf-8'
    except UnicodeDecodeError:
        text = data.decode('cp1252')
        encoding = 'cp1252'
        repairs.append('Declared UTF-8 is invalid; decoded inventory as Windows-1252.')
    # Native descriptors contain literal < and & inside quoted attributes.
    # Sanitize only the inventory representation, never the extracted bytes.
    text = re.sub(r'<!--.*?-->', '', text, flags=re.S)
    replacements = collections.Counter()

    def EscapeAttribute(match):
        value = match.group(2)
        value, ampCount = re.subn(r'&(?!#\d+;|#x[0-9a-fA-F]+;|amp;|lt;|gt;|quot;|apos;)', '&amp;', value)
        replacements['literal_ampersands'] += ampCount
        replacements['literal_less_than'] += value.count('<')
        return match.group(1) + value.replace('<', '&lt;') + match.group(3)

    text = re.sub(r'(=\s*")([^"]*)(")', EscapeAttribute, text)
    if any(replacements.values()):
        repairs.append(dict(escaped_attribute_characters=dict(replacements)))
    root = Xml.fromstring(text)
    elements = list(root.iter())
    return root, dict(root=root.tag, inventory_encoding=encoding, inventory_only_repairs=repairs,
        tag_counts=dict(sorted(collections.Counter(element.tag for element in elements).items())),
        descriptors=[dict(tag=element.tag, attributes=dict(element.attrib)) for element in elements if element.attrib])


def LevelInventory(name, data):
    if len(data) < 16:
        raise ValueError(f'Truncated level: {name}')
    version, header4, header8, nameLength = struct.unpack_from('<4I', data)
    expected = 16 + nameLength + LEVEL_BODY_SIZE
    if version != 7 or len(data) != expected:
        raise ValueError(f'Invalid v7 level {name}: version={version}, bytes={len(data)}, expected={expected}')
    body = 16 + nameLength
    kinds = collections.Counter(struct.unpack_from('<i', data, body + OBJECT_OFFSET + slot * OBJECT_SIZE + 44)[0] for slot in range(OBJECT_COUNT))
    tiles = collections.Counter(struct.unpack_from('<i', data, body + slot * TILE_SIZE)[0] for slot in range(TILE_COUNT))
    return dict(path=name, version=version, header4=header4, header8=header8,
        name=data[16:body].decode('cp1252'), name_hex=data[16:body].hex(), size=len(data),
        expected_size=expected, object_kind_counts=dict(sorted(kinds.items())), tile_kind_counts=dict(sorted(tiles.items())),
        player_count=kinds[0], extension_bytes=4096)


def Main():
    spec = importlib.util.spec_from_file_location('SvenArchiveValidation', ROOT / 'docs/validation/inspect_validation.py')
    validation = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validation)
    source = ROOT / 'games/svenxxx'
    packs = sorted(source.glob('*.pak'))
    if len(packs) != 5:
        raise ValueError(f'Expected five XXX packs; found {len(packs)}')
    sources = [source / 'SvenXXX.dat', *packs]
    report = dict(schema_version=1, decoder='docs/validation/inspect_validation.py:entries',
        decoder_sha256=Sha256(Path(validation.__file__).read_bytes()), extraction_root='rebuild/data/XXX',
        preserves_original_bytes=True, sources=[], files=[], xml=[], levels=[], references=[],
        notes=[
            'All paths and payloads retain archive spelling, case and bytes; no Zwo resources are overwritten.',
            'Deployment must retain the SvenXXX and five pack root directories below its selected resource root.',
            'Kind counts are raw serialized identifiers, not inferred behavior or Zwo-compatible mappings.',
            'Sprite rows/cols and all descriptor attributes are retained; do not infer frame layouts from filenames.',
            'Native XML may require quote-aware attribute escaping and Windows-1252 decoding despite its declaration.',
            'Archive .dat dependencies describe original containers; loose-file deployment must resolve extracted roots.',
        ])
    payloads = {}
    foldedPaths = {}
    # Validate every archive and path before creating extracted files.
    for archive in sources:
        data = archive.read_bytes()
        if data[:8] != b'MUDGE4.0':
            raise ValueError(f'Not a MUDGE4.0 archive: {archive}')
        entries, treeEnd = validation.entries(data)
        if treeEnd > len(data):
            raise ValueError(f'Truncated archive tree: {archive}')
        sourceName = archive.relative_to(ROOT).as_posix()
        inventory = dict(path=sourceName, sha256=Sha256(data), size=len(data), files=len(entries), tree_end=treeEnd,
            extension_counts=dict(sorted(collections.Counter(Path(name).suffix.lower() for name, _, _ in entries).items())), pack_xml_paths=[])
        for name, offset, payload in entries:
            SafePath(name)
            folded = name.casefold()
            if folded in foldedPaths:
                raise ValueError(f'Duplicate or case-colliding archive path: {name}, {foldedPaths[folded]}')
            foldedPaths[folded] = name
            payloads[name] = payload
            report['files'].append(dict(path=name, archive=sourceName, payload_offset=offset, size=len(payload), sha256=Sha256(payload)))
            if name.lower().endswith('.lvl'):
                report['levels'].append(LevelInventory(name, payload))
            if name.lower().endswith('.xml'):
                root, xmlInfo = XmlInventory(payload)
                xmlInfo['path'] = name
                report['xml'].append(xmlInfo)
                if root.find('package') is not None:
                    inventory['pack_xml_paths'].append(name)
                for element in root.iter():
                    for attribute in ('file', 'map', 'music', 'background'):
                        if element.get(attribute):
                            report['references'].append(dict(xml=name, tag=element.tag, name=element.get('name'), attribute=attribute, value=element.get(attribute)))
        report['sources'].append(inventory)
    if len(report['levels']) != 129:
        raise ValueError(f"Expected 129 levels, found {len(report['levels'])}")
    for reference in report['references']:
        name = reference['value']
        candidate = re.sub(r'[\\/]+', '/', name)
        if reference['tag'] == 'sprite':
            candidate = 'SvenXXX/gfx/' + candidate
            reference['resolution_rule'] = 'Sprite file relative to SvenXXX/gfx; backslash runs treated as separators (asset-backed candidate, not native code verification).'
        reference['candidate_path'] = candidate
        if candidate in payloads:
            reference['status'] = 'exact'
            reference['actual_path'] = candidate
        elif candidate.casefold() in foldedPaths:
            reference.update(status='case_mismatch', actual_path=foldedPaths[candidate.casefold()])
        elif name in { archive.name for archive in sources }:
            reference['status'] = 'archive_dependency'
        else:
            reference['status'] = 'missing'
    report['sprite_images'] = []
    for xmlInfo in report['xml']:
        for descriptor in xmlInfo['descriptors']:
            if descriptor['tag'] != 'sprite':
                continue
            attributes = descriptor['attributes']
            reference = next(reference for reference in report['references'] if reference['xml'] == xmlInfo['path'] and reference['tag'] == 'sprite' and reference['name'] == attributes.get('name'))
            image = dict(xml=xmlInfo['path'], attributes=attributes, status=reference['status'], actual_path=reference.get('actual_path'))
            if reference.get('actual_path'):
                bitmap = payloads[reference['actual_path']]
                if bitmap[:2] != b'BM' or len(bitmap) < 54 or struct.unpack_from('<I', bitmap, 14)[0] < 40:
                    raise ValueError(f'Unsupported sprite bitmap: {reference["actual_path"]}')
                width, height = struct.unpack_from('<ii', bitmap, 18)
                image.update(bitmap_width=width, bitmap_height=height, bits_per_pixel=struct.unpack_from('<H', bitmap, 28)[0])
            report['sprite_images'].append(image)
    report['summary'] = dict(files=len(payloads), bytes=sum(map(len, payloads.values())), levels=len(report['levels']),
        version_counts=dict(collections.Counter(level['version'] for level in report['levels'])),
        object_kind_counts=dict(sorted(sum((collections.Counter({ int(kind): count for kind, count in level['object_kind_counts'].items() }) for level in report['levels']), collections.Counter()).items())),
        tile_kind_counts=dict(sorted(sum((collections.Counter({ int(kind): count for kind, count in level['tile_kind_counts'].items() }) for level in report['levels']), collections.Counter()).items())),
        reference_status_counts=dict(collections.Counter(reference['status'] for reference in report['references'])))
    written = unchanged = 0
    for name, payload in payloads.items():
        destination = SafePath(name)
        if destination.exists() and destination.read_bytes() == payload:
            unchanged += 1
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)
        written += 1
    # The report itself is the SHA manifest, including source and member hashes.
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    encoded = (json.dumps(report, ensure_ascii=True, indent=2) + '\n').encode('utf-8')
    if not REPORT.exists() or REPORT.read_bytes() != encoded:
        REPORT.write_bytes(encoded)
    print(json.dumps(dict(report=REPORT.relative_to(ROOT).as_posix(), written=written, unchanged=unchanged, **report['summary']), indent=2))


if __name__ == '__main__':
    Main()
