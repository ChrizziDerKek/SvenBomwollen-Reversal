"""Record observed remake checks and native captures without assuming parity."""
from pathlib import Path
import datetime
import hashlib
import json
import re

BASE = Path(__file__).resolve().parents[1]

def read_log(path):
    raw = path.read_bytes()
    return raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

checks = {}
for configuration in ('Debug', 'Release'):
    path = BASE / f'runtime/checks-{configuration}.log'
    log = read_log(path)
    assert 'PASS: all remake checks.' in log, path
    assert 'PASS: 105 folder levels' in log, path
    checks[configuration] = {
        'log': str(path.relative_to(BASE)),
        'passed': [line[6:] for line in log.splitlines() if line.startswith('PASS: ')],
        'binary_sha256': sha(BASE / f'bin/Win32/{configuration}/Sven2Checks.exe'),
    }

visual = read_log(BASE / 'runtime/visual.log')
assert 'PASS: all remake checks.' in visual
capture_dir = Path(re.search(r'PASS: native UI captures in (.+)', visual)[1].strip())
captures = {p.name: sha(p) for p in sorted(capture_dir.glob('*.bmp'))}
assert {'menu.bmp', 'results.bmp', 'highscores.bmp', 'cards-first.bmp', 'cards-last.bmp', 'level.bmp'} <= captures.keys()
game = read_log(BASE / 'runtime/sven2/game.log')
assert game.rstrip().endswith('PASS') and 'StartSound: OK' in game
frames = int(re.search(r'Presented frames: (\d+)', game)[1])
assert frames > 0
lab = {}
lab_levels = len(json.loads((BASE / 'testpacks/ParityLab/manifest.json').read_text(encoding='utf-8'))['scenarios'])
for configuration, name in [('Debug', 'parity-catalog.log'), ('Release', 'parity-catalog-Release.log')]:
    path = BASE / 'runtime' / name
    log = read_log(path)
    assert f'PASS: {lab_levels} folder levels' in log and 'PASS: package catalog checks.' in log
    lab[configuration] = str(path.relative_to(BASE))
report = {
    'recorded_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'checks': checks,
    'parity_lab': {'levels': lab_levels, 'catalog_logs': lab,
                   'manifest_sha256': sha(BASE / 'testpacks/ParityLab/manifest.json')},
    'native_smoke': {'frames': frames, 'audio_requests_succeeded': True,
                     'log': 'runtime/sven2/game.log'},
    'capture_directory': str(capture_dir.relative_to(BASE)),
    'captures_sha256': captures,
    'binaries_sha256': {c: sha(BASE / f'bin/Win32/{c}/Sven2Bootstrap.exe') for c in checks},
    'limitations': [
        'Catalog simulation does not establish exhaustive behavioral equivalence.',
        'Original-game frame and audio waveform comparison remains pending.',
        'Demon states are reconstructed and tested; original runtime timing/replay comparison remains pending.',
        'Global RNG sequencing and dynamic depth ties require comparison.',
        'Local scores use UTF-8 TSV rather than original signed XML.',
        'Win32 input and windowed 32-bit video on this host.',
    ],
}
(BASE / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(f'Recorded Debug/Release checks, {len(captures)} native captures, and {frames} presented frames.')
