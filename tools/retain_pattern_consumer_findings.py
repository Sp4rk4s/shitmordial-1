"""Extend existing IDA annotations without renaming partial candidates."""
import hashlib
import json
from prepare_menu_preview import ROOT, ida

notes = {
    0xF8510: '2026-10-20 partial movement coordinator: direct checkbox reads link bunnyhop(53D0E8+138), fast stop(53DAE0+138), infinite duck stamina(53DC20+138). Called at379A58 from native CreateMove-shaped callback3798C0. Bunnyhop decision excerpt F8674..F870E reads old-client operand locations2D9CB32C(water-level offset) and2D8F778B(flags offset), current command global4DE214, bypass byte7DBBDC. New movement-original-x86 test executes this exact excerpt with explicit local offset/entity fixtures and verifies320 decisions against the bounded native bridge. Full coordinator includes weapon/other behaviors and is NOT enabled live; semantics outside excerpt remain partial.',
    0xF92D0: '2026-10-20 partial autostrafe dependency: original autostrafer checkbox53D228+138 gates this function; smoothing float at53D368+130. Reads current command4DE214 and old-client operand locations2D9CB32C(water),2D8F778B(flags),2D8F1D4F(velocity). Computes horizontal speed/angles, writes command movement and per-command state via3748D0. Whole math/state contract remains incomplete. Not called by the new two-feature bridge; autostrafer remains unresolved.',
    0x3798C0: '2026-10-20 partial original CreateMove-shaped callback: three stack args forwarded to original44B65FE0, derives sequence modulo150,100-byte commands and104-byte verified commands from payload CInput field4DE1A8. CallsF8510 at379A58 among many feature roots. Concrete game-build mismatch: original buffer reads Input+F4/F8; installed native CInput CreateMove atclient+305740 uses+F0/F4. Original XOR call at3799CE/D6 resolves stale client function2DBE2740; installed native checksum called atclient+307260 after verified-command copy. Do not install this whole callback from one corrected pattern or the incomplete decompiler prototype (bad stack warning). No original core hook installed live.',
    0x155840: '2026-10-20 pattern follow-up: current compiled event signature is consumed through this 18-byte helper, NOT the ASCII scanner at21E560. ECX points to24 bytes (18 pattern bytes,2 padding,mask DWORD+20); stack args are output DWORD*,start,length. Returns output pointer in EAX and RET12. Original strict excluded-final-position and first-byte behavior retained. Original x86 execution test event-pattern-consumer verifies the adapted pattern with this helper. Production adapter is additionally restricted to native return RVAs155352/155375, exact old signature/mask0, engine executable range, SHA256-guarded helper and installed Source files. It does not invoke1564B0 or enable gameplay.',
    0x155AD0: '2026-10-20 pattern follow-up: current GAMEEVENTSMANAGER002 slots8/9 independently call engine RVA211C70. Old18-byte prefix has0 matches; current18-byte prefix matches this target exactly once in installed executable sections. tools/audit_feature_patterns.py records public wrapper rel32 proof and reference provenance. Scoped source adapter prepared in event_pattern.cpp; NOT loaded in game23896. OFFLINE1564B0 failed-hook inventory now requests this target6C2A1C70 for this exact detour/original slot44B3B8EC, reducing null requests7->6. Callback preprocessing356130/full startup still unresolved; do not equate target recovery with event-hook or gameplay success.',
    0x155B80: '2026-10-20 partial dependency warning: native callback reads DWORD at captured old-client VA2DAA11A5, then writes [ESI+that value]=0 under a local-player/menu-state condition before forwarding original44B3B8C4. Recovering a new function target alone cannot repair this stale Source operand read. No field-offset identity inferred merely from a reference pattern; current field binding remains unresolved.',
    0x36A7C0: '2026-10-20 partial dependency warning: vector-result helper reached from155BD0 reads DWORD operands at captured old-client addresses2D906360,2DAA0FB0 and2D8FBEC0 and uses them as entity-relative scalar/vector offsets. Current Source is a different build/base. These are operand-location dependencies distinct from function-hook targets; do not blindly rebase or substitute offsets without dataflow/field proof. Whole helper semantics remain partial; no new name/prototype asserted.',
}


def main():
    raw=(ROOT/'primordial.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    path=ROOT/'analysis/ida/project-index.json'
    index=json.loads(path.read_text())
    receipts=[]
    address='0x440A06C0'
    before=ida('get_function_by_address',address=address)
    name='refresh_cached_local_player' if before['name'].startswith('sub_') else before['name']
    if before['name']!=name:
        ida('rename_function',function_address=address,new_name=name)
    prototype=f'void *__thiscall {name}(void *context);'
    comment='Verified native helper, no stack arguments/plainRET. ECX=context; calls EngineClient014 slot12 through payload4DE174 for current local index, then ClientEntityList003 slot3 through4DE170 for entity; stores returned entity at context+2C and returns it in EAX. Preserves ESI. Both public interface slots corroborated by current native methods/factory identity. Does not read cached entity instead of querying or create an interface itself. Hundreds of gameplay consumers pass local context4481E210. New bounded movement bridge uses the same verified public lookup directly; does not call this helper or populate guessed full context.'
    ida('set_function_prototype',function_address=address,prototype=prototype)
    ida('set_comment',address=address,comment=comment)
    assert comment in json.dumps(ida('disassemble_function',start_address=address))
    entry=dict(address=address,name=name,prototype=prototype,comment=comment,aliases=[before['name']])
    index['functions']=[row for row in index['functions'] if int(row['address'],16)!=int(address,16)]+[entry]
    for rva,note in notes.items():
        address=hex(0x44000000+rva)
        before=ida('disassemble_function',start_address=address)
        # The installed legacy IDA getter truncates individual comments at1024
        # characters. Keep the full existing entry comment and place the new
        # bounded note on the next instruction so its entire readback is proven.
        note_address=hex(0x44000000+rva+1)
        line=next((line for line in before.get('lines',[]) if int(line['address'],16)==int(note_address,16)),None)
        assert line is not None,'Expected existing IDA function/start annotation'
        existing='\n'.join(line.get('comments',[]))
        comment=existing if note in existing else existing+'\n\n'+note
        assert len(comment)<1024,'Split supplemental notes rather than truncating them'
        ida('set_comment',address=note_address,comment=comment)
        after=ida('disassemble_function',start_address=address)
        assert note in json.dumps(after)
        row=next((row for row in index['data_comments'] if int(row['address'],16)==int(note_address,16)),None)
        if row is None:
            index['data_comments'].append(dict(address=note_address,comment=comment))
        else:
            row['comment']=comment
        receipts.append(dict(address=note_address,function_address=address,name=before['name'],comment=comment,readback_verified=True))
    path.write_text(json.dumps(index,indent=2)+'\n')
    (ROOT/'analysis/menu/pattern-consumer-ida-applied.json').write_text(json.dumps(dict(
        scope='Existing annotations extended/read back/indexed. Latest IDA disk save unverified.',records=receipts),indent=2)+'\n')
    print('Local-player getter retained by name/prototype/comment; seven partial/scanner notes read back and indexed')


if __name__=='__main__':
    main()
