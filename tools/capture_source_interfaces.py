"""Read Source factory registries/singletons without calling target functions."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct

from test_menu import Process, ROOT, one, psapi, w


def u32(data, offset=0):
    return struct.unpack_from('<I', data, offset)[0]


def snapshot(pid, additional_modules=()):
    result = {'pid': pid, 'mode': 'read-only, non-atomic snapshot', 'factories': {}}
    with Process(pid) as process:
        def word(address):
            return u32(process.read(address, 4))

        modules = {}
        array = (c.c_void_p * 4096)()
        needed = w.DWORD()
        if not psapi.EnumProcessModulesEx(process.handle, array, c.sizeof(array), c.byref(needed), 3):
            raise c.WinError(c.get_last_error())
        if needed.value > c.sizeof(array):
            raise RuntimeError('Module buffer exhausted')
        for base in array[:needed.value // c.sizeof(c.c_void_p)]:
            name = c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(process.handle, base, name, len(name)):
                raise c.WinError(c.get_last_error())
            modules[Path(name.value).name.lower()] = (base, name.value)

        for module in dict.fromkeys(('vgui2.dll', 'vguimatsurface.dll', 'engine.dll', 'client.dll', 'inputsystem.dll', *additional_modules)):
            base, path = modules[module]
            header = process.read(base, 4096)
            pe = u32(header, 60)
            if header[:2] != b'MZ' or header[pe:pe + 4] != b'PE\0\0':
                raise RuntimeError('Invalid module PE')
            directory = base + u32(header, pe + 24 + 96)
            export = process.read(directory, 40)
            nf, nn, af, an, ao = struct.unpack_from('<IIIII', export, 20)
            if not 0 < nn <= nf < 65536:
                raise RuntimeError('Unexpected export bounds')
            names = struct.unpack(f'<{nn}I', process.read(base + an, nn * 4))
            ordinals = struct.unpack(f'<{nn}H', process.read(base + ao, nn * 2))
            functions = struct.unpack(f'<{nf}I', process.read(base + af, nf * 4))
            entries = [i for i, rva in enumerate(names)
                       if process.read(base + rva, 16).split(b'\0')[0] == b'CreateInterface']
            if len(entries) != 1:
                raise RuntimeError('CreateInterface export ambiguous')
            address = base + functions[ordinals[entries[0]]]
            wrapper = process.read(address, 9)
            if wrapper[:5] != bytes.fromhex('55 8b ec 5d e9'):
                raise RuntimeError('Unverified CreateInterface wrapper')
            internal = (address + 9 + struct.unpack_from('<i', wrapper, 5)[0]) & 0xFFFFFFFF
            code = process.read(internal, 112)
            if code[:6] != bytes.fromhex('55 8b ec 56 8b 35'):
                raise RuntimeError('Unverified factory registry instruction')
            head_slot = u32(code, 6)
            current = word(head_slot)
            item = {'base': hex(base), 'path': path,
                    'disk_sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                    'export': hex(address), 'wrapper_bytes': wrapper.hex(),
                    'internal': hex(internal), 'internal_bytes': code.hex(),
                    'registry_head_slot': hex(head_slot), 'entries': []}
            result['factories'][module] = item
            seen = set()
            while current:
                if current in seen or len(seen) >= 4096:
                    raise RuntimeError('Registry cycle or bound exhausted')
                seen.add(current)
                creator, name, following = struct.unpack('<III', process.read(current, 12))
                text = process.read(name, 128).split(b'\0')[0].decode('ascii')
                entry = {'record': hex(current), 'name': text, 'create': hex(creator),
                         'next': hex(following)}
                item['entries'].append(entry)
                current = following
                if not text.startswith(('VGUI_Panel', 'VGUI_Surface', 'VEngineVGui',
                                        'VEngineClient0', 'VClient0', 'InputSystemVersion')):
                    continue
                factory_code = process.read(creator, 6)
                if factory_code[0] != 0xB8 or factory_code[5] != 0xC3:
                    entry['limitation'] = 'Factory is not verified MOV EAX,imm32; RET'
                    continue
                singleton = u32(factory_code, 1)
                vtable = word(singleton)
                entry.update(factory_bytes=factory_code.hex(), singleton=hex(singleton),
                             singleton_bytes=process.read(singleton, 80).hex(),
                             vtable=hex(vtable), methods={})
                indexes = ((36, 40, 41, 42) if 'Panel' in text else
                           (0, 1, 2, 3, 4, 5, 8) if text.startswith('VClient0') else
                           (0, 5, 26) if text.startswith('VEngineClient0') else
                           (0, 15, 56) if text.startswith('InputSystemVersion') else (0, 1, 2))
                for index in indexes:
                    function = word(vtable + 4 * index)
                    entry['methods'][str(index)] = {'address': hex(function),
                                                    'bytes': process.read(function, 64).hex()}
                if text == 'VEngineVGui001':
                    method = word(vtable + 4)
                    method_code = process.read(method, 21)
                    if method_code[:7] != bytes.fromhex('55 8b ec ff 75 08 e8'):
                        raise RuntimeError('Unverified EngineVGui GetPanel wrapper')
                    helper = method + 11 + struct.unpack_from('<i', method_code, 7)[0]
                    helper_code = process.read(helper, 144)
                    if helper_code[:5] != bytes.fromhex('55 8b ec 80 3d'):
                        raise RuntimeError('Unverified panel switch flag')
                    jump = helper_code.find(bytes.fromhex('ff 24 85'))
                    if jump < 0:
                        raise RuntimeError('Unverified panel jump table')
                    table = u32(helper_code, jump + 3)
                    branch = word(table + 4 * (3 - 1))
                    branch_code = process.read(branch, 7)
                    if branch_code[:2] != b'\x8b\x41' or branch_code[3:] != b'\x5d\xc2\x04\x00':
                        raise RuntimeError('Unverified PANEL_TOOLS field load')
                    panel = word(singleton + branch_code[2])
                    getter = word(word(panel))
                    getter_code = process.read(getter, 4)
                    if getter_code[:2] != b'\x8b\x41' or getter_code[3] != 0xC3:
                        raise RuntimeError('Unverified panel handle getter')
                    flag_address = u32(helper_code, 5)
                    flag = process.read(flag_address, 1)[0]
                    entry['panel_tools'] = {
                        'method': hex(method), 'method_bytes': method_code.hex(),
                        'helper': hex(helper), 'helper_bytes': helper_code.hex(),
                        'early_return_flag': hex(flag_address), 'flag_value': flag,
                        'switch_branch': hex(branch), 'branch_bytes': branch_code.hex(),
                        'object': hex(panel), 'getter': hex(getter),
                        'getter_bytes': getter_code.hex(),
                        'handle': hex(word(panel + getter_code[2])) if not flag else '0x0'}
        result['payload_fields'] = {hex(a): hex(word(a)) for a in
                                    (0x4481E15C, 0x4481E16C, 0x44B3B884, 0x44B3B888)}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', nargs='?', type=int)
    parser.add_argument('--module', action='append', default=[], help='Additional loaded Source factory registry to read')
    args = parser.parse_args()
    pid = args.pid or one('csgo.exe')
    if not pid:
        raise SystemExit('No csgo.exe process')
    result = snapshot(pid, args.module)
    output = ROOT / f'analysis/menu/source-interface-state-{pid}.json'
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'evidence': str(output), 'payload_fields': result['payload_fields'],
                      'interfaces': {name: [e for e in item['entries'] if 'singleton' in e]
                                     for name, item in result['factories'].items()}}, indent=2))
