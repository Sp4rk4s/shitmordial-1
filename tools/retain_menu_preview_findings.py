"""Retain bounded native findings through existing IDA API; no save claim."""
import hashlib
import json
from prepare_menu_preview import ROOT, ida

def main():
    raw=(ROOT/'primordial.bin').read_bytes()
    pending=ROOT/'analysis/menu/hook-helper-annotations-pending.json'
    document=json.loads(pending.read_text())
    assert hashlib.sha256(raw).hexdigest()==document['binary_sha256']
    additions=[
        (0x357F80,0x357FB7,'input_button_state_pointer',
         'int *__cdecl input_button_state_pointer(int button);',
         'Verified complete native getter: clamps signed button to0..113; returns pointer44B47F08+4*button, caller-cleaned one stackDWORD, ordinaryRET (tail bounds-failure path is unreachable after clamp). Observed poll3582C0 states:0 up,1 just pressed,2 held,3 just released. Read-only getter; no callback/interface access.'),
        (0x358090,0x3580C5,'input_button_pressed_this_frame',
         'bool __cdecl input_button_pressed_this_frame(int button);',
         'Verified complete native predicate: clamps signed button to0..113 and returns AL=(DWORD[44B47F08+4*button]==1), caller-cleaned one stackDWORD, RET. State1 is the just-pressed transition in3582C0. High EAX bits unspecified. Used by native checkbox/tab/button input;107(6B) is left mouse. Preview feeds only observed cursor and left-button transitions and does not invoke callback-dispatch poll3582C0.'),
        (0x358100,0x358143,'input_cursor_inside_rectangle',
         'bool __cdecl input_cursor_inside_rectangle(const int *xy, const int *wh);',
         'Verified complete signed integer rectangle hit test against current cursor44B47F00/+4: x>=xy[0],y>=xy[1],x<xy[0]+wh[0],y<xy[1]+wh[1]. Lower inclusive, upper exclusive; two caller-cleaned pointers, plainRET, ALbool. No allocation, callback or interface access. Signed overflow follows native32bit arithmetic; preview constrains actual geometry.'),
        (0x335F10,0x335F56,'control_notify_value_observers',
         'void __thiscall control_notify_value_observers(void *control);',
         'Verified native observer iteration: ECX=control, vectorbegin/end at+CC/+D0,40-byte records; record+24 callback object must be nonnull, invokes virtual2 with address of local pointer to control. PlainRET/no stackargs; malformed null invokes failure helper. No iteration/callback when begin==end. Native checkbox32EF80 toggles byte+138 then calls this on left-button edge. Interactive preview permits native value input only when this observer vector and attached-child vector+124/+128 are both empty; action callbacks are excluded.') ,
        (0x34B860,0x34B881,'group_set_content_rectangle',
         'void __thiscall group_set_content_rectangle(void *group, const int *rectangle_xywh);',
         'Verified complete native four-DWORD assignment: ECX=group; one stack rectangle pointer; copies x,y,width,height into group+54/+58/+5C/+60, RET4. No allocation, callbacks, guard or interface access. Original tab updater34C9C0 calls it at34CEAE with tab rectangle after removing the130-pixel sidebar. Used by scoped menu-only content-layout probe; input updater remains partial.'),
        (0x3372A0,0x3372B0,'control_set_layout_width',
         'void __thiscall control_set_layout_width(void *control, int width);',
         'Verified complete native setter: ECX=control; copies one stack DWORD into control+B8, RET4. No allocation, callbacks, guard or interface access. Original group layout34A4B0 calls it at34AA0F before invoking child position virtual slot3. High-level control classes remain partial.'),
        (0x34DB30,0x34DB51,'tab_set_content_rectangle',
         'void __thiscall tab_set_content_rectangle(void *tab, const int *rectangle_xywh);',
         'Verified complete native assignment: ECX=tab, one stack pointer to four DWORDs x,y,width,height; stores them at tab+6C/+70/+74/+78, RET4. No allocation, callbacks, interface or guard access. Original window updater3504B0 calls it at351A5F with window+5C. Scoped menu-only layout probe uses this same helper after reproducing the native rectangle stores350DBC..350DFD. Full input updater remains partial and is not called by the preview.'),
        (0x155BD0,0x155C23,'override_vector_result_for_current_object',
         'unsigned int *__thiscall override_vector_result_for_current_object(void *object, unsigned int *out_words3);',
         'Verified native wrapper: ECX=this, one stack output pointer, RET4, EAX=output. First forwards this/output through original44B3B8E4. If this matches object returned by A06C0(4481E210), calls36A7C0 on that object with temporary12-byte output and zero flag, then copies three DWORDs from its return to caller output. Type/feature semantics of those words and identity of current object remain unverified; local-player/vector interpretation is an inference only. Protected1564B0 offline hook setup XORs captured globals44B474F0/F4 (2D01AF9A/00CEFC5A) into stale target2DCF53C0 and associates this wrapper. Current live fields are zero; no valid current target or hook established. Evidence candidate-1564b0-constructor-native-crt-38484-scan11-model-hook-memory-event-signature-captured-pair.json. No live callback applied.'),
        (0x3609A0,0x3609D4,'renderer_clear_clip_storage_and_reset_rect',
         'void __cdecl renderer_clear_clip_storage_and_reset_rect(void);',
         'Verified complete native no-argument helper, plainRET: calls35F650 on runtime44B487BC then ISurface slot147 from4481E15C with x0,y0,width44B487AC,height44B487B0.35F650 releases a nested owned collection including16-byte elements and pointer array (partial container type, not assigned). Requires valid current Surface and established dimensions/owned storage. Called by original panel callback154790. Authorized separate menu-only preview uses this helper inside PushMakeCurrent/PopMakeCurrent; this does not establish original bootstrap or gameplay hooks.')]
    receipts=[]
    indexpath=ROOT/'analysis/ida/project-index.json'
    index=json.loads(indexpath.read_text())
    for start,end,name,prototype,comment in additions:
        address=f'0x{0x44000000+start:08X}'
        entry=dict(address=address,end_address=f'0x{0x44000000+end:08X}',name=name,
            prototype=prototype,comment=comment,aliases=[f'sub_{0x44000000+start:08X}'],
            byte_sha256=hashlib.sha256(raw[start:end]).hexdigest())
        existing=ida('get_function_by_address',address=address)
        old=existing.get('name','')
        if old.startswith('sub_') or old==name:
            ida('rename_function',function_address=address,new_name=name)
        else:
            raise RuntimeError(('preserve analyst name',existing))
        ida('set_function_prototype',function_address=address,prototype=prototype)
        ida('set_comment',address=address,comment=comment)
        readback=ida('disassemble_function',start_address=address)
        assert readback['name']==name
        assert comment in json.dumps(readback)
        receipts.append(dict(entry=entry,readback=readback))
        for obj in (document,index):
            obj['functions']=[x for x in obj['functions'] if x['address'].lower()!=address.lower()]+[entry]
    pending.write_text(json.dumps(document,indent=2)+'\n')
    indexpath.write_text(json.dumps(index,indent=2)+'\n')
    (ROOT/'analysis/menu/menu-preview-native-ida-applied.json').write_text(json.dumps(dict(
        scope='Live names/prototypes/comments read back; newest disk save unverified',functions=receipts),indent=2)+'\n')
    comment='Bounded layout excerpt inside partial window updater3504B0:350DBC..350DFD stores window+5C=x(+3C),+60=y(+40)+titleHeight(+D0),+64=width(+44),+68=height(+48)-titleHeight-(tabvectorD4==D8?8:55). Caller later passes this rectangle to tab_set_content_rectangle34DB30. Live menu-only draw initially skipped updater; constructor leaves these fields undefined on native heap, whereas offline zero-filled allocator hid them. Actual38484 primary fields5C=68EF2782,60=35419685,64=12,68=196648B0 produced a visible title/frame with absent offscreen tabs/content. Scoped layout preview reproduces only these verified stores and settled tab animation values; native input/gameplay callbacks are not invoked. Full updater semantics remain partial.'
    ida('set_comment',address='0x44350DBC',comment=comment)
    ida('set_comment',address='0x4434CE73',comment='Bounded partial tab-updater layout tail34CE73..34CEAE: derives selected group rectangle from tab+6C/+70/+74/+78, x+=130,width-=130; selects group via tab+84 vector of20-byte entries and tab+80 index; calls pure group_set_content_rectangle34B860. Next34CEC8 calls group update34A4B0 forwarding the stack blocked-input flag. Full updater includes mouse/subtab logic and remains partial; scoped preview reproduces only geometry and settled animations.')
    ida('set_comment',address='0x4434A4B0',comment='Partial group input/layout updater, ECX=group, one stack blocked-input flag, RET4 at34AB6C. Geometry segment34A8B0..34AA9E chooses1..4 columns with minimum276-pixel width and16-pixel gutters; assigns each visible child width via3372A0 and position virtual3. Child input virtual1 receives true if caller blocked flag set (34AB00..19). Top scroll handling may enter protected helpers when group+68 nonzero; full runtime contract remains partial. Controlled menu-only probe calls first layout with blocked flagtrue ONLY when initial group+68 is zero, then does not repeat. No gameplay bootstrap invocation implied.')
    print('Three functions and bounded layout excerpt retained; disk save unverified.')

if __name__=='__main__':main()
