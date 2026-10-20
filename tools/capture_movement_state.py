"""Read-only movement bridge diagnostics; never toggles a setting or starts a map."""
import ctypes as c
import json
from pathlib import Path
import struct
from test_menu import Process, ROOT, one, psapi, w


def exports(process, base):
    header=process.read(base,4096)
    pe=struct.unpack_from('<I',header,60)[0]
    if header[:2]!=b'MZ' or not 0<pe<3900 or header[pe:pe+4]!=b'PE\0\0':
        raise RuntimeError('Invalid module PE')
    directory,size=struct.unpack_from('<II',header,pe+24+96)
    if not directory:
        return {}
    nf,nn,af,an,ao=struct.unpack_from('<5I',process.read(base+directory,40),20)
    if not 0<nn<=nf<65536:
        raise RuntimeError('Invalid export bounds')
    names=struct.unpack('<'+'I'*nn,process.read(base+an,nn*4))
    ordinals=struct.unpack('<'+'H'*nn,process.read(base+ao,nn*2))
    functions=struct.unpack('<'+'I'*nf,process.read(base+af,nf*4))
    result={}
    for name,ordinal in zip(names,ordinals):
        if ordinal>=nf:
            raise RuntimeError('Invalid export ordinal')
        text=process.read(base+name,128).split(b'\0')[0].decode('ascii')
        if directory<=functions[ordinal]<directory+size:
            continue
        result[text.lstrip('_')]=base+functions[ordinal]
    return result


def main():
    pid=one('csgo.exe')
    if not pid:
        raise RuntimeError('Legacy CS:GO is not running')
    result=dict(pid=pid,scope='Read-only non-atomic diagnostics; gameplay behavior requires user testing')
    with Process(pid) as process:
        array=(c.c_void_p*4096)();needed=w.DWORD()
        if not psapi.EnumProcessModulesEx(process.handle,array,c.sizeof(array),c.byref(needed),3) or needed.value>c.sizeof(array):
            raise RuntimeError('Module enumeration failed')
        modules={}
        for base in array[:needed.value//c.sizeof(c.c_void_p)]:
            path=c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(process.handle,base,path,len(path)):
                raise c.WinError(c.get_last_error())
            modules[Path(path.value).name.lower()]=(base,path.value)
        name='movement_probe.dll' if 'movement_probe.dll' in modules else 'primordial_loader.dll'
        if name not in modules:
            raise RuntimeError('Loader/feature diagnostic is absent')
        base,path=modules[name];table=exports(process,base)
        for suffix in ('state','commands','changes'):
            symbol='primordial_movement_'+suffix
            address=table.get(symbol)
            result[suffix]=struct.unpack('<i',process.read(address,4))[0] if address else None
        result['module']=path
        address=process.menu_state_address()
        result['menu_state']=struct.unpack('<I',process.read(address,4))[0] if address else None
        result['menu_shown']=bool(process.read(0x447F33B0+0x99,1)[0])
        result['checkboxes']={name:bool(process.read(0x44340000+rva+0x138,1)[0])
                              for name,rva in [('bunnyhop',0x53D0E8),('infinite_duck_stamina',0x53DC20)]}
        result['implemented_features']=['bunnyhop','infinite duck stamina']
        result['other_gameplay_features_ready']=False
    output=ROOT/f'analysis/menu/movement-live-{pid}.json'
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    main()
