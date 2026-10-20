"""Read-only feature prerequisite report; does not initialize or enable features."""
import hashlib,json,struct
from pathlib import Path
from test_menu import Process,one,ROOT

def main():
    pid=one('csgo.exe')
    trace=ROOT/f'analysis/menu/feature-startup-1564b0-{pid}-dry-hooks-client-panel-client-state-extra3-event-model.json'
    model=json.loads(trace.read_text());assert model['stop_reason']=='returned to caller sentinel with balanced cdecl stack'
    raw=(ROOT/'primordial.bin').read_bytes();assert hashlib.sha256(raw).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    ranges=json.loads((ROOT/f'analysis/menu/render-state-{pid}.json').read_text())['modules']
    hooks=[]
    with Process(pid) as process:
        for call in model['modeled_standard_calls']:
            if call['name']!='hypothetical_failed_inline_hook_request':continue
            target=int(call['target'],16);replacement=int(call['replacement'],16);original=int(call['original_out'],16)
            owner=next((m for m in ranges if m['base']<=target<m['base']+m['size']),None)
            hook=dict(target=hex(target),detour_rva=hex(replacement-0x44340000),original_slot=hex(original),live_original_value=hex(struct.unpack('<I',process.read(original,4))[0]),target_module=None if owner is None else Path(owner['path']).name,target_rva=None if owner is None else hex(target-owner['base']))
            hook['zero_encoded_pointer_inputs']=[r for r in call.get('preceding_image_reads',[]) if 0x44B47300<=int(r['address'],16)<0x44B47E80 and r['value']=='0x0'][-4:]
            if target:hook['current_target_prefix']=process.read(target,24).hex()
            hooks.append(hook)
        module_inputs=[]
        names=('shaderapidx9.dll','gameoverlayrenderer.dll','vstdlib.dll','vguimatsurface.dll','client.dll','server.dll','engine.dll','materialsystem.dll','tier0.dll','studiorender.dll','inputsystem.dll','vgui2.dll','panorama.dll','localize.dll')
        for i,name in enumerate(names):
            rva=0x4DDF28+40*i;captured=struct.unpack_from('<II',raw,rva);current=struct.unpack('<II',process.read(0x44340000+rva,8))
            module_inputs.append(dict(name=name,rva=hex(rva),captured_hash=hex(captured[0]),captured_module_base=hex(captured[1]),live_hash=hex(current[0]),live_handle=hex(current[1])))
        context={hex(rva):hex(struct.unpack('<I',process.read(0x44340000+rva,4))[0]) for rva in [0x4DE158,0x4DE168,0x4DE16C,0x4DE188,0x4DE198,0x4DE1AC,0x4DE1FC]}
        menu=struct.unpack('<I',process.read(process.menu_state_address(),4))[0]
    result=dict(scope='Read-only current prerequisites plus explicit OFFLINE hypothetical hook inventory; no hooks installed and no gameplay success claimed',pid=pid,menu_state=menu,feature_testing_ready=False,offline_instructions=model['instructions'],offline_contract='CPU guard/allocator models; all inline requests modeled as failed allocations; event registration modeled as hypothetical success; interface field roles bound only in emulator',hook_requests=hooks,unresolved_targets=sum(int(x['target'],16)==0 for x in hooks),live_context_fields=context,module_inputs=module_inputs,trace=str(trace),remaining=['Recover seven missing hook targets with current-build evidence','Recover original core gameplay hook installation and complete context/offset initialization','Verify ABI/lifetime before invoking callbacks; user will test actual feature behavior'])
    out=ROOT/f'analysis/menu/feature-readiness-{pid}.json';out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(report=str(out),pid=pid,menu_state=menu,hook_requests=len(hooks),unresolved_targets=result['unresolved_targets'],features_ready=False)))
if __name__=='__main__':main()
