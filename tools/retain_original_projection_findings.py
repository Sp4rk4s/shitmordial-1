"""Retain the verified native projection contract in the existing IDA target."""
import hashlib
import json
from prepare_menu_preview import ROOT, ida


def main():
    digest = hashlib.sha256((ROOT / 'primordial.bin').read_bytes()).hexdigest()
    metadata = ida('get_metadata')
    if metadata['sha256'] != digest or metadata['base'] != '0x44000000':
        raise RuntimeError('IDA input identity/base differs')
    address = '0x44362810'
    before = ida('get_function_by_address', address=address)
    if before['address'].lower() != address or int(before['size'], 16) != 0x95:
        raise RuntimeError('Native projection function boundary differs')
    name = before['name']
    if name.startswith('sub_'):
        name = 'original_project_world_to_screen'
        ida('rename_function', function_address=address, new_name=name)
    prototype = f'void *__cdecl {name}(void *screen_xy_valid, const float *world_xyz);'
    ida('set_function_prototype', function_address=address, prototype=prototype)
    note = (
        'Verified original RVA362810, size95, cdecl two pointer arguments/plainRET; '
        'returns output pointer. Calls interface4DE184 virtual13(world,float[3]); '
        'nonzero status or x/y exactly0 or screen width/height8087AC/B0 clears '
        'output byte+8 and preserves coordinates. Otherwise truncates floats to '
        'int at+0/+4, sets byte+8=1; not a general viewport-bounds check. '
        'NaN passes original parity branches and cvttss2si yields INT_MIN. '
        'Current3984 VDebugOverlay004 factory engine+10C4F0 returns singleton; '
        'slot13 engine+10C380 RET8 forwards to +109E70. Native F06C0 consumes '
        'this helper for original ESP screen boxes. 13 isolated x86 cases verify '
        'ABI/status/edge/truncation/NaN behavior with unchanged helper bytes; '
        'Source projection fixture is test-only. ESP player cache/bootstrap is '
        'unresolved; no ESP drawing enabled. Evidence visual-renderer-dependencies-ida.json.'
    )
    assembly = ida('disassemble_function', start_address=address)
    entry = assembly['lines'][0]
    existing = '\n'.join(entry.get('comments', []))
    target = address if not existing or note in existing else assembly['lines'][1]['address']
    line = next(x for x in assembly['lines'] if x['address'] == target)
    previous = '\n'.join(line.get('comments', []))
    comment = previous if note in previous else (previous + '\n\n' + note).strip()
    if len(comment) >= 1024:
        raise RuntimeError('Comment exceeds legacy readback bound')
    ida('set_comment', address=target, comment=comment)
    if note not in json.dumps(ida('disassemble_function', start_address=address)):
        raise RuntimeError('IDA annotation readback differs')
    path = ROOT / 'analysis/ida/project-index.json'
    index = json.loads(path.read_text())
    row = next((x for x in index['functions'] if int(x['address'], 16) == int(address, 16)), None)
    if row is None:
        row = dict(address=address, aliases=[before['name']])
        index['functions'].append(row)
    row.update(name=name, prototype=prototype)
    if target == address:
        row['comment'] = comment
    else:
        notes = index.setdefault('data_comments', [])
        item = next((x for x in notes if int(x['address'], 16) == int(target, 16)), None)
        if item is None:
            notes.append(dict(address=target, comment=comment))
        else:
            item['comment'] = comment
    path.write_text(json.dumps(index, indent=2) + '\n')
    receipt = dict(binary_sha256=digest, address=address, name=name,
                   comment_address=target, readback_verified=True,
                   disk_save_verified=False)
    (ROOT / 'analysis/menu/original-projection-ida-applied.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Original projection named, typed, commented, read back and indexed; disk save unverified.')


if __name__ == '__main__':
    main()
