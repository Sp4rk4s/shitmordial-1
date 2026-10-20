"""Detect/launch the verified legacy game, load DLLs, and optionally break at entry.

Examples:
  py -3 tools/test_menu.py status
  py -3 tools/test_menu.py run --launch --debug
No process is killed or restarted automatically. Menu visibility is user-confirmed.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x44340000
HANDOFF = 0x45751000
ENTRY = 0x4470A002
MODULES = ('shaderapidx9.dll', 'gameoverlayrenderer.dll', 'vstdlib.dll',
           'vguimatsurface.dll', 'client.dll', 'server.dll', 'engine.dll',
           'materialsystem.dll', 'tier0.dll', 'studiorender.dll',
           'inputsystem.dll', 'vgui2.dll', 'panorama.dll', 'localize.dll')
kernel = c.WinDLL('kernel32', use_last_error=True)
psapi = c.WinDLL('psapi', use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
psapi.EnumProcessModulesEx.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), w.DWORD]
psapi.GetModuleFileNameExW.argtypes = [w.HANDLE, c.c_void_p, w.LPWSTR, w.DWORD]


def processes(name):
    # Exact process selection, with executable paths retained for review.
    class Entry(c.Structure):
        _fields_ = [('size', w.DWORD), ('usage', w.DWORD), ('pid', w.DWORD),
                    ('heap', c.c_size_t), ('module', w.DWORD), ('threads', w.DWORD),
                    ('parent', w.DWORD), ('priority', w.LONG), ('flags', w.DWORD),
                    ('exe', w.WCHAR * 260)]
    kernel.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = w.HANDLE
    kernel.Process32FirstW.argtypes = [w.HANDLE, c.POINTER(Entry)]
    kernel.Process32NextW.argtypes = [w.HANDLE, c.POINTER(Entry)]
    snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
    if snapshot == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    result = []
    try:
        entry = Entry(); entry.size = c.sizeof(entry)
        more = kernel.Process32FirstW(snapshot, c.byref(entry))
        while more:
            if entry.exe.casefold() == name.casefold():
                result.append(entry.pid)
            more = kernel.Process32NextW(snapshot, c.byref(entry))
    finally:
        kernel.CloseHandle(snapshot)
    return result


class Process:
    def __init__(self, pid):
        self.pid = pid
        self.handle = kernel.OpenProcess(0x410, False, pid)
        if not self.handle:
            raise c.WinError(c.get_last_error())

    def __enter__(self):
        return self

    def __exit__(self, *args):
        kernel.CloseHandle(self.handle)

    def read(self, address, size):
        buffer = c.create_string_buffer(size); count = c.c_size_t()
        if not kernel.ReadProcessMemory(self.handle, address, buffer, size, c.byref(count)) or count.value != size:
            raise OSError(f'Cannot read {address:#x}: Win32 {c.get_last_error()}')
        return buffer.raw

    def modules(self):
        array = (c.c_void_p * 4096)(); needed = w.DWORD()
        if not psapi.EnumProcessModulesEx(self.handle, array, c.sizeof(array), c.byref(needed), 3):
            raise c.WinError(c.get_last_error())
        if needed.value > c.sizeof(array):
            raise RuntimeError('Module enumeration buffer exhausted')
        result = {}
        for address in array[:needed.value // c.sizeof(c.c_void_p)]:
            path = c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(self.handle, address, path, len(path)):
                raise c.WinError(c.get_last_error())
            result[Path(path.value).name.casefold()] = path.value
        return result

    def handoff(self):
        fields = struct.unpack('<8IQ', self.read(HANDOFF, 40))
        if fields[:6] != (0x504D4150, 2, self.pid, BASE, 0x140C000, 0x1411000):
            raise RuntimeError('Missing/incompatible staged-image handoff; fresh mapping required')
        return fields[6]

    def menu_state_address(self):
        """Find the loader's exported UI state without executing target code."""
        array=(c.c_void_p*4096)();needed=w.DWORD()
        if not psapi.EnumProcessModulesEx(self.handle,array,c.sizeof(array),c.byref(needed),3):
            raise c.WinError(c.get_last_error())
        if needed.value>c.sizeof(array):raise RuntimeError('Module enumeration buffer exhausted')
        for base in array[:needed.value//c.sizeof(c.c_void_p)]:
            path=c.create_unicode_buffer(32768)
            if not psapi.GetModuleFileNameExW(self.handle,base,path,len(path)):raise c.WinError(c.get_last_error())
            if Path(path.value).name.casefold()!='primordial_loader.dll':continue
            header=self.read(base,4096);pe=struct.unpack_from('<I',header,60)[0]
            if header[:2]!=b'MZ' or pe>4000 or header[pe:pe+4]!=b'PE\0\0':raise RuntimeError('Invalid loader PE')
            rva,size=struct.unpack_from('<II',header,pe+24+96)
            if not rva:return None
            exports=self.read(base+rva,40)
            nf,nn,af,an,ao=struct.unpack_from('<5I',exports,20)
            if not 0<nn<=nf<=65536:raise RuntimeError('Invalid loader export bounds')
            names=struct.unpack('<'+'I'*nn,self.read(base+an,4*nn))
            ordinals=struct.unpack('<'+'H'*nn,self.read(base+ao,2*nn))
            functions=struct.unpack('<'+'I'*nf,self.read(base+af,4*nf))
            for name,ordinal in zip(names,ordinals):
                text=self.read(base+name,64).split(b'\0')[0]
                if text in (b'primordial_menu_state',b'_primordial_menu_state'):
                    if ordinal>=nf or rva<=functions[ordinal]<rva+size:raise RuntimeError('Invalid UI state export')
                    return base+functions[ordinal]
        return None


def one(name):
    matches = processes(name)
    if len(matches) > 1:
        raise RuntimeError(f'Multiple {name} processes: {matches}; select using --game-pid')
    return matches[0] if matches else None


def pe_machine(path):
    with Path(path).open('rb') as f:
        dos = f.read(64)
        if dos[:2] != b'MZ':
            raise RuntimeError(f'Not a PE file: {path}')
        f.seek(struct.unpack_from('<I', dos, 60)[0])
        pe = f.read(6)
    if pe[:4] != b'PE\0\0':
        raise RuntimeError(f'Invalid PE signature: {path}')
    return struct.unpack_from('<H', pe, 4)[0]


def inject(pid, dll, machine, allow_pending=False):
    helper = ROOT / 'build/injector' / ('x86' if machine == 0x14c else 'x64') / 'loadlibrary_injector.exe'
    if not helper.is_file():
        raise RuntimeError('Build injectors first: powershell -File tools/build-injector.ps1')
    print(f'Loading {dll} into PID {pid}', flush=True)
    completed = subprocess.run([str(helper), str(pid), str(dll.resolve()), '15000'])
    if completed.returncode == 3 and allow_pending:
        return False
    if completed.returncode:
        raise RuntimeError(f'Injector returned {completed.returncode}; inspect target/debugger before retrying')
    return True


def rpc(endpoint, name, arguments):
    body = {'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
            'params': {'name': name, 'arguments': arguments}}
    request = urllib.request.Request(endpoint, data=json.dumps(body).encode(),
                                    headers={'Content-Type': 'application/json',
                                             'Accept': 'application/json, text/event-stream'})
    with urllib.request.urlopen(request, timeout=20) as response:
        result = json.load(response)
    if 'error' in result or result.get('result', {}).get('isError'):
        raise RuntimeError(f'{name}: {result}')
    blocks = result['result'].get('content', [])
    texts = [x['text'] for x in blocks if x.get('type') == 'text']
    value = json.loads(texts[0]) if len(texts) == 1 else texts
    if isinstance(value, dict) and (value.get('error') or value.get('success') is False):
        raise RuntimeError(f'{name}: {value}')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=['status', 'run'])
    parser.add_argument('--launch', action='store_true', help='Launch the specified legacy csgo.exe if absent')
    parser.add_argument('--debug', action='store_true', help='Attach idle x32dbg and set a hardware entry breakpoint')
    parser.add_argument('--game-pid', type=int)
    parser.add_argument('--game', type=Path, default=Path(r'C:\Program Files (x86)\Steam\steamapps\common\csgo legacy\csgo.exe'))
    parser.add_argument('--release', type=Path, default=ROOT / 'build')
    parser.add_argument('--mcp', default='http://127.0.0.1:3000/mcp')
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    game_pid = args.game_pid or one('csgo.exe')
    steam_pid = one('steam.exe')
    if args.command == 'status':
        result = {'steam_pid': steam_pid, 'game_pid': game_pid, 'game_executable': str(args.game)}
        if game_pid:
            with Process(game_pid) as game:
                result['modules'] = game.modules()
                try: result['handoff_state'] = game.handoff()
                except (OSError, RuntimeError) as exc: result['handoff_error'] = str(exc)
                address=game.menu_state_address()
                result['menu_state']=struct.unpack('<I',game.read(address,4))[0] if address else None
        print(json.dumps(result, indent=2)); return
    if not steam_pid:
        raise RuntimeError('Start Steam and sign in first')
    if not game_pid and not args.launch:
        raise RuntimeError('CS:GO is absent; use --launch or start it manually')
    if args.debug:
        state = rpc(args.mcp, 'debug_get_state', {})
        if state.get('state') != 'stopped':
            raise RuntimeError(f'x32dbg already has a session; preserving it: {state}')
    patcher = args.release.resolve() / 'primordial-x86-native/primordial_loader.dll'
    stager = args.release.resolve() / 'steam-x64-native/steam_module.dll'
    for path in (patcher, patcher.with_name('primordial_tls.dll'), stager):
        if not path.is_file(): raise RuntimeError(f'Missing required DLL: {path}')
    if not game_pid:
        if args.game.name.casefold() != 'csgo.exe' or pe_machine(args.game) != 0x14c:
            raise RuntimeError('Expected an x86 legacy csgo.exe')
        with Process(steam_pid) as steam:
            steam_modules = steam.modules()
            steam_exe = Path(steam_modules['steam.exe'])
            if pe_machine(steam_exe) != 0x8664:
                raise RuntimeError('Configured Steam stager requires x64 Steam')
            loaded = steam_modules.get('steam_module.dll')
            if loaded and Path(loaded).resolve() != stager:
                raise RuntimeError(f'Steam has a different pinned stager: {loaded}. Restart Steam manually to replace it.')
        inject(steam_pid, stager, 0x8664)
        # The stager starts asynchronously. Wait before launch so it can install
        # its hooks; the handoff check below is the actual staging success guard.
        time.sleep(1)
        print(f'Launching {args.game}', flush=True)
        subprocess.Popen([str(args.game), '-windowed', '-insecure', '-steam'], cwd=str(args.game.parent))
        deadline = time.monotonic() + args.timeout
        while not game_pid and time.monotonic() < deadline:
            game_pid = one('csgo.exe'); time.sleep(0.25)
        if not game_pid: raise RuntimeError('Legacy CS:GO did not appear')
    deadline = time.monotonic() + args.timeout
    with Process(game_pid) as game:
        print(f'CS:GO PID {game_pid}; waiting for staging and {len(MODULES)} game modules', flush=True)
        while True:
            modules = game.modules()
            actual_game = modules.get('csgo.exe')
            if not actual_game or Path(actual_game).resolve() != args.game.resolve():
                raise RuntimeError(f'Target executable differs from --game: {actual_game}')
            if pe_machine(actual_game) != 0x14c: raise RuntimeError('CS:GO target is not x86')
            missing = [name for name in MODULES if name not in modules]
            try: state = game.handoff()
            except (OSError, RuntimeError): state = None
            if state in (2, 3, 4):
                raise RuntimeError(f'Payload already attempted initialization (state {state}); use a fresh game process')
            if not missing and state == 1: break
            if time.monotonic() >= deadline:
                raise RuntimeError(f'Readiness timed out: handoff={state}, missing={missing}')
            time.sleep(0.5)
        if 'primordial_loader.dll' in modules:
            raise RuntimeError('A patcher is already loaded; preserving the existing process')
        if args.debug:
            print('Attaching x32dbg; hardware entry breakpoint preserves the raw checksum', flush=True)
            rpc(args.mcp, 'debug_attach_pid', {'pid': game_pid, 'attach_break': True, 'detach_first': False})
            state = rpc(args.mcp, 'debug_get_state', {})
            if state.get('state') != 'paused': raise RuntimeError(f'Debugger did not pause: {state}')
            # An idle debugger creates these one-shot breakpoints during attach.
            # They fire on DLL_THREAD_ATTACH too, before a remote LoadLibraryA
            # thread reaches its start routine. This session was created here;
            # preserve all other breakpoints and never continue unknown stops.
            breakpoints = rpc(args.mcp, 'breakpoint_list', {}).get('breakpoints', [])
            for breakpoint in breakpoints:
                if breakpoint.get('type') == 'software' and breakpoint.get('name', '').startswith('TLS Callback '):
                    rpc(args.mcp, 'breakpoint_disable', {'address': breakpoint['address']})
            # Preserve exit call stacks instead of losing a self-terminating
            # target immediately after startup. These sites are outside the dump.
            for symbol in ('ntdll.NtTerminateProcess', 'ntdll.RtlExitUserProcess'):
                resolved = rpc(args.mcp, 'symbol_resolve', {'symbol': symbol})
                address = resolved.get('address')
                if not address:
                    raise RuntimeError(f'Cannot resolve termination breakpoint: {resolved}')
                rpc(args.mcp, 'breakpoint_set', {'address': address, 'type': 'software'})
            rpc(args.mcp, 'breakpoint_set', {'address': hex(ENTRY), 'type': 'hardware',
                                            'hw_condition': 'execute', 'hw_size': 1})
            rpc(args.mcp, 'debug_run', {})
        loaded = inject(game_pid, patcher, 0x14c, allow_pending=args.debug)
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            if args.debug:
                debug_state = rpc(args.mcp, 'debug_get_state', {})
                if debug_state.get('state') == 'paused':
                    registers = rpc(args.mcp, 'register_get_batch', {'names': ['eip', 'esp', 'ebp', 'eax']})
                    result = {'pid': game_pid, 'debugger': debug_state, 'registers': registers,
                              'handoff_state': game.handoff(), 'expected_entry': hex(ENTRY),
                              'loadlibrary_completed': loaded}
                    directory = ROOT / 'analysis/menu'; directory.mkdir(exist_ok=True)
                    path = directory / f'debug-start-{game_pid}.json'
                    path.write_text(json.dumps(result, indent=2) + '\n')
                    print(f'Debugger paused; inspect stop reason before resuming. Evidence: {path}', flush=True)
                    print(json.dumps(result, indent=2)); return
            state = game.handoff()
            if state in (3, 4):
                subprocess.run(['py', '-3', str(ROOT / 'tools/capture_payload_state.py'), str(game_pid)], check=True)
                if state == 4: raise RuntimeError('Payload initialization failed; see crash log and snapshot')
                address=game.menu_state_address()
                if address:
                    while time.monotonic()<deadline:
                        menu_state=struct.unpack('<I',game.read(address,4))[0]
                        if menu_state==3:break
                        if menu_state==4:raise RuntimeError('Menu UI initialization failed; see primordial_menu.log in the game directory')
                        time.sleep(0.1)
                    else:raise RuntimeError('Entry returned TRUE but menu is still awaiting initialization/paint; inspect primordial_menu.log')
                    print('Entry returned TRUE; integrated menu completed its first real draw. Confirm appearance and interaction in the game.',flush=True)
                else:
                    print('Entry returned TRUE. This older loader has no integrated UI status; menu visibility still requires visual confirmation.', flush=True)
                return
            time.sleep(0.25)
        raise RuntimeError('Initialization is still pending; inspect debugger/game before retrying')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        raise SystemExit(f'ERROR: {exc}')
