"""Offline first-boundary probe of the actual menu renderer after construction.

Loads only the retained Unicorn snapshot. Does not call target functions or
write target memory. No missing interface, font, API result or page is invented.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'build/analysis-python'))
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_INVALID, UC_HOOK_MEM_READ
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

BASE = 0x44340000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True, help='Snapshot identifier, not a process access request')
    parser.add_argument('--cursor', choices=['stop', 'absent', 'present'], default='stop',
                        help='Explicit hypothetical GetCursor result; never an observed target result')
    parser.add_argument('--model-basic-crt', action='store_true',
                        help='Use explicit offline allocation/copy models already verified in constructor')
    parser.add_argument('--bind-source-facts', action='store_true',
                        help='Offline Engine.IsInGame model and timing/dimensions from read-only current facts')
    parser.add_argument('--interpret-snapshot-crt', action='store_true',
                        help='Interpret copied CRT and its KernelBase/NTDLL support, stopping at missing pages/OS transitions')
    parser.add_argument('--model-surface-draw', action='store_true',
                        help='Record hypothetical Source drawing calls without producing pixels')
    parser.add_argument('--model-empty-config', action='store_true',
                        help='Use the verified constructed config path with explicit virtual empty enumeration')
    parser.add_argument('--fix-empty-selector', action='store_true',
                        help='Offline byte-guarded JA-to-JG experiment for the verified zero-item selector bug')
    parser.add_argument('--repair-captured-strings', action='store_true',
                        help='Offline exact-profile intervention at verified native wstring release/assignment')
    parser.add_argument('--draw-window-only', action='store_true',
                        help='Explore primary native window draw without frame/input/periodic feature callbacks')
    parser.add_argument('--model-directory-handles', action='store_true',
                        help='OFFLINE ONLY: exact read-attributes opens and identity queries for virtual menu directories')
    parser.add_argument('--max-steps', type=int, default=1000000)
    parser.add_argument('--model-material-lookup', action='store_true',
                        help='OFFLINE ONLY: opaque effects/spark material result; no material methods or rendering modeled')
    parser.add_argument('--bind-native-script-path', action='store_true',
                        help='OFFLINE ONLY: transplant just the original162FA0-produced path from its retained replay')
    parser.add_argument('--model-native-cursor', action='store_true',
                        help='OFFLINE ONLY: original SetCursor request with hypothetical previous cursor')
    args = parser.parse_args()
    if args.model_directory_handles and not args.model_empty_config:
        parser.error('--model-directory-handles requires the explicit virtual empty filesystem')
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    with zipfile.ZipFile(ROOT / f'analysis/menu/menu-constructed-offline-{args.pid}.zip') as archive:
        manifest = json.loads(archive.read('manifest.json'))
        for region in manifest['regions']:
            cpu.mem_map(region['start'], region['size'])
            cpu.mem_write(region['start'], archive.read(region['member']))
    if args.interpret_snapshot_crt:
        with zipfile.ZipFile(ROOT / f'analysis/menu/renderer-crt-pages-{args.pid}.zip') as archive:
            supplements = json.loads(archive.read('manifest.json'))
            for page in supplements['pages']:
                address = page['address']
                try:
                    cpu.mem_read(address, 1)
                except UcError:
                    cpu.mem_map(address, 4096)
                    cpu.mem_write(address, archive.read(page['member']))
    cpu.reg_write(UC_X86_REG_MXCSR, 0x1F80)
    if args.fix_empty_selector:
        expected = bytes.fromhex('85ff78468b9950010000b8932449928b89540100002bcbf7e903d1c1fa048bc2c1e81f4803c23bf87720')
        if bytes(cpu.mem_read(BASE + 0x34356A, len(expected))) != expected:
            raise RuntimeError('Selector instruction profile mismatch')
        cpu.mem_write(BASE + 0x343592, b'\x7f')
    cpu.reg_write(UC_X86_REG_FPCW, 0x027F)
    for reg in [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]:
        cpu.reg_write(reg, 0x10011000)
    cpu.reg_write(UC_X86_REG_ESP, 0x10010000)
    cpu.mem_write(0x10010000, struct.pack('<I', 0x12345678))
    entry = 0x351AB0 if args.draw_window_only else 0x32BA40
    if args.draw_window_only:
        cpu.reg_write(UC_X86_REG_ECX, 0x447F33B0)
        cpu.mem_write(0x447F33B0 + 0x94, struct.pack('<f', 1.0))
    surface_methods = {
        14: ('DrawSetColorPacked', 1),
        15: ('DrawSetColor', 4), 16: ('DrawFilledRect', 4),
        18: ('DrawOutlinedRect', 4), 19: ('DrawLine', 4),
        23: ('DrawSetTextFont', 1), 25: ('DrawSetTextColor', 4),
        24: ('DrawSetTextColorPacked', 1),
        26: ('DrawSetTextPos', 2), 28: ('DrawPrintText', 3),
        37: ('DrawSetTextureRGBA', 4), 38: ('DrawSetTexture', 1),
        41: ('DrawTexturedRect', 4),
        43: ('CreateNewTextureID', 1), 147: ('SetClipRect', 4),
        106: ('DrawTexturedPolygon', 3),
        104: ('DrawTexturedPolyLine', 2),
    }
    draw_stubs = {}
    draw_font = [0]
    texture_next = [1]
    if args.model_surface_draw:
        for slot, signature in surface_methods.items():
            stub = 0x26001000 + slot * 16
            cpu.mem_write(stub, b'\xc3')
            cpu.mem_write(0x26000080 + slot * 4, struct.pack('<I', stub))
            draw_stubs[stub] = signature
    source_facts = None
    if args.bind_source_facts:
        source_facts = json.loads((ROOT / f'analysis/menu/current-render-api-facts-{args.pid}.json').read_text())
        game_windows = [r for r in source_facts['visible_game_windows']
                        if r['title'].startswith('Counter-Strike: Global Offensive')]
        if len(game_windows) != 1:
            raise RuntimeError('Expected one captured real game client rectangle')
        cpu.mem_map(0x26003000, 0x3000)
        cpu.mem_write(0x26003000, struct.pack('<I', 0x26003100))
        cpu.mem_write(0x26003100 + 26 * 4, struct.pack('<I', 0x26004000))
        cpu.mem_write(0x26004000, b'\xc3')
        cpu.mem_write(0x26005000, bytes.fromhex(source_facts['globalvars_bytes']))
        cpu.mem_write(0x4481E174, struct.pack('<I', 0x26003000))
        cpu.mem_write(0x4481E164, struct.pack('<I', 0x26005000))
        cpu.mem_write(0x44B487AC, struct.pack('<II', game_windows[0]['width'], game_windows[0]['height']))
    if args.model_material_lookup:
        cpu.mem_map(0x26006000,0x2000)
        cpu.mem_write(0x26006000,struct.pack('<I',0x26006100))
        cpu.mem_write(0x26006100+84*4,struct.pack('<I',0x26007000))
        cpu.mem_write(0x26006010,struct.pack('<I',0x26006500))
        cpu.mem_write(BASE+0x4DE19C,struct.pack('<I',0x26006000))
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    original = (ROOT / 'primordial.bin').read_bytes()
    if hashlib.sha256(original).hexdigest() != 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768':
        raise RuntimeError('Original dump identity changed')
    tail = collections.deque(maxlen=32)
    standard_models = {int(a): name for a, name in manifest['standard_models'].items()}
    exports = json.loads((ROOT / f'analysis/menu/runtime-standard-exports-{args.pid}.json').read_text())['exports']
    set_cursor_addresses={int(e['address'],16) for e in exports if e['module'].lower()=='user32.dll' and e['name']=='SetCursor' and not e['forwarded']}
    for export in exports:
        if not export['forwarded'] and export['name'] in ('malloc', 'calloc', 'memcpy', 'memmove', 'memset',
                                                         'GetLastError', 'SetLastError'):
            standard_models[int(export['address'], 16)] = export['name']
    heap_next = [0x22000000]
    allocation_context = json.loads((ROOT / f'analysis/menu/snapshot-allocator-context-{args.pid}.json').read_text())
    allocations = {int(a, 16): size for a, size in allocation_context['allocations'].items()}
    virtual_find = {'open': False, 'index': 0, 'handle': 0x66000001}
    directory_handles = {}
    virtual_directories = ('primordial', 'primordial/configs', 'primordial/scripts',
                           'primordial/skyboxes', 'csgo/materials/skybox', 'primordial/hitsounds', 'csgo/sound')
    if args.model_empty_config:
        with zipfile.ZipFile(ROOT / f'analysis/menu/config-constructed-offline-{args.pid}.zip') as archive:
            config_manifest = json.loads(archive.read('manifest.json'))

            def copied_bytes(address, length):
                region = next(r for r in config_manifest['regions']
                              if r['start'] <= address and address + length <= r['start'] + r['size'])
                return archive.read(region['member'])[address - region['start']:address - region['start'] + length]

            descriptor = copied_bytes(0x447F3D14, 24)
            pointer, _, _, _, size, capacity = struct.unpack('<6I', descriptor)
            text = copied_bytes(pointer, (size + 1) * 2)
            if text.decode('utf-16-le') != 'primordial/configs\0' or capacity != 23:
                raise RuntimeError('Constructed config path differs from verified native assignment')
        cpu.mem_write(0x23000000, text)
        cpu.mem_write(0x447F3D14, struct.pack('<6I', 0x23000000, 0, 0, 0, size, capacity))
        allocations[0x23000000] = (capacity + 1) * 2
        for export in exports:
            if export['name'] in ('CreateDirectoryW', 'GetFileAttributesExW', 'FindFirstFileExW', 'FindNextFileW', 'FindClose') and not export['forwarded']:
                standard_models[int(export['address'], 16)] = 'virtual_config_' + export['name']
            if args.model_directory_handles and export['name'] in ('CreateFileW', 'GetFileInformationByHandle', 'GetFileInformationByHandleEx', 'CloseHandle') and not export['forwarded']:
                standard_models[int(export['address'], 16)] = 'virtual_config_' + export['name']
    if args.bind_native_script_path:
        proof=json.loads((ROOT/'analysis/menu/path-initializer-162fa0-offline.json').read_text())
        if proof['stop_reason']!='returned to caller sentinel with balanced cdecl stack':
            raise RuntimeError('Native script-path replay is incomplete')
        with zipfile.ZipFile(ROOT/'analysis/menu/path-initialized-offline-162fa0.zip') as archive:
            entries=json.loads(archive.read('manifest.json'))['regions']
            def retained(address,size):
                region=next(r for r in entries if r['start']<=address and address+size<=r['start']+r['size'])
                return archive.read(region['member'])[address-region['start']:address-region['start']+size]
            descriptor=retained(BASE+0x4B3C74,24)
            pointer,_,_,_,size,capacity=struct.unpack('<6I',descriptor)
            text=retained(pointer,(size+1)*2)
            if text.decode('utf-16-le')!='primordial/scripts\0' or capacity!=23:
                raise RuntimeError('Native script directory output differs from retained proof')
        cpu.mem_write(0x23010000,text)
        cpu.mem_write(BASE+0x4B3C74,struct.pack('<6I',0x23010000,0,0,0,size,capacity))
        allocations[0x23010000]=(capacity+1)*2
    random_state = [allocation_context['rand_state']]
    modules = json.loads((ROOT / f'analysis/menu/render-state-{args.pid}.json').read_text())['modules']
    crt_ranges = [(m['base'], m['base'] + m['size']) for m in modules
                  if Path(m['path']).name.lower() in ('ucrtbase.dll', 'msvcp140.dll', 'vcruntime140.dll',
                                                      'kernelbase.dll', 'ntdll.dll') and m['base'] <= 0xFFFFFFFF]
    result = {'snapshot_pid': args.pid, 'mode': 'OFFLINE ONLY; no target process access',
              'entry_rva': hex(entry), 'instructions': 0, 'calls': [],
              'hypothetical_cursor_case': args.cursor, 'modeled_calls': []}
    result['source_fact_binding'] = bool(source_facts)
    result['experimental_empty_selector_patch'] = args.fix_empty_selector
    result['experimental_captured_string_repairs'] = []
    result['loader_context_reads'] = []

    def instruction(cpu, address, size, _):
        result['instructions'] += 1
        if args.model_native_cursor and address in set_cursor_addresses:
            sp=cpu.reg_read(UC_X86_REG_ESP);ret,cursor=struct.unpack('<II',bytes(cpu.mem_read(sp,8)))
            choice=struct.unpack('<I',bytes(cpu.mem_read(BASE+0x804E10,4)))[0]
            if choice>=7 or cursor!=struct.unpack('<I',bytes(cpu.mem_read(BASE+0x804E14+4*choice,4)))[0]:
                result['stop_reason']='native cursor request differs from original table';cpu.emu_stop();return
            result['modeled_calls'].append(dict(api='SetCursor',requested=hex(cursor),choice=choice,model='hypothetical cursor only; no target API call'))
            cpu.reg_write(UC_X86_REG_EAX,0 if args.cursor=='absent' else 1)
            cpu.reg_write(UC_X86_REG_ESP,sp+8);cpu.reg_write(UC_X86_REG_EIP,ret);return
        if args.model_material_lookup and address==0x26007000:
            sp=cpu.reg_read(UC_X86_REG_ESP)
            words=struct.unpack('<5I',bytes(cpu.mem_read(sp,20)))
            text=bytes(cpu.mem_read(words[1],64)).split(b'\0')[0]
            if text!=b'effects/spark' or any(words[2:]):
                result['stop_reason']='unmodeled material lookup arguments';cpu.emu_stop();return
            result['modeled_calls'].append(dict(api='synthetic MaterialSystem84 FindMaterial',name=text.decode(),
                model='opaque material identity only; no object methods or pixels',result='0x26006010'))
            cpu.reg_write(UC_X86_REG_EAX,0x26006010)
            cpu.reg_write(UC_X86_REG_ESP,sp+20)
            cpu.reg_write(UC_X86_REG_EIP,words[0]);return
        if args.repair_captured_strings and address in (BASE + 0xA11A0, BASE + 0xAEC80):
            obj = cpu.reg_read(UC_X86_REG_ECX)
            if BASE + 0x4B2000 <= obj <= BASE + 0x4DDBE0 - 24:
                raw = bytes(cpu.mem_read(obj, 24))
                capacity, = struct.unpack_from('<I', raw, 20)
                if capacity >= 8 and raw == original[obj - BASE:obj - BASE + 24]:
                    replacement = bytes(20) + struct.pack('<I', 7)
                    cpu.mem_write(obj, replacement)
                    result['experimental_captured_string_repairs'].append({
                        'rva': hex(obj - BASE), 'expected_bytes': raw.hex(),
                        'replacement_bytes': replacement.hex(), 'native_callee_rva': hex(address - BASE)})
        if address in draw_stubs:
            api, argc = draw_stubs[address]
            sp = cpu.reg_read(UC_X86_REG_ESP)
            words = struct.unpack('<' + 'I' * (argc + 1), bytes(cpu.mem_read(sp, 4 * (argc + 1))))
            event = {'api': 'ISurface.' + api, 'arguments': [hex(v) for v in words[1:]],
                     'model': 'record draw intent only; no real font, texture or pixel output'}
            value = 0
            if api == 'DrawSetTextFont':
                draw_font[0] = words[1]
                event['native_font_record'] = hex(cpu.reg_read(UC_X86_REG_EDI))
            elif api == 'DrawPrintText':
                if words[2] > 65536:
                    result['stop_reason'] = 'draw text length bound'
                    cpu.emu_stop()
                    return
                event['font'] = draw_font[0]
                event['text'] = bytes(cpu.mem_read(words[1], min(words[2], 256) * 2)).decode('utf-16-le', errors='replace')
            elif api == 'CreateNewTextureID':
                value = texture_next[0]
                texture_next[0] += 1
                event['hypothetical_texture_id'] = value
            result['modeled_calls'].append(event)
            cpu.reg_write(UC_X86_REG_EAX, value)
            cpu.reg_write(UC_X86_REG_ESP, sp + (argc + 1) * 4)
            cpu.reg_write(UC_X86_REG_EIP, words[0])
            return
        if address == 0x26004000 and source_facts:
            sp = cpu.reg_read(UC_X86_REG_ESP)
            ret, = struct.unpack('<I', bytes(cpu.mem_read(sp, 4)))
            cpu.reg_write(UC_X86_REG_EAX, int(source_facts['derived_is_in_game']))
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EIP, ret)
            result['modeled_calls'].append({'api': 'IVEngineClient.IsInGame',
                'result_from_read_only_signon_predicate': source_facts['derived_is_in_game']})
            return
        name = standard_models.get(address)
        if name and name.startswith('virtual_config_'):
            sp = cpu.reg_read(UC_X86_REG_ESP)
            values = struct.unpack('<7I', bytes(cpu.mem_read(sp, 28)))
            event = {'api': name, 'model': 'explicit virtual empty directory; no real file access'}

            def dot_entry(destination, filename):
                data = bytearray(592)
                struct.pack_into('<I', data, 0, 0x10)
                encoded = (filename + '\0').encode('utf-16-le')
                data[44:44 + len(encoded)] = encoded
                cpu.mem_write(destination, bytes(data))

            if name.endswith('CreateFileW'):
                words = struct.unpack('<8I', bytes(cpu.mem_read(sp, 32)))
                data = bytearray()
                for i in range(1024):
                    char = bytes(cpu.mem_read(words[1] + 2*i, 2))
                    if char == b'\0\0':break
                    data.extend(char)
                path = data.decode('utf-16-le').replace('\\', '/').rstrip('/')
                paths = virtual_directories
                if path not in paths or words[2:] not in ((0x80,7,0,3,0x2200000,0),(0x80,7,0,3,0x2000000,0)):
                    result['stop_reason'] = 'unmodeled directory handle open'
                    result['unmodeled_directory_open'] = dict(path=path, arguments=list(words[2:]))
                    cpu.emu_stop();return
                value = 0x66000100 + paths.index(path)
                directory_handles[value] = path
                event.update(path=path,handle=hex(value),arguments=list(words[2:]))
                pop = 32
            elif name.endswith('GetFileInformationByHandleEx'):
                if values[1] not in directory_handles or (values[2],values[4]) not in ((9,8),(1,24),(18,24)):
                    result['stop_reason']='unmodeled extended directory information request';cpu.emu_stop();return
                if values[2]==9:data=struct.pack('<II',0x10,0)
                elif values[2]==18:
                    data=struct.pack('<QQQ',1,values[1]-0x66000000,0)
                else:
                    data=bytearray(24)
                    struct.pack_into('<I',data,16,1)
                    data[21]=1
                cpu.mem_write(values[3],bytes(data))
                event.update(path=directory_handles[values[1]],information_class=values[2],size=values[4])
                value=1;pop=20
            elif name.endswith('GetFileInformationByHandle'):
                if values[1] not in directory_handles:
                    result['stop_reason'] = 'unmodeled file identity handle';cpu.emu_stop();return
                data = bytearray(52)
                struct.pack_into('<I', data, 0, 0x10)
                struct.pack_into('<I', data, 28, 1)
                struct.pack_into('<I', data, 40, 1)
                struct.pack_into('<I', data, 48, values[1]-0x66000000)
                cpu.mem_write(values[2], data)
                event.update(path=directory_handles[values[1]],handle=hex(values[1]))
                value=1;pop=12
            elif name.endswith('CloseHandle'):
                if values[1] not in directory_handles:
                    result['stop_reason']='unmodeled directory close handle';cpu.emu_stop();return
                event.update(path=directory_handles.pop(values[1]),handle=hex(values[1]))
                value=1;pop=8

            elif name.endswith('GetFileAttributesExW'):
                data = bytearray()
                for i in range(1024):
                    char = bytes(cpu.mem_read(values[1] + 2 * i, 2))
                    if char == b'\0\0':
                        break
                    data.extend(char)
                path = data.decode('utf-16-le').replace('\\', '/').rstrip('/')
                if values[2] or path not in virtual_directories:
                    result['stop_reason'] = 'unmodeled virtual directory attribute arguments'
                    result['unmodeled_path'] = path
                    cpu.emu_stop()
                    return
                cpu.mem_write(values[3], struct.pack('<I', 0x10) + bytes(32))
                event['path'] = path
                value = 1
                pop = 16
            elif name.endswith('CreateDirectoryW'):
                data = bytearray()
                for i in range(1024):
                    char = bytes(cpu.mem_read(values[1] + 2 * i, 2))
                    if char == b'\0\0':
                        break
                    data.extend(char)
                path = data.decode('utf-16-le').replace('\\', '/').rstrip('/')
                if values[2] or path not in virtual_directories:
                    result['stop_reason'] = 'unmodeled virtual directory creation arguments'
                    result['unmodeled_path'] = path
                    cpu.emu_stop()
                    return
                event['path'] = path
                value = 1
                pop = 12
            elif name.endswith('FindFirstFileExW'):
                data = bytearray()
                for i in range(1024):
                    char = bytes(cpu.mem_read(values[1] + 2 * i, 2))
                    if char == b'\0\0':
                        break
                    data.extend(char)
                path = data.decode('utf-16-le').replace('\\', '/')
                if path.replace('//', '/') not in tuple(x+'/*' for x in virtual_directories) or values[2] != 1 or any(values[4:]):
                    result['stop_reason'] = 'unmodeled virtual configuration search arguments'
                    result['unmodeled_path'] = path
                    cpu.emu_stop()
                    return
                dot_entry(values[3], '.')
                virtual_find.update(open=True, index=0)
                value = virtual_find['handle']
                pop = 28
            elif name.endswith('FindNextFileW'):
                if values[1] != virtual_find['handle'] or not virtual_find['open']:
                    result['stop_reason'] = 'unmodeled virtual directory handle'
                    cpu.emu_stop()
                    return
                pop = 12
                if virtual_find['index'] == 0:
                    dot_entry(values[2], '..')
                    virtual_find['index'] = 1
                    value = 1
                else:
                    value = 0
                    teb, = struct.unpack('<I', bytes(cpu.mem_read(0x18, 4)))
                    for slot in (0x34, teb + 0x34):
                        cpu.mem_write(slot, struct.pack('<I', 18))
            else:
                if values[1] != virtual_find['handle'] or not virtual_find['open']:
                    result['stop_reason'] = 'unmodeled virtual directory close'
                    cpu.emu_stop()
                    return
                virtual_find['open'] = False
                value = 1
                pop = 8
            result['modeled_calls'].append(event)
            cpu.reg_write(UC_X86_REG_EAX, value)
            cpu.reg_write(UC_X86_REG_ESP, sp + pop)
            cpu.reg_write(UC_X86_REG_EIP, values[0])
            return
        if args.model_basic_crt and name in ('GetLastError', 'SetLastError'):
            sp = cpu.reg_read(UC_X86_REG_ESP)
            ret, value = struct.unpack('<II', bytes(cpu.mem_read(sp, 8)))
            if name == 'GetLastError':
                value, = struct.unpack('<I', bytes(cpu.mem_read(0x34, 4)))
                cpu.reg_write(UC_X86_REG_EAX, value)
                pop = 4
            else:
                data = struct.pack('<I', value)
                cpu.mem_write(0x34, data)
                teb, = struct.unpack('<I', bytes(cpu.mem_read(0x18, 4)))
                cpu.mem_write(teb + 0x34, data)
                pop = 8
            result['modeled_calls'].append({'api': name, 'copied_thread_error_value': value})
            cpu.reg_write(UC_X86_REG_ESP, sp + pop)
            cpu.reg_write(UC_X86_REG_EIP, ret)
            return
        if args.model_basic_crt and name == 'synthetic_source_text_size':
            sp = cpu.reg_read(UC_X86_REG_ESP)
            ret, font, text, out_width, out_height = struct.unpack('<5I', bytes(cpu.mem_read(sp, 20)))
            length = 0
            while length < 65536 and bytes(cpu.mem_read(text + length * 2, 2)) != b'\0\0':
                length += 1
            if length == 65536:
                result['stop_reason'] = 'retained uniform text-metric model length bound'
                cpu.emu_stop()
                return
            cpu.mem_write(out_width, struct.pack('<I', length * 7))
            cpu.mem_write(out_height, struct.pack('<I', 14 if length else 0))
            result['modeled_calls'].append({'api': 'ISurface.GetTextSize', 'font': font,
                'text': bytes(cpu.mem_read(text, length * 2)).decode('utf-16-le', errors='replace'),
                'model': 'retained construction-only uniform metrics; not actual target fonts'})
            cpu.reg_write(UC_X86_REG_EAX, 0)
            cpu.reg_write(UC_X86_REG_ESP, sp + 20)
            cpu.reg_write(UC_X86_REG_EIP, ret)
            return
        if args.model_basic_crt and name in ('malloc', 'calloc', 'memcpy', 'memmove', 'memset', 'rand', 'free'):
            sp = cpu.reg_read(UC_X86_REG_ESP)
            values = struct.unpack('<4I', bytes(cpu.mem_read(sp, 16)))
            event = {'api': name, 'arguments': [hex(v) for v in values[1:]]}
            if name == 'rand':
                random_state[0] = (random_state[0] * 214013 + 2531011) & 0xFFFFFFFF
                value = (random_state[0] >> 16) & 0x7FFF
                event['model'] = 'continues the retained deterministic constructor stream; not game RNG state'
            elif name == 'free':
                pointer = values[1]
                if pointer and pointer not in allocations:
                    result['stop_reason'] = 'release of retained allocation; snapshot lifetime model not supplied'
                    result['unmodeled_free'] = hex(pointer)
                    cpu.emu_stop()
                    return
                if pointer:
                    del allocations[pointer]
                value = 0
            elif name in ('malloc', 'calloc'):
                length = values[1] if name == 'malloc' else values[1] * values[2]
                if length > 0x1000000 or heap_next[0] + max(length, 1) >= 0x24000000:
                    result['stop_reason'] = 'offline allocation bound'
                    cpu.emu_stop()
                    return
                value = heap_next[0]
                heap_next[0] += (max(length, 1) + 15) & ~15
                allocations[value] = length
                if name == 'calloc':
                    cpu.mem_write(value, bytes(length))
            else:
                value, source, length = values[1:]
                if length > 0x1000000:
                    result['stop_reason'] = 'offline copy bound'
                    cpu.emu_stop()
                    return
                data = bytes([source & 255]) * length if name == 'memset' else bytes(cpu.mem_read(source, length))
                cpu.mem_write(value, data)
            event['result'] = hex(value)
            result['modeled_calls'].append(event)
            cpu.reg_write(UC_X86_REG_EAX, value)
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EIP, values[0])
            return
        if address == 0x775B2570 and args.cursor != 'stop':
            # PID11240 USER32 PE export lookup independently identifies GetCursor.
            # Explore both null/non-null outcomes without claiming live state.
            sp = cpu.reg_read(UC_X86_REG_ESP)
            ret, = struct.unpack('<I', bytes(cpu.mem_read(sp, 4)))
            value = 0 if args.cursor == 'absent' else 1
            result['modeled_calls'].append({'api': 'GetCursor', 'hypothetical_result': value})
            cpu.reg_write(UC_X86_REG_EAX, value)
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EIP, ret)
            return
        if address == 0x12345678:
            result['stop_reason'] = 'returned to sentinel'
            cpu.emu_stop()
            return
        native_crt = args.interpret_snapshot_crt and any(lo <= address < hi for lo, hi in crt_ranges)
        if not BASE <= address < BASE + 0x140C000 and not native_crt:
            result['stop_reason'] = 'external function boundary; no API result modeled'
            cpu.emu_stop()
            return
        if bytes(cpu.mem_read(address, size)) in (b'\x0f\x34', b'\x0f\x05', b'\xcd\x2e'):
            result['stop_reason'] = 'OS transition boundary; no OS result invented'
            cpu.emu_stop()
            return
        item = next(decoder.disasm(bytes(cpu.mem_read(address, size)), address), None)
        if item:
            row = {'rva': hex(address - BASE), 'instruction': item.mnemonic + ' ' + item.op_str}
            tail.append(row)
            if item.mnemonic == 'call':
                result['calls'].append(dict(row, ecx=hex(cpu.reg_read(UC_X86_REG_ECX)),
                                            edx=hex(cpu.reg_read(UC_X86_REG_EDX))))

    def invalid(cpu, access, address, size, value, _):
        name = standard_models.get(address)
        if access==21 and args.model_native_cursor and address in set_cursor_addresses:
            cpu.mem_map(address&~4095,4096);cpu.mem_write(address,b'\xCC');return True
        if access == 21 and name and (name.startswith('virtual_config_') or
                                      (args.model_basic_crt and name in ('GetLastError', 'SetLastError'))):
            cpu.mem_map(address & ~4095, 4096)
            cpu.mem_write(address, b'\xc3')
            return True
        if address == 0x775B2570 and access == 21 and args.cursor != 'stop':
            cpu.mem_map(address & ~4095, 4096)
            cpu.mem_write(address, b'\xc3')
            return True
        result['stop_reason'] = 'unmapped access; no target page or synthetic replacement supplied'
        result['invalid_access'] = {'address': hex(address), 'size': size,
                                    'access': access, 'value': value}
        return False

    def read(cpu, access, address, size, value, _):
        if 0x1C85C000 <= address < 0x1C85E000:
            result['loader_context_reads'].append({'address': hex(address), 'size': size,
                'eip': hex(cpu.reg_read(UC_X86_REG_EIP)),
                'value': bytes(cpu.mem_read(address, size)).hex()})
        if address < 4096:
            eip = cpu.reg_read(UC_X86_REG_EIP)
            item = next(decoder.disasm(bytes(cpu.mem_read(eip, 15)), eip), None)
            if item is None or 0x64 not in item.prefix:
                result['stop_reason'] = 'ordinary null-page read; copied TEB is only valid for FS accesses'
                result['null_read'] = {'address': hex(address), 'size': size,
                                       'eip': hex(eip),
                                       'instruction': None if item is None else item.mnemonic + ' ' + item.op_str}
                cpu.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, instruction)
    cpu.hook_add(UC_HOOK_MEM_INVALID, invalid)
    cpu.hook_add(UC_HOOK_MEM_READ, read)
    try:
        cpu.emu_start(BASE + entry, 0, count=args.max_steps)
    except UcError as error:
        result['emulator_error'] = str(error)
    result['final_eip'] = hex(cpu.reg_read(UC_X86_REG_EIP))
    if cpu.reg_read(UC_X86_REG_EIP) == 0:
        result['stop_reason'] = 'control transferred to zero; missing dependency remains'
    result['final_esp'] = hex(cpu.reg_read(UC_X86_REG_ESP))
    if (result['final_eip'] == '0x12345678' and result['final_esp'] == '0x10010004' and
            result.get('invalid_access', {}).get('address') == '0x12345678' and
            result['invalid_access']['access'] == 21):
        result['expected_return_boundary'] = result.pop('invalid_access')
        result['stop_reason'] = 'returned to caller sentinel with balanced stack'
        result['emulator_error'] = None
    result['last_code'] = list(tail)
    result['stack'] = [hex(v) for v in struct.unpack('<12I', bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP), 48)))]
    result['stack_pointer_samples'] = {}
    for pointer in result['stack']:
        try:
            result['stack_pointer_samples'][pointer] = bytes(cpu.mem_read(int(pointer, 16), 64)).hex()
        except UcError:
            pass
    suffix = '' if args.cursor == 'stop' else '-' + args.cursor
    if args.model_basic_crt:
        suffix += '-crt'
    if args.bind_source_facts:
        suffix += '-source'
    if args.interpret_snapshot_crt:
        suffix += '-native'
    if args.model_surface_draw:
        suffix += '-draw'
    if args.model_empty_config:
        suffix += '-config'
    if args.fix_empty_selector:
        suffix += '-fixed'
    if args.repair_captured_strings:
        suffix += '-strings'
    if args.draw_window_only:
        suffix += '-window'
    if args.model_directory_handles:
        suffix += '-directory-handles'
    if args.model_material_lookup:
        suffix += '-material'
    if args.bind_native_script_path:
        suffix += '-native-script-path'
    if args.model_native_cursor:
        suffix += '-cursor-model'
    path = ROOT / f'analysis/menu/render-dependency-probe-{args.pid}{suffix}.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
