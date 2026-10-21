"""Read-only diagnostics for original visual ConVar callbacks."""
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
        base, path = modules.get('visual_probe.dll', modules['primordial_loader.dll'])
        symbols = exports(process, base)
        def word(address):
            return struct.unpack('<I', process.read(address, 4))[0]
        result = dict(pid=pid, module=path, module_base=hex(base),
                      sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                      scope='Read-only non-atomic callback receipt; visible behavior requires user confirmation')
        for key in ['state', 'frames']:
            address = symbols.get('primordial_visual_cvars_' + key)
            result[key] = word(address) if address else None
        result['aspect_slider'] = struct.unpack('<f', process.read(0x44873FA8, 4))[0]
        engine = modules['engine.dll'][0]
        convar = engine + 0x783488
        name = process.read(word(convar + 12), 64).split(b'\0')[0].decode('ascii')
        if name != 'r_aspectratio' or word(convar + 0x1C) != convar:
            raise RuntimeError('Current aspect ConVar identity differs')
        result['aspect_convar'] = dict(name=name, address=hex(convar),
                                      value=struct.unpack('<f', struct.pack('<I', word(convar + 0x2C) ^ convar))[0])
        result['menu_state'] = word(process.menu_state_address())
    (ROOT / f'analysis/menu/visual-cvars-live-{pid}.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
