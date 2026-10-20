"""Read-only D3D9 device/vtable/detour snapshot of the installed legacy game.

Uses the verified shaderapidx9 GetDeviceCaps call pattern. No target writes,
injection, suspension, or function calls. A missing payload-owned jump is only
a bounded observation, not proof that all render interception is absent.
"""
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import re
import struct

from test_menu import Process, kernel, one, psapi, ROOT


class ModuleInfo(c.Structure):
    _fields_ = [('base', c.c_void_p), ('size', w.DWORD), ('entry', c.c_void_p)]


def snapshot(pid):
    psapi.GetModuleInformation.argtypes = [w.HANDLE, c.c_void_p,
                                          c.POINTER(ModuleInfo), w.DWORD]
    result = {'pid': pid, 'mode': 'read-only, non-atomic process snapshot',
              'modules': [], 'devices': [], 'limitations': [
                  'Only the verified shaderapidx9 device references are checked.',
                  'Only direct E9, EB, FF25 and PUSH/RET jump chains are followed.',
                  'No WndProc or complete render-hook audit is claimed.']}
    with Process(pid) as process:
        array = (c.c_void_p * 4096)()
        needed = w.DWORD()
        if not psapi.EnumProcessModulesEx(process.handle, array, c.sizeof(array), c.byref(needed), 3):
            raise c.WinError(c.get_last_error())
        if needed.value > c.sizeof(array):
            raise RuntimeError('Module enumeration buffer exhausted')
        for base in array[:needed.value // c.sizeof(c.c_void_p)]:
            name = c.create_unicode_buffer(32768)
            info = ModuleInfo()
            if not psapi.GetModuleFileNameExW(process.handle, base, name, len(name)):
                raise c.WinError(c.get_last_error())
            if not psapi.GetModuleInformation(process.handle, base, c.byref(info), c.sizeof(info)):
                raise c.WinError(c.get_last_error())
            result['modules'].append({'base': base, 'size': info.size, 'path': name.value})

        def owner(address):
            if 0x44340000 <= address < 0x45750000:
                return 'staged primordial image'
            return next((m['path'] for m in result['modules']
                         if m['base'] <= address < m['base'] + m['size']), None)

        def chain(address):
            nodes = []
            visited = set()
            while address not in visited and len(nodes) < 12:
                visited.add(address)
                node = {'address': hex(address), 'owner': owner(address)}
                nodes.append(node)
                try:
                    code = process.read(address, 16)
                    node['bytes'] = code.hex()
                    target = None
                    if code[0] == 0xE9:
                        target = (address + 5 + struct.unpack_from('<i', code, 1)[0]) & 0xFFFFFFFF
                    elif code[0] == 0xEB:
                        target = (address + 2 + struct.unpack_from('<b', code, 1)[0]) & 0xFFFFFFFF
                    elif code[:2] == b'\xff\x25':
                        slot = struct.unpack_from('<I', code, 2)[0]
                        node['indirect_slot'] = hex(slot)
                        target = struct.unpack('<I', process.read(slot, 4))[0]
                    elif code[0] == 0x68 and code[5] == 0xC3:
                        target = struct.unpack_from('<I', code, 1)[0]
                    if target is None:
                        node['stop'] = 'no supported leading unconditional jump'
                        break
                    node['target'] = hex(target)
                    address = target
                except OSError as error:
                    node['error'] = str(error)
                    break
            return nodes

        shader = next(m for m in result['modules']
                      if Path(m['path']).name.lower() == 'shaderapidx9.dll')
        header = process.read(shader['base'], 4096)
        pe = struct.unpack_from('<I', header, 60)[0]
        if header[:2] != b'MZ' or header[pe:pe + 4] != b'PE\0\0':
            raise RuntimeError('Invalid shaderapidx9 header')
        count, = struct.unpack_from('<H', header, pe + 6)
        optional_size, = struct.unpack_from('<H', header, pe + 20)
        table = pe + 24 + optional_size
        pattern = re.compile(rb'\xa1(.{4})\x50\x8b\x08\xff\x51\x0c', re.DOTALL)
        for index in range(count):
            offset = table + 40 * index
            if offset + 40 > len(header):
                raise RuntimeError('Section table outside header snapshot')
            size, rva = struct.unpack_from('<II', header, offset + 8)
            flags, = struct.unpack_from('<I', header, offset + 36)
            if not flags & 0x20000000:
                continue
            code = process.read(shader['base'] + rva, size)
            for match in pattern.finditer(code):
                slot = struct.unpack('<I', match.group(1))[0]
                device = struct.unpack('<I', process.read(slot, 4))[0]
                item = {'reference': hex(shader['base'] + rva + match.start()),
                        'slot': hex(slot), 'device': hex(device)}
                result['devices'].append(item)
                if not device:
                    continue
                vtable = struct.unpack('<I', process.read(device, 4))[0]
                item['vtable'] = hex(vtable)
                item['methods'] = {}
                for method, method_index in [('Reset', 16), ('Present', 17), ('EndScene', 42)]:
                    address = struct.unpack('<I', process.read(vtable + 4 * method_index, 4))[0]
                    item['methods'][method] = chain(address)
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', nargs='?', type=int)
    args = parser.parse_args()
    pid = args.pid or one('csgo.exe')
    if not pid:
        raise SystemExit('No csgo.exe process')
    result = snapshot(pid)
    path = ROOT / f'analysis/menu/render-state-{pid}.json'
    path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'evidence': str(path), 'devices': result['devices']}, indent=2))
