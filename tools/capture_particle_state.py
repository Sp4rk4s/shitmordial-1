"""Read-only original particle hook receipt; effect confirmation is a user test."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
import struct
from capture_movement_state import exports
from test_menu import Process, ROOT, one, psapi, w


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int)
    args = parser.parse_args()
    pid = args.pid or one('csgo.exe')
    if not pid:
        raise SystemExit('No csgo.exe process')
    raw = (ROOT / 'primordial.bin').read_bytes()
    with Process(pid) as process:
        array = (c.c_void_p * 4096)()
        needed = w.DWORD()
        if not psapi.EnumProcessModulesEx(process.handle, array, c.sizeof(array), c.byref(needed), 3) or needed.value > c.sizeof(array):
            raise RuntimeError('Module bounds')
        modules = {}
        for base in array[:needed.value // c.sizeof(c.c_void_p)]:
            name = c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(process.handle, base, name, len(name)):
                raise c.WinError(c.get_last_error())
            modules[Path(name.value).name.lower()] = (base, name.value)
        loader, path = modules['primordial_loader.dll']
        client, client_path = modules['client.dll']
        symbols = exports(process, loader)
        def word(address):
            return struct.unpack('<I', process.read(address, 4))[0]
        rows = []
        for rva, size, target, slot in [(0x155F40, 0x307, 0x9597A0, 0x7FB8F8), (0x155C30, 0x30A, 0x959B30, 0x7FB8FC)]:
            prefix = process.read(client + target, 5)
            destination = client + target + 5 + struct.unpack_from('<i', prefix, 1)[0] if prefix[0] == 0xE9 else None
            rows.append(dict(callback_rva=hex(rva), callback_bytes_unchanged=process.read(0x44340000 + rva, size) == raw[rva:rva + size],
                             target=hex(client + target), target_prefix=prefix.hex(),
                             target_jump_destination=hex(destination) if destination else None,
                             jumps_to_original_callback=destination == 0x44340000 + rva,
                             saved_source_trampoline=hex(word(0x44340000 + slot))))
        state = symbols.get('primordial_particle_state')
        menu = process.menu_state_address()
        result = dict(pid=pid, scope='Read-only non-atomic installation receipt; user confirmation supplies visible effect evidence',
                      loader=path, loader_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                      client_sha256=hashlib.sha256(Path(client_path).read_bytes()).hexdigest(),
                      state=word(state) if state else None, menu_state=word(menu) if menu else None,
                      hooks=rows, modulate_fire=bool(process.read(0x448790C8, 1)[0]),
                      smoke_mode=word(0x44877420),
                      fire_rgb=list(process.read(0x44879328, 3)), smoke_rgb=list(process.read(0x448776A8, 3)),
                      mouse_lock_original_slot=hex(word(0x44B3B878)))
    output = ROOT / f'analysis/menu/particle-live-{pid}.json'
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
