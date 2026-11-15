"""Read-only queries to the three explicitly supplied local IDA MCP servers."""
import json
from pathlib import Path
import urllib.request

OUT = Path(__file__).parent / 'ida'


def call(port, name, arguments, label):
    assert port in (13337, 13338, 13339)
    assert name in ('server_health', 'lookup_funcs', 'list_funcs', 'find_regex',
                    'find', 'xrefs_to', 'decompile', 'get_bytes', 'disasm')
    request = {'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
               'params': {'name': name, 'arguments': arguments}}
    response = json.load(urllib.request.urlopen(urllib.request.Request(
        f'http://127.0.0.1:{port}/mcp', data=json.dumps(request).encode(),
        headers={'Content-Type': 'application/json'}), timeout=90))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (label + '.json')).write_text(json.dumps(response, indent=2), encoding='utf-8')
    result = response.get('result', {})
    value = result.get('structuredContent')
    if value is None:
        text = '\n'.join(c.get('text', '') for c in result.get('content', []) if c.get('type') == 'text')
        try:
            value = json.loads(text)
        except ValueError:
            value = text
    if isinstance(value, dict) and 'code' in value:
        (OUT / (label + '.txt')).write_text(value['code'], encoding='utf-8')
    return value
