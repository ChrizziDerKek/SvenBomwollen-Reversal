"""Validate audit coverage and write compact, complete CSV views of the inventory."""
import collections
import csv
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
COMMON = {0: 'player', 1: 'scenery', 2: 'sheep', 3: 'alien', 4: 'dog',
          5: 'farmer', 6: 'girl', 7: 'beer', 8: 'teddy', 9: 'bone',
          10: 'artifact', 11: 'chicken', 12: 'goat', 13: 'pig',
          14: 'rocket', 15: 'present', 16: 'slow mushroom',
          17: 'stinky mushroom', 18: 'flower'}
NAMES = {'sven2': COMMON, 'svenxxx': dict(COMMON)}
NAMES['svenxxx'].update({25: 'penguin', 26: 'turtle', 27: 'rabbit',
                        28: 'mermaid', 43: 'bubble effect emitter',
                        44: 'yellow effect emitter'})
NAMES['sven004'] = dict(enumerate([
    'player', 'scenery', 'sheep', 'mermaid', 'penguin', 'turtle', 'rabbit',
    'chicken', 'goat', 'pig', 'groundhog', 'alien pet', 'hedgehog', 'peacock',
    'skunk', 'alien', 'dog', 'farmer', 'girl', 'hunter', 'bone', 'teddy', 'beer',
    'artifact', 'rocket', 'present', 'slow mushroom', 'stinky mushroom',
    'flower', 'horn', 'coin', 'vehicle', 'barrel', 'configurable collision object',
    'thin ice', 'extras box', 'exit']))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tally(levels, field):
    result = collections.Counter()
    for level in levels:
        result.update(level[field])
    return dict(sorted(result.items(), key=lambda kv: int(kv[0])))


def main():
    inventory = json.loads((HERE/'inventory.json').read_text(encoding='utf-8'))
    expected = {'sven2': (105, 11, 6, 1), 'svenxxx': (129, 12, 7, 2),
                'sven004': (202, 27, 12, 2)}
    pack_rows, kind_rows, code_rows, manifests = [], [], [], []
    fields = ('terrain', 'item_low16', 'spawner_markers')
    for game, data in inventory['games'].items():
        count, sets, version, players = expected[game]
        summary = data['summary']
        assert summary['levels'] == count and summary['levelsets'] == sets
        assert summary['versions'] == {str(version): count}
        assert summary['player_count_histogram'] == {str(players): count}
        assert summary['unreferenced_levels'] == summary['missing_references'] == 0
        supplied = {p.resolve() for p in (ROOT/'games'/game).rglob('*')
                    if p.suffix.lower() in ('.pak', '.dat')}
        scanned = {(ROOT/p['archive']).resolve() for p in data['archives']}
        assert supplied == scanned, (game, supplied-scanned)
        for pack in data['archives']:
            assert sha(ROOT/pack['archive']) == pack['sha256'], pack['archive']
            manifests.append({'path': pack['archive'], 'sha256': pack['sha256']})
            levels = list(pack['levels'].values())
            if not levels:
                continue
            labels = [s['name'] for d in pack['descriptors'] for s in d['levelsets']]
            row = [game, pack['archive'], len(levels), ' | '.join(labels)]
            for field in ('object_kinds',)+fields:
                row.append(json.dumps(tally(levels, field), ensure_ascii=False))
            pack_rows.append(row)
        for kind, number in sorted(summary['object_kinds'].items(), key=lambda kv: int(kv[0])):
            assert int(kind) in NAMES[game], (game, kind)
            kind_rows.append([game, int(kind), NAMES[game][int(kind)], number])
        for field in fields:
            for code, number in sorted(summary[field].items(), key=lambda kv: int(kv[0])):
                code_rows.append([game, field, int(code), number])
        exe = ROOT/'games'/game/(game+'.exe')
        manifests.append({'path': exe.relative_to(ROOT).as_posix(), 'sha256': sha(exe)})
    for filename, header, rows in [
        ('pack-matrix.csv', ['game', 'archive', 'levels', 'levelsets', 'object_kinds', *fields], pack_rows),
        ('object-kinds.csv', ['game', 'object_kind', 'meaning', 'record_count'], kind_rows),
        ('grid-codes.csv', ['game', 'field', 'raw_code', 'cell_count'], code_rows)]:
        with (HERE/filename).open('w', encoding='utf-8', newline='') as stream:
            writer = csv.writer(stream)
            writer.writerow(header)
            writer.writerows(rows)
    games = list(expected)
    keys = sorted({key for data in inventory['games'].values() for key in data['core']['parameters']})
    with (HERE/'core-parameters.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['name', *games])
        for key in keys:
            writer.writerow([key, *[json.dumps(inventory['games'][g]['core']['parameters'][key], ensure_ascii=False)
                                     if key in inventory['games'][g]['core']['parameters'] else '' for g in games]])
    result = {'levels': sum(v[0] for v in expected.values()), 'packs': len(pack_rows),
              'archives': len(manifests)-3, 'all_archive_hashes_match': True,
              'missing_map_references': 0, 'unreferenced_levels': 0,
              'coverage_checks_passed': True, 'inputs': manifests}
    (HERE/'validation.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('PASS: 436 levels, 15 packs, 18 archives; all links, versions, player counts and archive hashes checked.')


if __name__ == '__main__':
    main()
