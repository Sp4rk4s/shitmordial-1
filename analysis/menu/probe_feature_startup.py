"""OFFLINE ONLY feature startup dependency probe; no target execution/writes."""
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
    parser=argparse.ArgumentParser();parser.add_argument('--entry',type=lambda x:int(x,0),required=True);parser.add_argument('--adapt-event-signature',action='store_true',help='OFFLINE only: use the private dispatcher proven by both current public event wrappers');parser.add_argument('--model-event-registration',action='store_true');parser.add_argument('--bind-field',action='append',default=[]);parser.add_argument('--bind-client-state',action='store_true');parser.add_argument('--bind-client-panel',action='store_true');parser.add_argument('--dry-hooks',action='store_true');parser.add_argument('--max-steps',type=int,default=3000000);args=parser.parse_args()
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
        bindings=[]
        if args.bind_client_panel:
            facts=json.loads((ROOT/f'analysis/menu/source-interface-state-{pid}.json').read_text())['factories']
            for name,version,rva in [('client.dll','VClient018',0x4DE168),('engine.dll','VEngineVGui001',0x4DE16C)]:
                singleton=next(e for e in facts[name]['entries'] if e['name']==version)
                address=int(singleton['singleton'],16)
                assert process.read(int(singleton['create'],16),6)==bytes.fromhex(singleton['factory_bytes'])
                assert struct.unpack_from('<I',image,rva)[0]==0
                struct.pack_into('<I',image,rva,address)
                bindings.append(dict(rva=hex(rva),interface=version,observed_singleton=hex(address),scope='OFFLINE field-role hypothesis, not live binding'))
        if args.bind_client_state:
            engine=struct.unpack_from('<I',image,0x4DE174)[0]
            vt=struct.unpack('<I',process.read(engine,4))[0]
            getter=struct.unpack('<I',process.read(vt+78*4,4))[0]
            code=process.read(getter,12)
            assert code[0]==0xA1 and code[5:12]==bytes.fromhex('8b809c000000c3')
            state=struct.unpack('<I',process.read(struct.unpack_from('<I',code,1)[0],4))[0]
            assert struct.unpack_from('<I',image,0x4DE198)[0]==0
            struct.pack_into('<I',image,0x4DE198,state)
            bindings.append(dict(rva='0x4DE198',observed_client_state=hex(state),getter=hex(getter),getter_bytes=code.hex(),scope='OFFLINE field-role hypothesis; no target writes'))
        for spec in args.bind_field:
            rva,module,version=spec.split(':');rva=int(rva,0)
            facts=json.loads((ROOT/f'analysis/menu/source-interface-state-{pid}.json').read_text())['factories']
            entry=next(e for e in facts[module]['entries'] if e['name']==version)
            code=process.read(int(entry['create'],16),6)
            assert code[0]==0xB8 and code[5]==0xC3,'Factory is not a verified constant singleton getter'
            singleton=struct.unpack_from('<I',code,1)[0]
            assert struct.unpack_from('<I',image,rva)[0]==0
            struct.pack_into('<I',image,rva,singleton)
            bindings.append(dict(rva=hex(rva),interface=version,singleton=hex(singleton),factory=entry['create'],factory_bytes=code.hex(),scope='Explicit OFFLINE field-role hypothesis, not a live binding'))
        peb=struct.unpack('<I',process.read(teb+0x30,4))[0]
        build=struct.unpack('<H',process.read(peb+0xAC,2))[0]
        expected=(0x306C4+0x800+(build<<7))^0x1C85CEC8
        fields={0xB8:0x59DB1540,0x20:expected^0x40DF21BC}
        from masked_scan11_model import PROFILES
        for rva,(width,digest) in PROFILES.items():models[probe.BASE+rva]=f'verified_masked_scan:{width}'
        if args.dry_hooks:
            raw=(ROOT/'primordial.bin').read_bytes()
            assert __import__('hashlib').sha256(raw).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
            assert image[0x1554F0:0x15571B]==raw[0x1554F0:0x15571B]
            models[probe.BASE+0x1554F0]='hypothetical_failed_inline_hook_request'
        if args.model_event_registration:
            events=struct.unpack_from('<I',image,0x4DE188)[0];assert events
            vt=struct.unpack('<I',process.read(events,4))[0]
            registration=struct.unpack('<I',process.read(vt+6*4,4))[0]
            models[registration]='hypothetical_source_global_event_registration'
        adaptations={}
        if args.adapt_event_signature:
            assert args.entry==0x1564B0 and args.dry_hooks,'Signature experiment requires the bounded failed-hook inventory'
            from audit_feature_patterns import event_link,executable_sections
            facts=json.loads((ROOT/f'analysis/menu/source-interface-state-{pid}.json').read_text())['factories']
            section_data=executable_sections(process,int(facts['engine.dll']['base'],16))
            link=event_link(process,facts,section_data)
            target=link['expected_target']
            start,data=next((a,d) for a,d in section_data if a<=target<a+len(d))
            adaptations[link['old_pattern']]={
                'range_start':start,'range_end':start+len(data),
                'replacement_pattern':link['replacement_pattern'],'expected_target':target,
                'proof':link,'scope':'OFFLINE only; exact old pattern/mask, current executable range, two public wrappers and unique target'}
        result=probe.probe(process,bytes(image),teb,args.entry,0,fields,
            standard_models=models,step_budget=args.max_steps,
            signature_adaptations=adaptations,
            reject_null_reads=True,observed_exports=observed,
            external_code_ranges=[(m['base'],m['base']+m['size']) for m in modules if Path(m['path']).name.lower() in
                ('ucrtbase.dll','msvcp140.dll','vcruntime140.dll','kernelbase.dll','kernel32.dll','ntdll.dll','inputsystem.dll','engine.dll','client.dll','vstdlib.dll','tier0.dll')])
    result['offline_interface_hypotheses']=bindings
    result['scope']='OFFLINE ONLY candidate startup root with synthetic guard/allocations and exact masked-scan models; real current process page copies. No target calls/writes. Root ABI remains unresolved.'
    (ROOT/f'analysis/menu/feature-startup-{args.entry:06x}-{pid}{'-dry-hooks' if args.dry_hooks else ''}{'-client-panel' if args.bind_client_panel else ''}{'-client-state' if args.bind_client_state else ''}{'-extra'+str(len(args.bind_field)) if args.bind_field else ''}{'-event-model' if args.model_event_registration else ''}{'-event-pattern' if args.adapt_event_signature else ''}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:result.get(key) for key in ['instructions','stop_reason','last_eip_rva','synthetic_field_reads','final_stack_words','emulator_error']},indent=2))

if __name__=='__main__':main()
