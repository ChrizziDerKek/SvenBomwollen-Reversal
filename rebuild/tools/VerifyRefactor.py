"""Verify retained regression evidence and the protected backup source snapshot."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
REBUILD = ROOT / 'rebuild'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest = json.loads((REBUILD/'refactor-backup-manifest.json').read_text())
    changed = [name for name, expected in manifest.items() if digest(ROOT/name) != expected]
    assert not changed, f'Protected backup source changed: {changed}'
    old = REBUILD/'runtime/refactor-baseline-trace.jsonl'
    new = REBUILD/'runtime/refactor-current-trace.jsonl'
    assert old.read_bytes() == new.read_bytes(), 'Behavior trace differs from baseline.'
    frames = sum(bool(re.fullmatch(rb'\d+ \d+', line)) for line in new.read_bytes().splitlines())
    assert frames == 105 * 2 * 480
    fsr = REBUILD/'Sven2Bootstrap/FSR1'
    provenance = json.loads((fsr/'SOURCE.json').read_text())
    for name, expected in provenance['files'].items():
        assert digest(fsr/name) == expected, f'AMD header changed: {name}'
    result = {
        'protected_backup_sources': len(manifest),
        'backup_source_hashes_unchanged': True,
        'baseline_trace_matches': True,
        'trace_format': 'v1; UFO resource root casing normalized in trace formatter only',
        'levels': 105, 'seeds': 2, 'frames_per_seed': 480,
        'compared_frames': frames, 'trace_sha256': digest(new),
        'amd_shader_headers_unchanged': True,
    }
    (REBUILD/'refactor-validation.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'PASS: {frames} matching frames; {len(manifest)} protected backup sources unchanged; AMD headers intact.')


if __name__ == '__main__':
    main()
