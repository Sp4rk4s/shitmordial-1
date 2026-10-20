"""Read-only snapshot of actual menu-preview state, never offline-state injection."""
import json
import struct
import sys
import hashlib
from pathlib import Path
from test_menu import Process,one,ROOT

def main():
    pid=one('csgo.exe')
    fonts=json.loads((ROOT/'analysis/menu/all-default-font-record-associations.json').read_text())
    with Process(pid) as process:
        state=dict(pid=pid,handoff=process.handoff(),modules=process.modules(),scope='read-only actual preview state; visibility from user separately')
        state['primary_window_hex']=process.read(0x447F33B0,0xE8).hex()
        fields=dict(surface=0x4481E15C,engine=0x4481E174,timing=0x4481E164,guard=0x44C5BC4B,
            windows_begin=0x44B44E5C,windows_end=0x44B44E60,width=0x44B487AC,height=0x44B487B0)
        state['fields']={k:hex(struct.unpack('<I',process.read(a,4))[0]) for k,a in fields.items()}
        state['fonts']=[]
        for entry in fonts['records']:
            rva=int(entry['exact_record_candidates'][0]['rva'],16)
            state['fonts'].append(dict(rva=hex(rva),actual_record=struct.unpack('<3I',process.read(0x44340000+rva,12))))
    target=ROOT/f'analysis/menu/menu-preview-runtime-{pid}.json'
    target.write_text(json.dumps(state,indent=2)+'\n')
    print(target)
    print(json.dumps(state['fields']))

if __name__=='__main__':main()
