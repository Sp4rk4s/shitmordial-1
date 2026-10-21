"""Join retained original hook requests to current, read-only game evidence.

The 1564B0 trace modeled failed installs. Its target RVAs are leads, never an
installation recipe or a claim to have enumerated every protected callback.
"""
import hashlib
import json
from pathlib import Path
import struct

from capture_render_state import snapshot as render_snapshot
from capture_source_interfaces import snapshot as interface_snapshot
from test_menu import Process, ROOT, one


def imports(process, module):
    base = module['base']
    header = process.read(base, 4096)
    pe = struct.unpack_from('<I', header, 60)[0]
    assert header[:2] == b'MZ' and header[pe:pe+4] == b'PE\0\0'
    rva, size = struct.unpack_from('<II', header, pe+24+104)
    if not rva:
        return []
    assert 20 <= size < module['size']
    def string(address):
        return process.read(address, 192).split(b'\0')[0].decode('ascii')
    result = []
    for offset in range(0, size, 20):
        descriptor = struct.unpack('<5I', process.read(base+rva+offset, 20))
        if not any(descriptor):
            return result
        original, _, _, name, first = descriptor
        entry = dict(module=string(base+name), symbols=[], name_table_available=bool(original))
        result.append(entry)
        if not original:
            continue
        for index in range(65536):
            value = struct.unpack('<I', process.read(base+original+4*index, 4))[0]
            if not value:
                break
            symbol = ('ordinal:'+str(value & 0xFFFF)) if value & 0x80000000 else string(base+value+2)
            entry['symbols'].append(dict(name=symbol, iat=hex(base+first+index*4),
                current=hex(struct.unpack('<I', process.read(base+first+index*4, 4))[0])))
        else:
            raise RuntimeError('Import thunk bound exhausted')
    raise RuntimeError('Import descriptor bound exhausted')


def main():
    pid = one('csgo.exe')
    if not pid:
        raise SystemExit('No csgo.exe process')
    raw = (ROOT/'primordial.bin').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    historical = json.loads((ROOT/'analysis/menu/feature-readiness-23896.json').read_text())
    render = render_snapshot(pid)
    interfaces = interface_snapshot(pid)
    by_name = {Path(m['path']).name.lower(): m for m in render['modules']}
    rows = []
    with Process(pid) as process:
        for item in historical['hook_requests']:
            row = dict(detour_rva=item['detour_rva'], original_slot=item['original_slot'],
                origin='OFFLINE1564B0 failed-install model; historical target lead',
                live_original_value=hex(struct.unpack('<I',process.read(int(item['original_slot'],16),4))[0]))
            name, rva = item.get('target_module'), item.get('target_rva')
            if name and rva:
                module = by_name[name.lower()]
                target = module['base']+int(rva,16)
                prefix = process.read(target,24).hex()
                row.update(target_module=name, target_rva=rva, current_target=hex(target),
                    current_prefix=prefix, matches_historical_prefix=prefix==item.get('current_target_prefix'),
                    status='target lead; callback/dependencies require validation')
            else:
                row.update(status='missing target', zero_encoded_pointer_inputs=item.get('zero_encoded_pointer_inputs',[]))
            if int(item['detour_rva'],16) == 0x154730:
                row['role']='original Surface LockCursor; verified capture getter and Surface66 unlock branch'
            elif int(item['detour_rva'],16) == 0x155110:
                row['role']='Surface UnlockCursor forwarding thunk; no additional behavior'
            elif int(item['detour_rva'],16) == 0x156250:
                row['role']='verified return-true/RET4 leaf; target semantics unresolved'
            rows.append(row)
        surface = next(e for e in interfaces['factories']['vguimatsurface.dll']['entries'] if e['name']=='VGUI_Surface031')
        surface_methods = []
        for slot in (66,67):
            target = struct.unpack('<I',process.read(int(surface['vtable'],16)+4*slot,4))[0]
            surface_methods.append(dict(slot=slot,target=hex(target),bytes=process.read(target,48).hex()))
        shader_imports = imports(process, by_name['shaderapidx9.dll'])
        directx_imports = [x for x in shader_imports if 'd3d' in x['module'].lower()]
    result = dict(binary_sha256=digest,pid=pid,mode='read-only, non-atomic',
        coverage='24 requests from one modeled path, plus independently recovered core callbacks; NOT exhaustive protected hook enumeration',
        inline_requests=rows,source_surface_methods=surface_methods,
        independent_core_callbacks=[
            dict(detour_rva='0x154790',role='native Source panel paint',status='full callback has unresolved protected/gameplay dependencies; current adapter drives original window/control routines'),
            dict(detour_rva='0x3798C0',role='original CreateMove-shaped core callback',status='full callback unresolved; original bounded movement routines active through verified ClientMode24 adapter'),
            dict(detour_rva='0x14FF30',role='original camera callback',status='original bounded render-FOV block and original entity gate active through ClientMode18'),
            dict(detour_rva='0x14FE90',role='original viewmodel FOV',status='original callback/helpers active through ClientMode35')],
        directx=render['devices'],shader_directx_imports=directx_imports,
        limitations=['An unchanged prefix or module RVA alone does not establish method role, ABI, or hook readiness.',
            'Saved payload callback slots can be zero while copied original-code adapters are active.',
            'DirectX device methods are vtable callbacks; recovered menu draw routines use Source ISurface.'])
    path=ROOT/f'analysis/menu/hook-inventory-{pid}.json'
    path.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(evidence=str(path),requests=len(rows),missing=sum(r['status']=='missing target' for r in rows),
        shader_directx_imports=directx_imports,coverage=result['coverage']),indent=2))


if __name__=='__main__':main()
