"""Legacy profile command alias and existing IDA RPC helper; no game process needed."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from test_menu import Process, one

def ida(method, **params):
    request = urllib.request.Request('http://127.0.0.1:13337/mcp',
        data=json.dumps(dict(jsonrpc='2.0', id=1, method=method, params=params)).encode(),
        headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=25) as response:
        value = json.load(response)
    if 'error' in value:
        raise RuntimeError(value)
    return value['result']

def main():
    from generate_menu_profile import main as generate
    generate()

if __name__=='__main__':
    main()
