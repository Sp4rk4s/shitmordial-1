"""Read-only UI thread, named exports and keyboard-conversion table snapshot."""
import ctypes as c
from ctypes import wintypes as w
import json
import struct
from pathlib import Path
from test_menu import Process,one,ROOT,kernel,psapi
from capture_render_state import ModuleInfo

def main():
    pid=one('csgo.exe')
    user=c.WinDLL('user32');native=c.WinDLL('ntdll')
    callback=c.WINFUNCTYPE(w.BOOL,w.HWND,c.c_void_p)
    user.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
    found=[]
    @callback
    def visit(window,_):
        owner=w.DWORD();tid=user.GetWindowThreadProcessId(window,c.byref(owner))
        title=c.create_unicode_buffer(512);user.GetWindowTextW(window,title,512)
        if owner.value==pid and title.value.startswith('Counter-Strike: Global Offensive'):found.append((window,tid))
        return True
    user.EnumWindows(visit,0)
    if len(found)!=1:raise RuntimeError(('game window ambiguity',found))
    window,tid=found[0]
    kernel.OpenThread.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenThread.restype=w.HANDLE
    handle=kernel.OpenThread(0x40,False,tid)
    if not handle:raise c.WinError(c.get_last_error())
    native.NtQueryInformationThread.argtypes=[w.HANDLE,w.ULONG,c.c_void_p,w.ULONG,c.POINTER(w.ULONG)]
    basic=c.create_string_buffer(48);used=w.ULONG()
    try:
        status=native.NtQueryInformationThread(handle,0,basic,48,c.byref(used))
        if status<0:raise RuntimeError(('ThreadBasicInformation',hex(status&0xFFFFFFFF)))
    finally:kernel.CloseHandle(handle)
    native_teb=struct.unpack_from('<Q',basic.raw,8)[0]
    modules=[];exports=[]
    with Process(pid) as process:
        offset=struct.unpack('<i',process.read(native_teb+0x180C,4))[0]
        candidates=[native_teb+offset,native_teb+0x2000,native_teb-0x2000]
        teb=None
        for address in candidates:
            if not 0<address<0x100000000:continue
            try:
                data=process.read(address,0x40)
                if struct.unpack_from('<I',data,0x18)[0]==address and struct.unpack_from('<II',data,0x20)==(pid,tid):teb=address;break
            except OSError:pass
        if teb is None:raise RuntimeError('No verified x86 TEB for the UI thread')
        class Gui(c.Structure):
            _fields_=[('size',w.DWORD),('flags',w.DWORD),('active',w.HWND),('focus',w.HWND),('capture',w.HWND),('menu',w.HWND),('move',w.HWND),('caret',w.HWND),('rect',w.RECT)]
        gui=Gui();gui.size=c.sizeof(gui)
        user.GetGUIThreadInfo.argtypes=[w.DWORD,c.POINTER(Gui)]
        user.GetForegroundWindow.restype=w.HWND
        if not user.GetGUIThreadInfo(tid,c.byref(gui)):raise c.WinError(c.get_last_error())
        thread=dict(tid=tid,teb=hex(teb),native_teb=hex(native_teb),window=hex(window),
            observed_active=hex(gui.active or 0),observed_focus=hex(gui.focus or 0),observed_foreground=hex(user.GetForegroundWindow() or 0))
        (ROOT/f'analysis/menu/threads-{pid}.json').write_text(json.dumps(dict(scope='Read-only current UI thread; TEB self/client IDs checked',threads=[thread]),indent=2)+'\n')
        psapi.GetModuleInformation.argtypes=[w.HANDLE,c.c_void_p,c.POINTER(ModuleInfo),w.DWORD]
        array=(c.c_void_p*4096)();needed=w.DWORD()
        if not psapi.EnumProcessModulesEx(process.handle,array,c.sizeof(array),c.byref(needed),3):raise c.WinError(c.get_last_error())
        for base in array[:needed.value//c.sizeof(c.c_void_p)]:
            path=c.create_unicode_buffer(32768);info=ModuleInfo()
            if not psapi.GetModuleFileNameExW(process.handle,base,path,len(path)):raise c.WinError(c.get_last_error())
            if not psapi.GetModuleInformation(process.handle,base,c.byref(info),c.sizeof(info)):raise c.WinError(c.get_last_error())
            modules.append(dict(base=base,size=info.size,path=path.value))
            if base>0xFFFFFFFF:continue
            header=process.read(base,4096);pe=struct.unpack_from('<I',header,60)[0]
            if header[:2]!=b'MZ' or struct.unpack_from('<H',header,pe+24)[0]!=0x10B:continue
            directory,length=struct.unpack_from('<II',header,pe+24+96)
            if not directory:continue
            nf,nn,af,an,ao=struct.unpack_from('<5I',process.read(base+directory,40),20)
            if not 0<nn<=nf<65536:continue
            functions=struct.unpack('<'+'I'*nf,process.read(base+af,4*nf))
            names=struct.unpack('<'+'I'*nn,process.read(base+an,4*nn));ordinals=struct.unpack('<'+'H'*nn,process.read(base+ao,2*nn))
            for nrva,ordinal in zip(names,ordinals):
                name=process.read(base+nrva,256).split(b'\0')[0].decode('ascii')
                address=functions[ordinal]
                exports.append(dict(module=Path(path.value).name,name=name,address=hex(base+address),forwarded=directory<=address<directory+length))
        obj=struct.unpack('<I',process.read(0x4481E160,4))[0];vtable=struct.unpack('<I',process.read(obj,4))[0]
        getter=struct.unpack('<I',process.read(vtable+46*4,4))[0];code=process.read(getter,17)
        assert code[:9]==bytes.fromhex('558bec8b45088b0485') and code[13:17]==bytes.fromhex('5dc20400')
        table=struct.unpack_from('<I',code,9)[0];mapping=struct.unpack('<107I',process.read(table,107*4))
        assert mapping[11]==0x41 and mapping[72]==0x2D
        (ROOT/f'analysis/menu/source-keyboard-map-{pid}.json').write_text(json.dumps(dict(scope='Actual current read-only ButtonCodeToVirtualKey slot46 getter/table',input=hex(obj),getter=hex(getter),getter_bytes=code.hex(),table=hex(table),virtual_keys=list(mapping)),indent=2)+'\n')
    (ROOT/f'analysis/menu/render-state-{pid}.json').write_text(json.dumps(dict(pid=pid,scope='read-only module ranges for offline UI probe',modules=modules),indent=2)+'\n')
    (ROOT/f'analysis/menu/runtime-standard-exports-{pid}.json').write_text(json.dumps(dict(pid=pid,exports=exports),indent=2)+'\n')
    print(json.dumps(dict(pid=pid,thread=thread,exports=len(exports),keyboard_keys=len(mapping))))

if __name__=='__main__':main()
