"""Offline replay of original window UI update against copied live menu state."""
import argparse
import json
import struct
from pathlib import Path
import sys
import protected_entry_probe as probe
from unicorn.x86_const import UC_X86_REG_ECX

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from test_menu import one

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--tab',type=int,default=3);parser.add_argument('--search',action='store_true');args=parser.parse_args()
    pid=one('csgo.exe')
    thread=json.loads((ROOT/f'analysis/menu/threads-{pid}.json').read_text())['threads'][0]
    teb=int(thread['teb'],16)
    modules=json.loads((ROOT/f'analysis/menu/render-state-{pid}.json').read_text())['modules']
    exports=json.loads((ROOT/f'analysis/menu/runtime-standard-exports-{pid}.json').read_text())['exports']
    models={};observed={}
    for e in exports:
        if e['forwarded']:continue
        address=int(e['address'],16);observed.setdefault(address,[]).append(e['module']+'!'+e['name'])
        if e['module'].lower()=='user32.dll' and e['name'] in ('GetFocus','GetForegroundWindow','GetActiveWindow'):
            value=thread[{'GetFocus':'observed_focus','GetForegroundWindow':'observed_foreground','GetActiveWindow':'observed_active'}[e['name']]]
            models[address]='observed_win32_handle:'+value
        if e['module'].lower() in ('ucrtbase.dll','vcruntime140.dll'):
            name=e['name']
            if name in ('malloc','_malloc_base','calloc','_calloc_base','realloc','_realloc_base','free','_free_base','memcpy','memmove','memset','strlen'):
                models[address]=name.removeprefix('_').removesuffix('_base')
            if name in ('_lock_locales','_unlock_locales'):models[address]='single_thread_locale_lock'
    with probe.Process(pid) as process:
        image=bytearray(process.read(probe.BASE,0x1411000))
        # Explicit hypothetical no-input frame: this is not a live interaction.
        image[0x807F08:0x807F08+114*4]=bytes(114*4)
        struct.pack_into('<ii',image,0x807F00,-100,-100)
        struct.pack_into('<I',image,0x804DF8,0x447F33B0)
        struct.pack_into('<I',image,0x4B33B0+0xE0,args.tab)
        image[0x804E07]=int(args.search)
        image[0x804DF3]=1
        image[0x4B33B0+0x99]=1
        struct.pack_into('<f',image,0x4B33B0+0x94,1.0)
        surface=struct.unpack_from('<I',image,0x4DE15C)[0]
        vtable=struct.unpack('<I',process.read(surface,4))[0]
        text_size=struct.unpack('<I',process.read(vtable+79*4,4))[0]
        models[text_size]='synthetic_source_text_size'
        peb=struct.unpack('<I',process.read(teb+0x30,4))[0]
        build=struct.unpack('<H',process.read(peb+0xAC,2))[0]
        expected=(0x306C4+0x800+(build<<7))^0x1C85CEC8
        fields={0xB8:0x59DB1540,0x20:expected^0x40DF21BC}
        result=probe.probe(process,bytes(image),teb,0x3504B0,0,fields,
            standard_models=models,initial_registers={UC_X86_REG_ECX:0x447F33B0},step_budget=3000000,
            reject_null_reads=True,observed_exports=observed,
            external_code_ranges=[(m['base'],m['base']+m['size']) for m in modules if Path(m['path']).name.lower() in
                ('ucrtbase.dll','msvcp140.dll','vcruntime140.dll','kernelbase.dll','kernel32.dll','ntdll.dll','inputsystem.dll','engine.dll')])
    result['scope']='OFFLINE ONLY original UI update; copied actual menu state, no-input hypothetical frame, synthetic guard/allocations/text metrics. No live calls or target writes.'
    (ROOT/f'analysis/menu/native-ui-update-probe-{pid}-tab{args.tab}-search{int(args.search)}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:result.get(key) for key in ['instructions','stop_reason','last_eip_rva','synthetic_field_reads','final_stack_words','emulator_error']},indent=2))

if __name__=='__main__':main()
