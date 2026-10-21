"""OFFLINE ONLY: original path initialization from a retained snapshot.

No process is opened. Virtual empty directories, CPU guard and CRT allocations
are explicit fixtures; resulting snapshots must never be injected.
"""
import argparse
import json
import struct
import zipfile
from pathlib import Path
import protected_entry_probe as probe

ROOT=Path(__file__).resolve().parents[2]


class FrozenPages:
    def __init__(self):
        self.pages={}
        self.modeled_only_pages=set()
        with zipfile.ZipFile(ROOT/'analysis/menu/menu-constructed-offline-11240.zip') as archive:
            self.manifest=json.loads(archive.read('manifest.json'))
            for region in self.manifest['regions']:
                data=archive.read(region['member'])
                for offset in range(0,len(data),4096):self.pages[region['start']+offset]=data[offset:offset+4096]
        with zipfile.ZipFile(ROOT/'analysis/menu/renderer-crt-pages-11240.zip') as archive:
            for page in json.loads(archive.read('manifest.json'))['pages']:
                self.pages.setdefault(page['address'],archive.read(page['member']))
    def read(self,address,size):
        data=bytearray()
        while size:
            base=address&~4095;offset=address-base;length=min(4096-offset,size)
            if base not in self.pages:
                if base not in self.modeled_only_pages:raise OSError('Not retained: '+hex(address))
                # Only named APIs are intercepted. Every other instruction on
                # this explicit fixture page traps instead of inventing code.
                data.extend(b'\xCC'*length)
            else:data.extend(self.pages[base][offset:offset+length])
            address+=length;size-=length
        return bytes(data)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--entry',required=True,type=lambda x:int(x,0),choices=[0x162FA0,0xE2030]);args=parser.parse_args()
    memory=FrozenPages();teb=struct.unpack('<I',memory.read(0x18,4))[0]
    image=memory.read(probe.BASE,0x140C000)
    exports=json.loads((ROOT/'analysis/menu/runtime-standard-exports-11240.json').read_text())['exports']
    models={int(address):name for address,name in memory.manifest['standard_models'].items()}
    for export in exports:
        if export['forwarded']:continue
        name=export['name'];address=int(export['address'],16)
        if name in ('CreateDirectoryW','FindFirstFileExW','FindNextFileW','FindClose'):models[address]='virtual_empty_config_'+name
        elif name in ('GetLastError','SetLastError'):models[address]={'GetLastError':'get_last_error','SetLastError':'set_last_error'}[name]
        elif name in ('_lock_locales','_unlock_locales'):models[address]='single_thread_locale_lock'
        elif name=='AreFileApisANSI':models[address]='synthetic_are_file_apis_ansi'
        elif name=='MultiByteToWideChar':models[address]='synthetic_ascii_include_to_wide'
    peb=struct.unpack('<I',memory.read(teb+0x30,4))[0];build=struct.unpack('<H',memory.read(peb+0xAC,2))[0]
    memory.modeled_only_pages={address&~4095 for address in models}
    expected=(0x306C4+0x800+(build<<7))^0x1C85CEC8
    result=probe.probe(memory,image,teb,args.entry,0,{0xB8:0x59DB1540,0x20:expected^0x40DF21BC},
        cpuid_override=dict(eax=0x306C4,ebx=0x800,ecx=0xCED82203,edx=0x78BFBFD),
        standard_models=models,step_budget=4000000,reject_null_reads=True,
        experiment_captured_destructors=True,external_code_ranges=memory.manifest['external_code_ranges'],
        snapshot_output=ROOT/f'analysis/menu/path-initialized-offline-{args.entry:x}.zip')
    result['scope']='OFFLINE ONLY frozen pages, modeled guard/CRT/empty directories; no process opened or target calls/writes'
    (ROOT/f'analysis/menu/path-initializer-{args.entry:x}-offline.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result.get(k) for k in ['instructions','stop_reason','last_eip_rva','synthetic_field_reads','original_stack_argument_reads','incoming_register_buffer_accesses','experimental_captured_object_resets','final_esp']},indent=2))


if __name__=='__main__':main()
