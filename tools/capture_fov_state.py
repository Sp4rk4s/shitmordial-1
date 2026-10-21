"""Read-only diagnostics for original render/viewmodel FOV callbacks."""
import argparse
import ctypes as c
import json
from pathlib import Path
import struct
from capture_movement_state import exports
from test_menu import Process,ROOT,one,psapi,w


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pid',type=int);args=parser.parse_args()
    pid=args.pid or one('csgo.exe')
    with Process(pid) as p:
        array=(c.c_void_p*4096)();needed=w.DWORD()
        if not psapi.EnumProcessModulesEx(p.handle,array,c.sizeof(array),c.byref(needed),3) or needed.value>c.sizeof(array):raise RuntimeError('Module bounds')
        modules={}
        for base in array[:needed.value//c.sizeof(c.c_void_p)]:
            name=c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(p.handle,base,name,len(name)):raise c.WinError(c.get_last_error())
            modules[Path(name.value).name.lower()]=(base,name.value)
        base,path=modules.get('fov_probe.dll',modules['primordial_loader.dll']);symbols=exports(p,base)
        result=dict(pid=pid,module=path,scope='Read-only non-atomic native callback diagnostics; visual confirmation requires user test')
        for suffix in ['state','views','models','render_before','render_after','model_before','model_after']:
            address=symbols.get('primordial_fov_'+suffix)
            result[suffix]=struct.unpack('<f' if 'before' in suffix or 'after' in suffix else '<i',p.read(address,4))[0] if address else None
        result['sliders']={name:struct.unpack('<f',p.read(0x44340000+rva,4))[0] for name,rva in [('render_fov_add',0x531EF8),('viewmodel_fov_add',0x532160)]}
        result['menu_shown']=bool(p.read(0x447F3449,1)[0])
        address=p.menu_state_address();result['menu_state']=struct.unpack('<I',p.read(address,4))[0] if address else None
    (ROOT/f'analysis/menu/fov-live-{pid}.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
