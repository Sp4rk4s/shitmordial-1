"""Retain verified original team/observer dependency contracts in live IDA."""
import hashlib
import json
from prepare_menu_preview import ROOT, ida

RECORDS = [
    (0x36B2B0, 'original_entities_are_enemies',
     'bool __thiscall {name}(void *entity, void *other);',
     'Verified complete10C-byte original comparator, ECX=entity, one stack other, RET4/AL. '
     'NULL other or equal networkable indices return false. Networkable9 dormant/10 index; '
     'dormant teams read resource POINTER SLOT4DE1B0 with original operand2DB22972. '
     'Ordinary teams operand2D92D648. Mode4DE310==6 &&4DE314==0 uses survival operand2DB5D56F; '
     'negative survival team returns true. Otherwise4DE318 or differing teams select result. '
     'Current RecvTables independently identify resourceTeamF38/teamF4/survival10410; '
     'original354530 mode hashes identify actual game_type/game_mode/mp_teammates_are_enemies. '
     'original_team capsule executes native instructions; only5 operand cells repaired. '
     '23 native comparator/observer/indirection/restoration cases pass. No live chams hook enabled.'),
    (0x36ACC0, 'original_entity_is_enemy_to_local_or_observed',
     'bool __thiscall {name}(void *entity, bool direct_local);',
     'Verified complete13D-byte original observer-aware gate, ECX=entity, stack bool direct_local, RET4/AL. '
     'Returns false if originalA06C0 local lookup or entity NULL. direct_local compares local directly '
     'through36B2B0; otherwise original36ABF0 alive or observer modes0/1/2/3/6 or invalid/missing '
     'target use local; remaining modes resolve actual observer handle via EntityList4 and compare '
     'observed entity. Old observer operand cells2DA9DC0C/2DAF0360 repaired to current RecvTable '
     '3388/339C. Capsule executes originalA06C0,36ABF0,36B2B0 dependencies, repairing verified '
     'operand cells/rel32 only and scopes/restores interfaces/resource/local/modes. '
     '23 isolated native cases pass, including modes4/5 and dormant resource fallback. '
     'Original132FF0 chams uses this gate; full chams runtime remains unresolved.'),
]


def main():
    path = ROOT / 'analysis/ida/project-index.json'
    index = json.loads(path.read_text())
    metadata = ida('get_metadata')
    assert metadata['sha256'] == index['binary_sha256'] == hashlib.sha256((ROOT / 'primordial.bin').read_bytes()).hexdigest()
    assert int(metadata['base'], 16) == 0x44000000
    receipts = []
    for rva, wanted, template, note in RECORDS:
        address = hex(0x44000000 + rva)
        before = ida('get_function_by_address', address=address)
        assert int(before['address'], 16) == int(address, 16)
        name = before['name']
        if name.startswith('sub_'):
            ida('rename_function', function_address=address, new_name=wanted)
            name = wanted
        prototype = template.format(name=name)
        ida('set_function_prototype', function_address=address, prototype=prototype)
        asm = ida('disassemble_function', start_address=address)
        target = next(x for x in asm['lines'][:10] if not x.get('comments') or note in '\n'.join(x.get('comments', [])))['address']
        assert len(note) < 1024
        ida('set_comment', address=target, comment=note)
        assert note in json.dumps(ida('disassemble_function', start_address=address))
        row = next((r for r in index['functions'] if int(r['address'], 16) == int(address, 16)), None)
        if row is None:
            row = dict(address=address, aliases=[before['name']])
            index['functions'].append(row)
        row.update(name=name, prototype=prototype)
        if target == address:
            row['comment'] = note
        else:
            notes = index.setdefault('data_comments', [])
            notes[:] = [r for r in notes if int(r['address'], 16) != int(target, 16)]
            notes.append(dict(address=target, comment=note))
        receipts.append(dict(address=address, name=name, prototype=prototype, comment_address=target, readback_verified=True))
    path.write_text(json.dumps(index, indent=2) + '\n')
    (ROOT / 'analysis/menu/original-team-ida-applied.json').write_text(json.dumps(dict(database_disk_save_verified=False, records=receipts), indent=2) + '\n')
    print('Original team and observer helpers named/typed/commented/read back/indexed; disk save unverified.')


if __name__ == '__main__':
    main()
