"""Read-only native menu frame evidence; no injection or target calls."""
import hashlib
import json
import struct
from capture_render_state import snapshot
from test_menu import Process, ROOT, one


def main():
    pid=one('csgo.exe')
    if not pid:raise SystemExit('No csgo.exe process')
    modules=snapshot(pid)['modules']
    with Process(pid) as process:
        def word(rva):return struct.unpack('<I',process.read(0x44340000+rva,4))[0]
        def path(rva):
            data=process.read(0x44340000+rva,24)
            size,capacity=struct.unpack_from('<II',data,16)
            if not 0<=size<=capacity<=65536:raise RuntimeError('Unexpected path bounds')
            pointer=word(rva) if capacity>=8 else 0x44340000+rva
            try:text=process.read(pointer,size*2).decode('utf-16-le')
            except OSError:text=None
            return dict(rva=hex(rva),pointer=hex(pointer),size=size,capacity=capacity,text=text)
        source=(ROOT/'primordial.bin').read_bytes()
        frame=process.read(0x4466BA40,0x6E5)
        assert hashlib.sha256(source).hexdigest()=='e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
        loader=next((m for m in modules if m['path'].lower().endswith('primordial_loader.dll')),None)
        state_address=process.menu_state_address()
        result=dict(pid=pid,scope='Read-only non-atomic evidence; visual confirmation is a separate user test',
            loader=loader['path'] if loader else None,
            loader_sha256=hashlib.sha256(open(loader['path'],'rb').read()).hexdigest() if loader else None,
            handoff=process.handoff(),menu_state=struct.unpack('<I',process.read(state_address,4))[0] if state_address else None,
            original_frame_bytes_unchanged=frame==source[0x32BA40:0x32C125],
            config_directory=path(0x4B3D14),script_directory=path(0x4B3C74),
            periodic_path_label=path(0x4B3C14),
            centered=bool(process.read(0x44B44DF0,1)[0]),
            periodic_refresh_time=struct.unpack('<f',process.read(0x44B44E08,4))[0],
            cursor_choice=word(0x804E10),capture=bool(process.read(0x44B4869B,1)[0]),
            surface_lock_original_slot=hex(word(0x7FB878)),
            material_system=hex(word(0x4DE19C)))
    (ROOT/f'analysis/menu/native-frame-live-{pid}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
