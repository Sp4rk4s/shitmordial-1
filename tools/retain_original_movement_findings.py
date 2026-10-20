"""Retain verified original movement ABI/dependencies in the user's live IDA."""
import hashlib
import json
from prepare_menu_preview import ROOT,ida

RECORDS=[
    (0xF92D0,'original_autostrafe','void __cdecl {name}(void);',
     'Verified original autostrafer, no stack args/plainRET, size49D. Checkbox53D360, water<2, no IN_SPEED, inactive slow-walk bind53D700 and airborne flag gate. A06C0 queries local entity; captured Source operand locations2D9CB32C/2D8F778B/2D8F1D4F supply water/flags/velocity offsets. Original math writes command forward/side and ring3748D0 record+14/+18/+1C; toggles7DBBDD. Smoothing53D498 affects angle correction. Native core later appliesF7A50 to convert saved desired-angle basis into actual command movement. Production capsule preserves native decisions/math/callees and repairs only those three verified operand cells plus rel32 call locations. Isolated original_movement_tests exercises real original local lookup, bind getter and correction with public Source interface fixtures. Actual gameplay behavior still requires user validation.'),
    (0x369540,'lookup_ground_entity','void *__thiscall {name}(void *entity);',
     'Verified native leaf21bytes/plainRET. ECX=entity; reads field-offset DWORD from captured old-client2D8F003C, then entity+offset as handle, calls ClientEntityList003 slot4 through global4DE170 with that handle and returns its result. Does not validate null interface/method. Offset identity is m_hGroundEntity: current CCSPlayer RecvTable resolves150 and current interface slot4 is handle lookup. Fast-stop blockF8713 calls this before applying braking. Production capsule changes only old operand location to owned verified offset cell; it still calls the real current public interface. Isolated tests cover valid ground handle and no-ground result; no synthetic entity/model used live.'),
    (0x33AE00,'keybind_is_active','unsigned char __thiscall {name}(void *control);',
     'Verified native control getter/plainRET, ECX=keybind control, returns AL. Key field+130, mode+134, one-shot flag+154. If one-shot and key nonzero and32C360 false, returns input_button_pressed_this_frame. Otherwise modes0..4 select table33AED0: latched state helper3580D0, held input state, inverse-held input state, always true, always false. Hold/inverse-held use input_button_state_pointer and32C360 suppression; zero key returns false. Table and full getter bytes SHA256 guarded in original movement backend. Autostrafer queries this via bind53D700 slot8; duck prefix uses bind5192B0. Test fixtures exercise actual always/never branches; current original menu supplies real key state live. Wider input subsystem meaning remains in existing input annotations.'),
]


def main():
    assert hashlib.sha256((ROOT/'primordial.bin').read_bytes()).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    path=ROOT/'analysis/ida/project-index.json';index=json.loads(path.read_text());receipts=[]
    for rva,wanted,template,note in RECORDS:
        address=hex(0x44000000+rva);before=ida('get_function_by_address',address=address);name=before['name']
        if name.startswith('sub_'):
            ida('rename_function',function_address=address,new_name=wanted);name=wanted
        prototype=template.format(name=name)
        ida('set_function_prototype',function_address=address,prototype=prototype)
        asm=ida('disassemble_function',start_address=address)
        first=next(x for x in asm['lines'] if int(x['address'],16)==int(address,16))
        existing='\n'.join(first.get('comments',[]))
        # Preserve earlier meaningful entry comments; put this bounded note at
        # the next instruction when an entry annotation already exists.
        target=address if not existing or note in existing else asm['lines'][1]['address']
        line=next(x for x in asm['lines'] if x['address']==target)
        previous='\n'.join(line.get('comments',[]))
        comment=previous if note in previous else (previous+'\n\n'+note).strip()
        assert len(comment)<1024
        ida('set_comment',address=target,comment=comment)
        assert note in json.dumps(ida('disassemble_function',start_address=address))
        row=next((x for x in index['functions'] if int(x['address'],16)==int(address,16)),None)
        if row is None:row=dict(address=address,aliases=[before['name']]);index['functions'].append(row)
        row.update(name=name,prototype=prototype)
        if target==address:row['comment']=comment
        else:
            item=next((x for x in index['data_comments'] if int(x['address'],16)==int(target,16)),None)
            if item is None:index['data_comments'].append(dict(address=target,comment=comment))
            else:item['comment']=comment
        receipts.append(dict(address=address,comment_address=target,name=name,prototype=prototype,comment=comment,readback_verified=True))
    path.write_text(json.dumps(index,indent=2)+'\n')
    (ROOT/'analysis/menu/original-gameplay-ida-applied.json').write_text(json.dumps(dict(scope='Live annotations read back/indexed; latest disk save unverified',records=receipts),indent=2)+'\n')
    print('Original autostrafer, ground lookup and native bind getter retained/read back/indexed.')


if __name__=='__main__':main()
