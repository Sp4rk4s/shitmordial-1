"""Retain original cursor-hook findings in the existing IDA; no game writes."""
import hashlib
import json

from prepare_menu_preview import ROOT, ida


RECORDS = [
    (0x3582B0, 0x3582B6, 'menu_input_capture_active',
     'unsigned char __cdecl menu_input_capture_active(void);',
     'Complete six-byte native getter: MOV AL,[runtime44B4869B]; RET. No arguments; '
     'returns original menu input-capture byte RVA80869B, high EAX bits unspecified. '
     'Original surface LockCursor hook154730 calls this and chooses UnlockCursor '
     'when nonzero. Current menu coordinator supplies this flag from original '
     'UI state804DF4. This getter does not initialize input or a renderer.'),
    (0x155110, 0x155116, 'surface_unlock_cursor_forward',
     'void __fastcall surface_unlock_cursor_forward(void *surface, void *preserved_edx);',
     'Complete six-byte native tail thunk: JMP DWORD PTR[runtime44B3B87C]. '
     'No stack arguments; passes incoming ECX/EDX unchanged to saved Source '
     'UnlockCursor. OFFLINE1564B0 inventory pairs this detour with the current '
     'VGUI_Surface031 slot66. No additional cursor behavior. Saved callback '
     'must be bound before execution; no live hook applied during this finding.'),
    (0x154730, 0x154766, 'original_surface_lock_cursor',
     'void __fastcall original_surface_lock_cursor(void *surface, void *preserved_edx);',
     'Complete54-byte original LockCursor replacement. Saves incoming ECX/EDX, '
     'calls menu_input_capture_active3582B0. When AL!=0 tail-calls virtual66 '
     '(+108h) of surface global4DE15C: UnlockCursor. Otherwise restores ECX/EDX '
     'and tail-calls saved LockCursor at runtime44B3B878. No stack arguments; '
     'ESI/EDI preserved. Native null branches still lead to an invalid call, '
     'so a valid surface/method is required. OFFLINE1564B0 inventory targets '
     'current VGUI_Surface031 slot67; current slots66/67 independently inspected. '
     'Legacy IDA lacks this function definition; guarded replay pending. '
     'No full startup or live hook success is implied.'),
]


def main():
    raw = (ROOT / 'primordial.bin').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    path = ROOT / 'analysis/ida/project-index.json'
    index = json.loads(path.read_text())
    metadata = ida('get_metadata')
    assert digest == index['binary_sha256'] == metadata['sha256']
    assert int(metadata['base'], 16) == 0x44000000
    receipts = []
    for rva, end, name, prototype, comment in RECORDS:
        address = hex(0x44000000 + rva)
        row = dict(address=address, end_address=hex(0x44000000 + end),
                   name=name, prototype=prototype, comment=comment,
                   byte_sha256=hashlib.sha256(raw[rva:end]).hexdigest())
        try:
            before = ida('get_function_by_address', address=address)
        except RuntimeError as error:
            if 'No function found at address' not in str(error):
                raise
            row['expected_bytes'] = raw[rva:end].hex()
            index['pending_functions'] = [r for r in index['pending_functions']
                if int(r['address'], 16) != int(address, 16)] + [row]
            # Preserve entry comments by placing this newly verified note on
            # instruction+1; original entry is not an existing IDA function.
            note_address = hex(int(address, 16) + 1)
            existing = next((r for r in index['data_comments']
                            if int(r['address'], 16) == int(note_address, 16)), None)
            merged = (existing['comment'] + '\n\n' + comment) if existing and comment not in existing['comment'] else comment
            response = ida('set_comment', address=note_address, comment=merged)
            index['data_comments'] = [r for r in index['data_comments']
                if int(r['address'], 16) != int(note_address, 16)] + [dict(address=note_address, comment=merged)]
            receipts.append(dict(record=row, status='address comment submitted; definition/name/prototype pending guarded replay', response=response))
            continue
        old = before['name']
        if old.startswith('sub_'):
            ida('rename_function', function_address=address, new_name=name)
        elif old != name:
            raise RuntimeError('Preserving unexpected analyst name: ' + old)
        ida('set_function_prototype', function_address=address, prototype=prototype)
        assembly = ida('disassemble_function', start_address=address)
        assert int(assembly['start_ea'], 16) == int(address, 16)
        first = assembly['lines'][0]
        previous = '\n'.join(first.get('comments', []))
        merged = previous if comment in previous else (previous + '\n\n' + comment).strip()
        assert len(merged) < 1024
        ida('set_comment', address=address, comment=merged)
        after = ida('disassemble_function', start_address=address)
        assert comment in json.dumps(after)
        row.update(comment=merged, aliases=[old])
        index['functions'] = [r for r in index['functions']
            if int(r['address'], 16) != int(address, 16)] + [row]
        receipts.append(dict(record=row, status='name/prototype/comment applied and comment read back'))
    path.write_text(json.dumps(index, indent=2) + '\n')
    (ROOT / 'analysis/menu/native-cursor-ida-applied.json').write_text(json.dumps(
        dict(binary_sha256=digest, database_disk_save_verified=False,
             records=receipts), indent=2) + '\n')
    print('Cursor findings retained; undefined LockCursor function queued for guarded replay. Disk save unverified.')


if __name__ == '__main__':
    main()
