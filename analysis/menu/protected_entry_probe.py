"""Read-only process-backed OFFLINE x86 emulation of protected entry prefixes.

Requires workspace-local build/analysis-python/{unicorn,capstone}. The target is
only opened with PROCESS_QUERY_INFORMATION | PROCESS_VM_READ. No target code is
called, and no target memory/thread/debugger state is changed. Emulated general
registers/stack and CPU model are synthetic; results are prefix evidence, not
proof of a supported target call ABI or observed target execution.
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
sys.path[:0] = [str(ROOT / 'build/analysis-python'), str(ROOT / 'tools')]
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_INVALID, UC_MEM_FETCH_UNMAPPED
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_MXCSR, UC_X86_REG_FPCW
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from test_menu import Process

BASE = 0x44340000
ROOTS = [(0x2CAB40, 0x2CAB70), (0x2E04D0, 0x2E051B),
         (0x362970, 0x3629A0), (0x3752F0, 0x375323)]


def probe(process, image, teb, entry, marker, synthetic_fields=None, cpuid_override=None,
          standard_models=None, initial_registers=None, step_budget=1000000,
          reject_null_reads=False, external_code_ranges=(), synthetic_pages=None,
          experiment_captured_destructors=False, snapshot_output=None, observed_exports=None,
          observed_write_ranges=(), signature_adaptations=None, decoder_stop_targets=()):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    # Windows/MSVC masks floating-point exceptions by default. Unicorn's empty
    # register context is not a Windows thread's floating-point environment.
    cpu.reg_write(UC_X86_REG_MXCSR, 0x1F80)
    cpu.reg_write(UC_X86_REG_FPCW, 0x027F)
    cpu.mem_map(BASE, (len(image) + 4095) & ~4095)
    cpu.mem_write(BASE, image)
    # Include downward room for the menu constructor's real large stack probe.
    cpu.mem_map(0x0FF00000, 0x120000)
    # Default FS selector has base zero in Unicorn. Place a COPY of the current
    # TEB at zero so FS:[0x30] resolves the actual current target PEB.
    cpu.mem_map(0, 4096)
    cpu.mem_write(0, process.read(teb, 4096))
    cpu.mem_map(teb, 4096)
    cpu.mem_write(teb, process.read(teb, 4096))
    cpu.reg_write(UC_X86_REG_ESP, 0x10010000)
    cpu.mem_write(0x10010000, struct.pack('<I', 0x12345678))
    if synthetic_fields is not None:
        cpu.mem_map(0x1C85C000, 8192)
        for offset, value in synthetic_fields.items():
            cpu.mem_write(0x1C85CEC8 + offset, struct.pack('<I', value))
    for register in [UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                     UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP]:
        cpu.reg_write(register, 0x10011000)
    if initial_registers:
        for register, value in initial_registers.items():
            cpu.reg_write(register, value)
    if synthetic_pages:
        for address, data in synthetic_pages.items():
            cpu.mem_map(address, (len(data) + 4095) & ~4095)
            cpu.mem_write(address, data)
    heap_next = [0x20000000]
    allocations = {}
    random_state = [1]
    virtual_find = {'open': False, 'index': 0, 'handle': 0x66000001}
    virtual_next = [0x27000000]
    virtual_allocations = {}
    last_error = [struct.unpack('<I', process.read(teb + 0x34, 4))[0]]
    if standard_models:
        cpu.mem_map(0x20000000, 0x4000000)
        mapped_stubs = set()
        for address in standard_models:
            if address & ~4095 not in mapped_stubs:
                try:
                    cpu.mem_read(address, 1)
                except UcError:
                    cpu.mem_map(address & ~4095, 4096)
                    cpu.mem_write(address & ~4095, process.read(address & ~4095, 4096))
                mapped_stubs.add(address & ~4095)
    result = {'entry_rva': hex(entry), 'marker_boundary_rva': hex(marker),
              'instructions': 0, 'external_page_reads': [], 'cpuid': [],
              'loader_pointer_reads': [], 'pointer_value_writes': [],
              'synthetic_fields': None if synthetic_fields is None else
                  {hex(k): hex(v) for k, v in synthetic_fields.items()},
              'synthetic_field_reads': [], 'modeled_prefix_stack_writes': [],
              'native_calls': [], 'modeled_standard_calls': [],
              'native_export_entries': [],
              'original_stack_argument_reads': [], 'incoming_register_buffer_accesses': [],
              'native_image_reads': [], 'experimental_captured_object_resets': []}
    result['synthetic_cpuid_override'] = cpuid_override
    code = collections.deque(maxlen=20)
    reads = collections.deque(maxlen=40)
    image_data_reads = collections.deque(maxlen=128)
    font_records = [BASE + rva for rva in (0x580F34, 0x580F40, 0x7FB610, 0x805430, 0x805480, 0x8054C8)]
    result['font_record_writes'] = []
    result['external_code_writes'] = []
    result['signature_adaptations'] = signature_adaptations or {}
    result['native_decoder_observations'] = []
    result['feature_global_writes'] = []
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    ascii_pending = []
    ascii_validated = set()
    result['ascii_conversion_validation'] = []
    cpuid_pending = [False]
    modeled_prefix_active = [False]

    def on_code(cpu, address, size, _):
        result['instructions'] += 1
        code.append(hex(address - BASE))
        if address == BASE + 0x1554F0 and decoder_stop_targets:
            sp = cpu.reg_read(UC_X86_REG_ESP)
            words = struct.unpack('<4I', bytes(cpu.mem_read(sp, 16)))
            result['native_decoder_observations'].append({'return': hex(words[0]),
                                                          'arguments': [hex(v) for v in words[1:]]})
            # First stack argument is the output descriptor, second is the
            # instruction pointer loaded from [EBP+0Ch] by native1554F0.
            if words[2] in decoder_stop_targets:
                result['stop_reason'] = 'observed captured-address decoding result; stopped before stale target dereference'
                cpu.emu_stop()
                return
        if observed_exports and address in observed_exports and len(result['native_export_entries']) < 256:
            sp = cpu.reg_read(UC_X86_REG_ESP)
            words = struct.unpack('<7I', bytes(cpu.mem_read(sp, 28)))
            event = {'address': hex(address), 'names': observed_exports[address],
                     'return_address': hex(words[0]), 'arguments': [hex(v) for v in words[1:]],
                     'argument_pointer_samples': {}}
            for pointer in words[1:5]:
                try:
                    event['argument_pointer_samples'][hex(pointer)] = bytes(cpu.mem_read(pointer, 128)).hex()
                except UcError:
                    pass
            if any(name.endswith(('!NtCreateFile', '!NtOpenFile')) for name in observed_exports[address]):
                try:
                    attributes = words[3]
                    string_ptr, = struct.unpack('<I', bytes(cpu.mem_read(attributes + 8, 4)))
                    length, _, buffer = struct.unpack('<HHI', bytes(cpu.mem_read(string_ptr, 8)))
                    if length <= 4096:
                        event['object_name'] = bytes(cpu.mem_read(buffer, length)).decode('utf-16-le')
                except (UcError, UnicodeError):
                    pass
            result['native_export_entries'].append(event)
        sp_now = cpu.reg_read(UC_X86_REG_ESP)
        for item in list(ascii_pending):
            if address == item['return'] and sp_now == item['sp'] + 4:
                descriptor = bytes(cpu.mem_read(item['out'], 24))
                length, capacity = struct.unpack_from('<II', descriptor, 16)
                data_address = item['out'] if capacity < 16 else struct.unpack_from('<I', descriptor)[0]
                actual = bytes(cpu.mem_read(data_address, length))
                valid = actual == item['expected'] and cpu.reg_read(UC_X86_REG_EAX) == item['out']
                result['ascii_conversion_validation'].append({'expected': item['expected'].hex(),
                    'actual': actual.hex(), 'eax_is_output': cpu.reg_read(UC_X86_REG_EAX) == item['out'],
                    'cdecl_stack_cleanup': True, 'valid': valid})
                ascii_pending.remove(item)
                if not valid:
                    result['stop_reason'] = 'ASCII conversion semantic model validation failed'
                    cpu.emu_stop()
                    return
                ascii_validated.add(item['expected'])
        if experiment_captured_destructors and address in (BASE + 0xA11A0, BASE + 0xAEC80, BASE + 0x9FE00):
            obj = cpu.reg_read(UC_X86_REG_ECX)
            count = 12 if address == BASE + 0x9FE00 else 24
            if BASE + 0x4B2000 <= obj < BASE + 0x4DDBE0 - count:
                data = bytes(cpu.mem_read(obj, count))
                off = obj - BASE
                if data == image[off:off + count]:
                    pointer = struct.unpack_from('<I', data)[0]
                    candidate = pointer != 0
                    if count == 24:
                        candidate = candidate and struct.unpack_from('<I', data, 20)[0] >= 8
                    if candidate:
                        try:
                            process.read(pointer, 1)
                        except OSError:
                            replacement = bytes(20) + struct.pack('<I', 7) if count == 24 else bytes(12)
                            result['experimental_captured_object_resets'].append({
                                'rva': hex(off), 'kind': 'wstring24' if count == 24 else 'vector20_storage',
                                'expected_bytes': data.hex(), 'replacement_bytes': replacement.hex(),
                                'destructor_rva': hex(address - BASE)})
                            cpu.mem_write(obj, replacement)
        if bytes(cpu.mem_read(address, min(size, 2))) in (b'\x0f\x34', b'\x0f\x05', b'\xcd\x2e'):
            result['stop_reason'] = 'unmodeled OS transition; no syscall result invented'
            cpu.emu_stop()
            return
        if standard_models and address in standard_models:
            name = standard_models[address]
            sp = cpu.reg_read(UC_X86_REG_ESP)
            values = struct.unpack('<5I', bytes(cpu.mem_read(sp, 20)))
            event = {'name': name, 'address': hex(address), 'return': hex(values[0]),
                     'arguments': [hex(v) for v in values[1:]]}
            extra_pop = 0
            result['modeled_standard_calls'].append(event)
            if name in ('virtual_empty_config_FindFirstFileExW',
                        'virtual_empty_config_FindNextFileW', 'virtual_empty_config_FindClose'):
                event['model'] = 'explicit virtual empty-directory enumeration; no filesystem access'

                def write_dot_entry(destination, filename):
                    data = bytearray(592)
                    struct.pack_into('<I', data, 0, 0x10)
                    encoded = (filename + '\0').encode('utf-16-le')
                    data[44:44 + len(encoded)] = encoded
                    cpu.mem_write(destination, bytes(data))

                if name.endswith('FindFirstFileExW'):
                    args = struct.unpack('<6I', bytes(cpu.mem_read(sp + 4, 24)))
                    path = bytearray()
                    for offset in range(1024):
                        char = bytes(cpu.mem_read(args[0] + offset * 2, 2))
                        if char == b'\0\0':
                            break
                        path.extend(char)
                    directory = path.decode('utf-16-le').replace('\\', '/')
                    if directory not in ('primordial/configs/*','primordial/scripts/*','primordial/nades/*') or args[1] != 1 or any(args[3:]):
                        result['stop_reason'] = 'unmodeled virtual FindFirstFileExW arguments'
                        event['path'] = directory
                        cpu.emu_stop()
                        return
                    event['path'] = directory
                    virtual_find.update(open=True, index=0)
                    write_dot_entry(args[2], '.')
                    value = virtual_find['handle']
                    extra_pop = 24
                elif name.endswith('FindNextFileW'):
                    if values[1] != virtual_find['handle'] or not virtual_find['open']:
                        result['stop_reason'] = 'unmodeled virtual FindNextFileW handle'
                        cpu.emu_stop()
                        return
                    extra_pop = 8
                    if virtual_find['index'] == 0:
                        write_dot_entry(values[2], '..')
                        virtual_find['index'] = 1
                        value = 1
                    else:
                        value = 0
                        last_error[0] = 18
                        for slot in (0x34, teb + 0x34):
                            cpu.mem_write(slot, struct.pack('<I', 18))
                else:
                    if values[1] != virtual_find['handle'] or not virtual_find['open']:
                        result['stop_reason'] = 'unmodeled virtual FindClose handle'
                        cpu.emu_stop()
                        return
                    virtual_find['open'] = False
                    value = 1
                    extra_pop = 4
            elif name == 'virtual_empty_config_CreateDirectoryW':
                path = bytearray()
                for offset in range(1024):
                    char = bytes(cpu.mem_read(values[1] + 2 * offset, 2))
                    if char == b'\0\0':
                        break
                    path.extend(char)
                else:
                    result['stop_reason'] = 'virtual config directory path length bound'
                    cpu.emu_stop()
                    return
                directory = path.decode('utf-16-le')
                if values[2] or directory.replace('\\', '/') not in ('primordial', 'primordial/configs','primordial/scripts','primordial/scripts/include','primordial/nades'):
                    result['stop_reason'] = 'unexpected virtual config CreateDirectoryW arguments'
                    cpu.emu_stop()
                    return
                event['directory'] = directory
                event['model'] = 'explicit virtual directory creation; no filesystem writes'
                value = 1
                extra_pop = 8
            elif name == 'hypothetical_source_global_event_registration':
                if values[1] != BASE+0x4B3D54 or values[2] != 0:
                    result['stop_reason']='unmodeled event listener registration arguments';cpu.emu_stop();return
                event['model']='Explicit hypothetical AddListenerGlobal success; no registration, locks, Source mutation or event callbacks'
                value=1;extra_pop=8
            elif name == 'hypothetical_failed_inline_hook_request':
                # Deliberately take the observed VirtualAlloc-failure return
                # semantics of1554F0. This inventories requests without decoding,
                # allocating trampolines, modifying Source code, or supplying a
                # working original callback. It is NOT hook/startup success.
                output,target,replacement,original_out=struct.unpack('<4I',bytes(cpu.mem_read(sp+4,16)))
                event.update(model='Explicit hypothetical allocation failure; output record zeroed; no hook or original callback created',
                    target=hex(target),replacement=hex(replacement),original_out=hex(original_out),
                    preceding_image_reads=list(image_data_reads)[-20:])
                cpu.mem_write(output,bytes(12));value=output
            elif name == 'synthetic_are_file_apis_ansi':
                value=1
                event['model']='Explicit OFFLINE ANSI-file-API case; not an observed target result'
            elif name == 'synthetic_ascii_include_to_wide':
                args=struct.unpack('<7I',bytes(cpu.mem_read(sp,28)))
                if args[1:3]!=(0,8) or args[4]!=7 or bytes(cpu.mem_read(args[3],7))!=b'include' or (args[5] and args[6]<7):
                    result['stop_reason']='outside exact include ASCII conversion fixture';cpu.emu_stop();return
                if args[5]:cpu.mem_write(args[5],'include'.encode('utf-16-le'))
                elif args[6]:
                    result['stop_reason']='invalid include conversion output arguments';cpu.emu_stop();return
                value=7;extra_pop=24
                event['model']='Explicit OFFLINE exact ASCII include conversion/query; no target API call'
            elif name.startswith('observed_win32_handle:'):
                value=int(name.split(':',1)[1],0)
                event['model']='No-argument Win32 handle getter; result copied from explicit read-only UI focus observation, not a live target call'
            elif name.startswith('shared_cursors:'):
                cursors = {int(k, 0): int(v, 0) for k, v in json.loads(name.split(':', 1)[1]).items()}
                if values[1] != 0 or values[2] not in cursors:
                    result['stop_reason'] = 'unexpected cursor resource arguments'
                    cpu.emu_stop()
                    return
                value = cursors[values[2]]
                event['result'] = hex(value)
                extra_pop = 8
            elif name == 'verified_ascii_wstring_to_narrow':
                out, source = values[1:3]
                descriptor = bytes(cpu.mem_read(source, 24))
                length, capacity = struct.unpack_from('<II', descriptor, 16)
                text_address = source if capacity < 8 else struct.unpack_from('<I', descriptor)[0]
                text = bytes(cpu.mem_read(text_address, min(length, 1025) * 2))
                ascii_only = length <= 1024 and all(text[i] < 128 and text[i + 1] == 0 for i in range(0, len(text), 2))
                if not ascii_only:
                    event['model'] = 'native conversion; outside validated ASCII subset'
                    return
                expected = text[::2]
                if len(ascii_validated) < 3:
                    ascii_pending.append({'return': values[0], 'sp': sp, 'out': out, 'expected': expected})
                    event['model'] = 'native reference conversion for validation'
                    return
                value = out
                if length < 16:
                    representation = expected + bytes(16 - length)
                    output_capacity = 15
                else:
                    output_capacity = length | 15
                    pointer = heap_next[0]
                    heap_next[0] += (output_capacity + 16) & ~15
                    allocations[pointer] = output_capacity + 1
                    if heap_next[0] >= 0x24000000:
                        result['stop_reason'] = 'synthetic conversion allocation bound'
                        cpu.emu_stop()
                        return
                    cpu.mem_write(pointer, expected + b'\0')
                    representation = struct.pack('<I', pointer) + bytes(12)
                cpu.mem_write(out, representation + struct.pack('<II', length, output_capacity))
                event['model'] = 'ASCII semantics after three distinct native reference conversions'
                event['text'] = expected.hex()
            elif name == 'synthetic_hook_VirtualAlloc':
                requested, length, allocation, protection = values[1:]
                if requested or not 0 < length <= 0x100000 or allocation != 0x1000 or protection != 0x40:
                    result['stop_reason'] = 'unsupported synthetic hook allocation request'
                    cpu.emu_stop()
                    return
                value = virtual_next[0]
                size = (length + 4095) & ~4095
                virtual_next[0] += size
                cpu.mem_map(value, size)
                virtual_allocations[value] = size
                event.update(model='synthetic private emulator pages only, no target allocation', result=hex(value))
                extra_pop = 16
            elif name == 'synthetic_hook_VirtualProtect':
                target, length, protection, old_out = values[1:]
                if not target or not 0 < length <= 0x100000 or protection not in (0x20, 0x40):
                    result['stop_reason'] = 'unsupported synthetic hook protection request'
                    cpu.emu_stop()
                    return
                if not any(lo <= target and target + length <= hi for lo, hi in observed_write_ranges) and not any(
                        lo <= target and target + length <= lo + size for lo, size in virtual_allocations.items()):
                    result['stop_reason'] = 'synthetic protection request outside known game/emulator allocations'
                    cpu.emu_stop()
                    return
                cpu.mem_write(old_out, struct.pack('<I', 0x20))
                event['model'] = 'hypothetical successful protection and previous RX; emulator only, no VirtualProtect call'
                value = 1
                extra_pop = 16
            elif name == 'synthetic_hook_FlushInstructionCache':
                if values[1] != 0xFFFFFFFF:
                    result['stop_reason'] = 'unexpected instruction-cache target process'
                    cpu.emu_stop()
                    return
                event['model'] = 'emulator-only cache operation; no target API call'
                value = 1
                extra_pop = 12
            elif name.startswith('verified_masked_scan:'):
                from masked_scan11_model import find_offset, PROFILES
                helper_rva = address - BASE
                width, expected_hash = PROFILES[helper_rva]
                helper_code = bytes(cpu.mem_read(address, 0x8C))
                if hashlib.sha256(helper_code).hexdigest() != expected_hash:
                    result['stop_reason'] = 'native masked scan byte profile mismatch'
                    cpu.emu_stop()
                    return
                output, start, length = values[1:4]
                if length > 0x4000000 or start < 11 or start + length > 0xFFFFFFFF:
                    result['stop_reason'] = 'native scan11 model input/wrap bound'
                    cpu.emu_stop()
                    return
                pattern_address = cpu.reg_read(UC_X86_REG_ECX)
                mask_offset = (width + 3) & ~3
                pattern = bytes(cpu.mem_read(pattern_address, mask_offset + 4))
                copied_pages = 0
                for page in range(start & ~4095, (start + length + 4095) & ~4095, 4096):
                    try:
                        cpu.mem_read(page, 4096)
                    except UcError:
                        try:
                            data = process.read(page, 4096)
                        except OSError as error:
                            result['stop_reason'] = 'native scan11 source page unreadable; no fabricated page'
                            event['page_error'] = str(error)
                            cpu.emu_stop()
                            return
                        cpu.mem_map(page, 4096)
                        cpu.mem_write(page, data)
                        copied_pages += 1
                data = bytes(cpu.mem_read(start, length))
                mask = struct.unpack_from('<I', pattern, mask_offset)[0]
                search_pattern = pattern[:width]
                adaptation = (signature_adaptations or {}).get(search_pattern.hex())
                if adaptation is not None:
                    if mask or not adaptation['range_start'] <= start < start + length <= adaptation['range_end'] + 1:
                        result['stop_reason'] = 'event signature adaptation range/mask profile mismatch'
                        cpu.emu_stop()
                        return
                    search_pattern = bytes.fromhex(adaptation['replacement_pattern'])
                    event['offline_signature_experiment'] = adaptation
                offset = find_offset(data, search_pattern, mask)
                match = 0 if offset is None else start + offset
                if adaptation is not None and match not in (0, adaptation['expected_target']):
                    result['stop_reason'] = 'event signature alternative matched an unexpected target'
                    cpu.emu_stop()
                    return
                cpu.mem_write(output, struct.pack('<I', match))
                value = output
                extra_pop = 12
                event.update(model='exact pure-helper semantics;954 original-x86 differential cases passed across9 profiles',
                             start=hex(start), length=length, pattern=pattern[:width].hex(), mask=hex(mask),
                             match=hex(match), source_sha256=hashlib.sha256(data).hexdigest(),
                             copied_read_only_target_pages=copied_pages)
            elif name == 'synthetic_gdi_embedded_font_registration':
                font, size, reserved, count_out = values[1:]
                expected_sizes = {BASE + 0x4B3D58: 0x3500, BASE + 0x4B7258: 0x23EA4}
                if expected_sizes.get(font) != size or reserved:
                    result['stop_reason'] = 'unexpected synthetic embedded font registration arguments'
                    cpu.emu_stop()
                    return
                data = bytes(cpu.mem_read(font, size))
                event['asset_sha256'] = hashlib.sha256(data).hexdigest()
                event['model'] = 'hypothetical successful AddFontMemResourceEx; no OS registration'
                cpu.mem_write(count_out, struct.pack('<I', 1))
                value = 0x7200 + sum(call['name'] == name for call in result['modeled_standard_calls'])
                extra_pop = 16
            elif name.startswith('synthetic_source_screen_size:'):
                width, height = map(int, name.split(':')[1:])
                cpu.mem_write(values[1], struct.pack('<I', width))
                cpu.mem_write(values[2], struct.pack('<I', height))
                event['model'] = 'offline screen query; copied window dimensions, no engine call'
                extra_pop = 8
                value = 0
            elif name == 'synthetic_source_create_font':
                value = 0x7100 + sum(call['name'] == name for call in result['modeled_standard_calls'])
                event['model'] = 'synthetic font identifier; no real font created'
                event['result'] = hex(value)
            elif name == 'synthetic_source_set_font_glyph':
                args = struct.unpack('<9I', bytes(cpu.mem_read(sp + 4, 36)))
                text = bytearray()
                for offset in range(256):
                    char = bytes(cpu.mem_read(args[1] + offset, 1))
                    if char == b'\0':
                        break
                    text.extend(char)
                else:
                    result['stop_reason'] = 'synthetic glyph face length bound'
                    cpu.emu_stop()
                    return
                event['glyph_arguments'] = list(args)
                event['face'] = text.decode('ascii', errors='backslashreplace')
                event['model'] = 'hypothetical successful glyph setup; no installed glyph result'
                extra_pop = 36
                value = 1
            elif name == 'synthetic_source_text_size':
                # Explicit construction-only experiment. These are NOT measured
                # target glyph metrics or a production renderer implementation.
                text = values[2]
                length = 0
                while length < 65536 and bytes(cpu.mem_read(text + length * 2, 2)) != b'\0\0':
                    length += 1
                if length == 65536:
                    result['stop_reason'] = 'synthetic text metric bound'
                    cpu.emu_stop()
                    return
                cpu.mem_write(values[3], struct.pack('<I', length * 7))
                cpu.mem_write(values[4], struct.pack('<I', 14 if length else 0))
                event['model'] = 'synthetic constant metrics, construction path only'
                extra_pop = 16
                value = 0
            elif name == 'get_last_error':
                value = last_error[0]
            elif name == 'set_last_error':
                last_error[0] = values[1]
                value = 0
                extra_pop = 4
            elif name == 'single_thread_locale_lock':
                # The offline CPU has one thread and a private locale clone.
                # No target lock is entered and no concurrent locale writer exists.
                value = 0
                event['model'] = 'single-thread emulator lock elision'
            elif name == 'rand':
                random_state[0] = (random_state[0] * 214013 + 2531011) & 0xFFFFFFFF
                value = (random_state[0] >> 16) & 0x7FFF
                event['deterministic_seed_initial'] = 1
                event['result'] = hex(value)
            elif name.startswith('locale_init_existing:'):
                slot = int(name.split(':', 1)[1], 0)
                value = struct.unpack('<I', process.read(slot, 4))[0]
                if not value:
                    result['stop_reason'] = 'existing locale singleton is absent; no initialization invented'
                    cpu.emu_stop()
                    return
                try:
                    cpu.mem_read(value, 8)
                except UcError:
                    data = process.read(value & ~4095, 4096)
                    cpu.mem_map(value & ~4095, 4096)
                    cpu.mem_write(value & ~4095, data)
                vtable = struct.unpack('<I', bytes(cpu.mem_read(value, 4)))[0]
                increment = struct.unpack('<I', process.read(vtable + 4, 4))[0]
                if process.read(increment, 5) != bytes.fromhex('f0ff4104c3'):
                    result['stop_reason'] = 'existing locale refcount method byte guard mismatch'
                    cpu.emu_stop()
                    return
                if values[1] & 255:
                    count = struct.unpack('<I', bytes(cpu.mem_read(value + 4, 4)))[0]
                    cpu.mem_write(value + 4, struct.pack('<I', (count + 1) & 0xFFFFFFFF))
                event['result'] = hex(value)
                event['local_clone_refcount_increment'] = bool(values[1] & 255)
            elif name == 'codecvt_wchar_ctor_c_locale':
                # Verified installed constructor writes this vtable/refcount,
                # enters _Locinfo("C"), copies the44-byte _Getcvt result, then
                # restores locale and RET4. The C state has page0/max1/isC1.
                if process.read(0x56302D30, 2) != b'C\0' or process.read(0x5630CB03, 3) != bytes.fromhex('c20400'):
                    result['stop_reason'] = 'codecvt constructor byte/profile guard mismatch'
                    cpu.emu_stop()
                    return
                value = cpu.reg_read(UC_X86_REG_ECX)
                cvt = struct.pack('<11I', 0, 1, 1, *([0] * 8))
                cpu.mem_write(value, struct.pack('<II', 0x56302DD0, values[1]) + cvt)
                event['result'] = hex(value)
                event['constructed_bytes'] = bytes(cpu.mem_read(value, 52)).hex()
                extra_pop = 4
            elif name == 'free':
                pointer = values[1]
                if pointer and pointer not in allocations:
                    if any(lo <= address < hi for lo, hi in external_code_ranges):
                        event['model'] = 'interpret original CRT free on offline-cloned native heap'
                        return
                    result['stop_reason'] = 'free outside synthetic heap and no allowed native CRT'
                    cpu.emu_stop()
                    return
                if pointer:
                    del allocations[pointer]
                value = 0
            elif name == 'strlen':
                pointer = values[1]
                value = 0
                while value < 0x100000 and bytes(cpu.mem_read(pointer + value, 1)) != b'\0':
                    value += 1
                if value == 0x100000:
                    result['stop_reason'] = 'modeled strlen bound'
                    cpu.emu_stop()
                    return
                event['result'] = hex(value)
            elif name in ('malloc', 'realloc', 'calloc'):
                pointer = values[1] if name == 'realloc' else 0
                length = values[1] * values[2] if name == 'calloc' else values[2] if name == 'realloc' else values[1]
                if pointer and pointer not in allocations:
                    result['stop_reason'] = 'realloc of pointer outside synthetic allocation model'
                    cpu.emu_stop()
                    return
                if length > 0x1000000 or heap_next[0] + max(length, 1) > 0x24000000:
                    result['stop_reason'] = 'synthetic heap size bound'
                    cpu.emu_stop()
                    return
                value = heap_next[0]
                heap_next[0] += (max(length, 1) + 15) & ~15
                allocations[value] = length
                if name == 'calloc' and length:
                    cpu.mem_write(value, bytes(length))
                if pointer:
                    cpu.mem_write(value, bytes(cpu.mem_read(pointer, min(allocations[pointer], length))))
                    del allocations[pointer]
                if name == 'realloc' and length == 0:
                    del allocations[value]
                    value = 0
                event['result'] = hex(value)
            else:
                destination, source_or_byte, length = values[1:4]
                if length > 0x800000:
                    result['stop_reason'] = 'modeled copy size bound'
                    cpu.emu_stop()
                    return
                # Host-side Unicorn reads do not invoke demand-page hooks. Copy
                # missing SOURCE pages from the read-only process snapshot;
                # never fabricate bytes for a current native allocation.
                if name != 'memset' and length:
                    for page in range(source_or_byte & ~4095, (source_or_byte + length + 4095) & ~4095, 4096):
                        try:
                            cpu.mem_read(page, 1)
                        except UcError:
                            block = process.read(page, 4096)
                            cpu.mem_map(page, 4096)
                            cpu.mem_write(page, block)
                            result['external_page_reads'].append(dict(address=hex(page), model_source_page=True, copied_sha256=hashlib.sha256(block).hexdigest()))
                data = bytes([source_or_byte & 255]) * length if name == 'memset' else bytes(cpu.mem_read(source_or_byte, length))
                cpu.mem_write(destination, data)
                value = destination
                event['data_prefix'] = data[:64].hex()
            cpu.reg_write(UC_X86_REG_EAX, value)
            cpu.reg_write(UC_X86_REG_ESP, sp + 4 + extra_pop)
            cpu.reg_write(UC_X86_REG_EIP, values[0])
            return
        if cpuid_pending[0]:
            if cpuid_override is not None:
                for name, register in [('eax', UC_X86_REG_EAX), ('ebx', UC_X86_REG_EBX),
                                       ('ecx', UC_X86_REG_ECX), ('edx', UC_X86_REG_EDX)]:
                    cpu.reg_write(register, cpuid_override.get(name, 0))
            result['cpuid'][-1]['result'] = {
                'eax': hex(cpu.reg_read(UC_X86_REG_EAX)),
                'ebx': hex(cpu.reg_read(UC_X86_REG_EBX)),
                'ecx': hex(cpu.reg_read(UC_X86_REG_ECX)),
                'edx': hex(cpu.reg_read(UC_X86_REG_EDX))}
            cpuid_pending[0] = False
        if address == BASE + marker:
            result['stop_reason'] = 'native marker reached'
            cpu.emu_stop()
        elif not BASE <= address < BASE + len(image) and not any(lo <= address < hi for lo, hi in external_code_ranges):
            result['stop_reason'] = 'external code boundary'
            cpu.emu_stop()
        elif bytes(cpu.mem_read(address, min(size, 15))) == b'\x0f\xa2':
            result['cpuid'].append({'rva': hex(address - BASE),
                                    'eax_before': hex(cpu.reg_read(UC_X86_REG_EAX))})
            cpuid_pending[0] = True
        if BASE + 0x1000 <= address < BASE + 0x40D000:
            raw = bytes(cpu.mem_read(address, min(size, 15)))
            if raw[0] == 0xE8 or (raw[0] == 0xFF and len(raw) > 1 and (raw[1] >> 3) & 7 == 2):
                item = next(decoder.disasm(raw, address), None)
                result['native_calls'].append({
                    'rva': hex(address - BASE),
                    'instruction': None if item is None else item.mnemonic + ' ' + item.op_str,
                    'ecx': hex(cpu.reg_read(UC_X86_REG_ECX)),
                    'edx': hex(cpu.reg_read(UC_X86_REG_EDX)),
                    'esp': hex(cpu.reg_read(UC_X86_REG_ESP)),
                    'stack_first_32': bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP), 32)).hex()})

    def on_read(cpu, access, address, size, value, _):
        if reject_null_reads and address < 4096:
            current = next(decoder.disasm(bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_EIP), 15)), cpu.reg_read(UC_X86_REG_EIP)), None)
            if current is not None and 'fs:' not in current.op_str:
                result['stop_reason'] = 'ordinary null-page read; synthetic TEB is only valid for FS reads'
                result['null_read'] = {'address': hex(address), 'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                                       'instruction': current.mnemonic + ' ' + current.op_str}
                cpu.emu_stop()
                return
        try:
            actual = int.from_bytes(cpu.mem_read(address, size), 'little')
        except UcError:
            actual = None
        record = {'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                  'address': hex(address), 'size': size,
                  'value': None if actual is None else hex(actual)}
        reads.append(record)
        if BASE + 0x4B2000 <= address < BASE + 0x826654:
            image_data_reads.append(record)
        if 0x10010004 <= address < 0x10010100:
            result['original_stack_argument_reads'].append(record)
        if 0x10011000 <= address < 0x10012000:
            result['incoming_register_buffer_accesses'].append({'kind': 'read', **record})
        if BASE <= address < BASE + len(image) and BASE + 0x1000 <= cpu.reg_read(UC_X86_REG_EIP) < BASE + 0x40D000:
            result['native_image_reads'].append(record)
        if address == BASE + 0x91BC4B:
            result['loader_pointer_reads'].append(record)
        if synthetic_fields is not None and 0x1C85C000 <= address < 0x1C85E000:
            result['synthetic_field_reads'].append(record)
            modeled_prefix_active[0] = True

    def on_write(cpu, access, address, size, value, _):
        if len(result['feature_global_writes']) < 4096 and (
                BASE+0x4DE150 <= address < BASE+0x4DE260 or
                BASE+0x807300 <= address < BASE+0x807E80):
            result['feature_global_writes'].append(dict(address=hex(address),size=size,value=hex(value),eip_rva=hex(cpu.reg_read(UC_X86_REG_EIP)-BASE)))
        if len(result['external_code_writes']) < 512 and any(lo <= address < hi for lo, hi in observed_write_ranges):
            result['external_code_writes'].append({'address': hex(address), 'size': size,
                'value': hex(value), 'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                'previous_bytes': bytes(cpu.mem_read(address, size)).hex()})
        if any(address < font + 12 and font < address + size for font in font_records):
            result['font_record_writes'].append({'address': hex(address), 'size': size,
                'value': hex(value), 'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE)})
        # Keep the zero-based FS model and its actual TEB pointer alias coherent.
        if size <= 8 and (0 <= address < 4096 or teb <= address < teb + 4096):
            alias = teb + address if address < 4096 else address - teb
            cpu.mem_write(alias, int(value & ((1 << (size * 8)) - 1)).to_bytes(size, 'little'))
        if 0x10011000 <= address < 0x10012000:
            result['incoming_register_buffer_accesses'].append({
                'kind': 'write', 'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                'address': hex(address), 'size': size, 'value': hex(value)})
        if modeled_prefix_active[0] and 0x1000FFC0 <= address < 0x10010000:
            result['modeled_prefix_stack_writes'].append({
                'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                'address': hex(address), 'size': size, 'value': hex(value)})
        if value & 0xFFFF0000 == 0x1C850000:
            result['pointer_value_writes'].append({
                'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                'address': hex(address), 'size': size, 'value': hex(value),
                'preceding_reads': list(reads)[-12:]})

    def invalid(cpu, access, address, size, value, _):
        instruction = None
        if access != UC_MEM_FETCH_UNMAPPED:
            instruction = next(decoder.disasm(bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_EIP), 15)), cpu.reg_read(UC_X86_REG_EIP)), None)
        record = {'address': hex(address), 'access': access,
                  'eip_rva': hex(cpu.reg_read(UC_X86_REG_EIP) - BASE),
                  'instruction': None if instruction is None else instruction.mnemonic + ' ' + instruction.op_str}
        result['external_page_reads'].append(record)
        if access == UC_MEM_FETCH_UNMAPPED and not any(lo <= address < hi for lo, hi in external_code_ranges):
            result['stop_reason'] = 'unmapped external code boundary'
            return False
        try:
            data = process.read(address & ~4095, 4096)
        except OSError as error:
            record['target_read_error'] = str(error)
            result['stop_reason'] = 'target page unreadable; no fabricated page'
            return False
        cpu.mem_map(address & ~4095, 4096)
        cpu.mem_write(address & ~4095, data)
        record['copied_sha256'] = hashlib.sha256(data).hexdigest()
        return True

    cpu.hook_add(UC_HOOK_CODE, on_code)
    cpu.hook_add(UC_HOOK_MEM_READ, on_read)
    cpu.hook_add(UC_HOOK_MEM_WRITE, on_write)
    cpu.hook_add(UC_HOOK_MEM_INVALID, invalid)
    try:
        cpu.emu_start(BASE + entry, 0, count=step_budget)
    except UcError as error:
        result['emulator_error'] = str(error)
    result['last_eip_rva'] = hex(cpu.reg_read(UC_X86_REG_EIP) - BASE)
    result['final_eax'] = hex(cpu.reg_read(UC_X86_REG_EAX))
    result['final_esp'] = hex(cpu.reg_read(UC_X86_REG_ESP))
    try:
        final_stack = bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP), 64))
        result['final_stack_words'] = [hex(v) for v in struct.unpack('<16I', final_stack)]
        result['final_stack_pointer_samples'] = {}
        for pointer in struct.unpack('<16I', final_stack):
            try:
                result['final_stack_pointer_samples'][hex(pointer)] = bytes(cpu.mem_read(pointer, 64)).hex()
            except UcError:
                pass
    except UcError:
        pass
    if cpu.reg_read(UC_X86_REG_EIP) == 0:
        result['stop_reason'] = 'control transferred to zero (emulator end boundary)'
    elif result['instructions'] >= step_budget and not result.get('stop_reason'):
        result['stop_reason'] = 'instruction budget reached; incomplete path'
    result['last_code_rvas'] = list(code)
    result['last_reads'] = list(reads)
    result['last_image_data_reads'] = list(image_data_reads)
    result['final_font_records'] = {hex(font): list(struct.unpack('<3I', bytes(cpu.mem_read(font, 12))))
                                  for font in font_records}
    if cpu.reg_read(UC_X86_REG_EIP) == 0x12345678 and cpu.reg_read(UC_X86_REG_ESP) == 0x10010004:
        result['stop_reason'] = 'returned to caller sentinel with balanced cdecl stack'
        result['emulator_error'] = None
        result['constructed_menu'] = {}
        for label, address, size in [('windows', 0x44B44E5C, 12),
                                     ('font_body', 0x448C0F40, 12),
                                     ('font_small', 0x44B45430, 12),
                                     ('font_header', 0x448C0F34, 12)]:
            result['constructed_menu'][label] = bytes(cpu.mem_read(address, size)).hex()
        begin, end, capacity = struct.unpack('<3I', bytes(cpu.mem_read(0x44B44E5C, 12)))
        if begin and begin <= end <= capacity and end - begin <= 4096 and not (end - begin) % 4:
            try:
                windows = struct.unpack('<' + 'I' * ((end - begin) // 4), bytes(cpu.mem_read(begin, end - begin)))
                result['constructed_menu']['window_records'] = {
                    hex(a): bytes(cpu.mem_read(a, 0xA0)).hex() for a in windows}
            except UcError:
                result['constructed_menu']['window_records_note'] = 'Not touched/mapped by this replay; no diagnostic read fabricated.'
        if snapshot_output:
            regions = []
            with zipfile.ZipFile(snapshot_output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
                for lo, hi, permissions in cpu.mem_regions():
                    name = f'{lo:08x}.bin'
                    archive.writestr(name, bytes(cpu.mem_read(lo, hi - lo + 1)))
                    regions.append({'start': lo, 'size': hi - lo + 1,
                                    'permissions': permissions, 'member': name})
                archive.writestr('manifest.json', json.dumps({
                    'purpose': 'Offline Unicorn state only; synthetic heap, interfaces and CPU guard',
                    'regions': regions, 'source_image_sha256': hashlib.sha256(image).hexdigest(),
                    'standard_models': standard_models, 'external_code_ranges': external_code_ranges,
                    'result': result['constructed_menu']}))
            result['offline_snapshot'] = str(snapshot_output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--teb', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    image = args.image.read_bytes()
    result = {'mode': 'OFFLINE EMULATION with read-only target page copies',
              'image': str(args.image), 'image_sha256': hashlib.sha256(image).hexdigest(),
              'pid': args.pid, 'teb_source': hex(args.teb), 'runtime_base': hex(BASE),
              'limitations': ['Synthetic general registers and stack; no call ABI established.',
                              'Unicorn CPU model, not target CPUID identity.',
                              'No target execution; no evidence these roots were reached by target startup.',
                              'Target memory pages are non-atomic read-only snapshots.']}
    with Process(args.pid) as process:
        result['probes'] = [probe(process, image, args.teb, entry, marker)
                            for entry, marker in ROOTS]
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps([{'entry': p['entry_rva'], 'last_eip': p['last_eip_rva'],
                       'instructions': p['instructions'], 'cpuid': p['cpuid'],
                       'stop': p.get('stop_reason')} for p in result['probes']], indent=2))


if __name__ == '__main__':
    main()
