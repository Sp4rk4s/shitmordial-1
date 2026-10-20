"""Offline continuation of retained menu-constructor experiment, never a game call.

Copies current process pages read-only into Unicorn. Models the verified CRT
boundaries from the preceding experiment and optionally interprets copied
MSVCP140 machine code. All heap/refcount writes remain inside the emulator.
"""
import hashlib
import json
from pathlib import Path
import struct
import ctypes as ctypes
import argparse
import protected_entry_probe as probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', required=True, type=int)
    parser.add_argument('--entry', type=lambda value: int(value, 0), default=0x2E04D0,
                        choices=[0x2E04D0, 0x2CAB40, 0x360080, 0x36E110, 0xB5320, 0xE2030,
                                 0x162FA0, 0x37BF70, 0x3542C0, 0x352B80, 0x35AEE0, 0x1564B0])
    parser.add_argument('--max-steps', type=int, default=50000000)
    parser.add_argument('--model-empty-config', action='store_true',
                        help='Explicit virtual directory creation only; no real filesystem writes')
    parser.add_argument('--interpret-source-rng', action='store_true',
                        help='Interpret read-only copies of installed vstdlib/tier0 code; no target calls')
    parser.add_argument('--model-renderer-setup', action='store_true',
                        help='Offline screen/font models and synthetic OLD language-pointer layout; never live bindings')
    parser.add_argument('--model-native-scan11', '--model-native-scans', action='store_true',
                        help='Exact offline models of nine byte-guarded native masked scan helpers')
    parser.add_argument('--model-hook-memory', action='store_true',
                        help='Explicit offline allocation/protection success models; all hook writes stay in Unicorn')
    parser.add_argument('--adapt-event-signature', action='store_true',
                        help='Offline exact old/new event-dispatch signature experiment, guarded by current API-wrapper evidence')
    parser.add_argument('--experiment-captured-hook-pair', action='store_true',
                        help='Offline restore of one captured pair, stopping BEFORE dereferencing its decoded stale address')
    args = parser.parse_args()
    kind = {0x2E04D0: 'menu', 0x2CAB40: 'config'}.get(args.entry, f'candidate-{args.entry:06x}')
    root = Path(__file__).resolve().parents[2]
    previous = json.loads((root / 'analysis/menu/menu-constructor-codecvt-model-21728.json').read_text())
    with probe.Process(args.pid) as process:
        image = process.read(0x44340000, 0x140C000)
    image_sha256 = hashlib.sha256(image).hexdigest()
    if not kind.startswith('candidate-'):
        (root / f'analysis/menu/runtime-{args.pid}-prepared-image.bin').write_bytes(image)
    # Experimental correction confined to this emulator input: the verified
    # menu title move assignment would free captured old-process heap43E85588.
    image = bytearray(image)
    decoder_stop_targets = ()
    if args.experiment_captured_hook_pair:
        assert args.adapt_event_signature and args.entry == 0x1564B0
        original = (root / 'primordial.bin').read_bytes()
        assert hashlib.sha256(original).hexdigest() == 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
        assert bytes(image[0x8074F0:0x8074F8]) == bytes(8)
        captured = bytes.fromhex('9aaf012d5afcce00')
        assert original[0x8074F0:0x8074F8] == captured
        image[0x8074F0:0x8074F8] = captured
        decoder_stop_targets = (0x2DCF53C0,)
    assert image[0x4B33B4:0x4B33CC] in (bytes.fromhex('8855e8430000000000000000000000000a0000000f000000'), bytes(20) + struct.pack('<I', 7))
    image[0x4B33B4:0x4B33CC] = bytes(20) + struct.pack('<I', 7)
    assert image[0x4B3484:0x4B3490] in (bytes.fromhex('703b4341e83b4341e83b4341'), bytes(12))
    image[0x4B3484:0x4B3490] = bytes(12)
    # Synthetic Source interface only in Unicorn, to explore constructor-owned
    # memory. Uniform metrics do not establish real font appearance/layout.
    struct.pack_into('<I', image, 0x4DE15C, 0x26000000)
    page = bytearray(4096)
    struct.pack_into('<I', page, 0, 0x26000080)
    struct.pack_into('<I', page, 0x80 + 79 * 4, 0x26001000)
    models = {int(call['address'], 16): call['name'] for call in previous['modeled_standard_calls']}
    models[0x26001000] = 'synthetic_source_text_size'
    models[0x44340000 + 0xC67F0] = 'verified_ascii_wstring_to_narrow'
    if args.model_native_scan11:
        from masked_scan11_model import PROFILES
        for rva, (width, digest) in PROFILES.items():
            models[0x44340000 + rva] = f'verified_masked_scan:{width}'
    synthetic_pages = {0x26000000: bytes(page), 0x26001000: b'\xc3' + bytes(4095)}
    if args.model_renderer_setup:
        assert args.entry == 0x360080
        facts = json.loads((root / f'analysis/menu/current-render-api-facts-{args.pid}.json').read_text())
        window = next(w for w in facts['visible_game_windows'] if 'Direct3D 9' in w['title'])
        struct.pack_into('<I', image, 0x4DE174, 0x26003000)
        struct.pack_into('<I', image, 0x4DE1E8, 0x26002000)
        engine = bytearray(4096)
        struct.pack_into('<I', engine, 0, 0x26003080)
        struct.pack_into('<I', engine, 0x80 + 5 * 4, 0x26004000)
        struct.pack_into('<I', page, 0x80 + 71 * 4, 0x26001100)
        struct.pack_into('<I', page, 0x80 + 72 * 4, 0x26001200)
        synthetic_pages.update({0x26000000: bytes(page), 0x26002000: bytes(4096),
                                0x26003000: bytes(engine), 0x26004000: b'\xc3' + bytes(4095)})
        models[0x26004000] = f"synthetic_source_screen_size:{window['width']}:{window['height']}"
        models[0x26001100] = 'synthetic_source_create_font'
        models[0x26001200] = 'synthetic_source_set_font_glyph'
    with probe.Process(args.pid) as process:
        threads = json.loads((root / f'analysis/menu/threads-{args.pid}.json').read_text())['threads']
        teb = int(threads[0]['teb'], 0)
        assert struct.unpack('<I', process.read(teb + 0x18, 4))[0] == teb
        # Verify identities directly against the current x86 UCRT export table.
        base = 0x75800000
        header = process.read(base, 4096)
        pe = struct.unpack_from('<I', header, 60)[0]
        directory = struct.unpack_from('<I', header, pe + 24 + 96)[0]
        nf, nn, af, an, ao = struct.unpack_from('<5I', process.read(base + directory, 40), 20)
        functions = struct.unpack('<' + 'I' * nf, process.read(base + af, nf * 4))
        names = struct.unpack('<' + 'I' * nn, process.read(base + an, nn * 4))
        ordinals = struct.unpack('<' + 'H' * nn, process.read(base + ao, nn * 2))
        kernel_exports = {}
        for i, name_rva in enumerate(names):
            name = process.read(base + name_rva, 128).split(b'\0')[0].decode('ascii')
            if name in ('_lock_locales', '_unlock_locales'):
                models[base + functions[ordinals[i]]] = 'single_thread_locale_lock'
            if name in ('malloc', '_malloc_base', 'realloc', '_realloc_base',
                        'calloc', '_calloc_base', 'free', '_free_base'):
                models[base + functions[ordinals[i]]] = name.removeprefix('_').removesuffix('_base')
        modules = json.loads((root / f'analysis/menu/render-state-{args.pid}.json').read_text())['modules']
        exports = json.loads((root / f'analysis/menu/runtime-standard-exports-{args.pid}.json').read_text())['exports']
        # This reused model came from an older process. VCRuntime can move even
        # when UCRT/MSVCP keep their bases; bind memcpy to the current named
        # export rather than treating the old process VA as a model endpoint.
        models = {address: name for address, name in models.items() if name != 'memcpy'}
        memcpy_exports = [e for e in exports if e['module'].lower() == 'vcruntime140.dll'
                          and e['name'] == 'memcpy' and not e['forwarded']]
        assert len(memcpy_exports) == 1, 'Current VCRuntime memcpy export is ambiguous/missing'
        models[int(memcpy_exports[0]['address'], 16)] = 'memcpy'
        observed_exports = {}
        adaptations = {}
        if args.adapt_event_signature:
            assert args.model_native_scan11 and args.model_hook_memory and args.entry == 0x1564B0
            fact = json.loads((root / f'analysis/menu/game-event-private-target-{args.pid}.json').read_text())
            for index in ('8', '9'):
                method = fact['methods'][index]
                expected = bytes.fromhex(method['bytes'])
                assert process.read(int(method['address'], 16), len(expected)) == expected
            target = int(fact['private_target'], 16)
            expected = bytes.fromhex(fact['private_prefix'])
            assert process.read(target, len(expected)) == expected
            assert fact['replacement_pattern_matches'] == [fact['private_target']]
            start = int(fact['engine_scan_start'], 16)
            adaptations[fact['old_pattern']] = {
                'range_start': start, 'range_end': start + fact['engine_scan_length'],
                'replacement_pattern': fact['replacement_pattern'], 'expected_target': target,
                'proof': f'game-event-private-target-{args.pid}.json',
                'scope': 'OFFLINE signature substitution only; public wrappers converge on same private target'}
        observed_write_ranges = [(m['base'], m['base'] + m['size']) for m in modules
                                 if Path(m['path']).name.lower() in
                                 ('client.dll', 'engine.dll', 'vgui2.dll', 'vguimatsurface.dll',
                                  'inputsystem.dll', 'materialsystem.dll', 'shaderapidx9.dll',
                                  'server.dll', 'studiorender.dll', 'vstdlib.dll', 'tier0.dll',
                                  'gameoverlayrenderer.dll')]
        for export in exports:
            if not export['forwarded']:
                observed_exports.setdefault(int(export['address'], 16), []).append(export['module'] + '!' + export['name'])
                if args.model_renderer_setup and export['name'] == 'AddFontMemResourceEx':
                    models[int(export['address'], 16)] = 'synthetic_gdi_embedded_font_registration'
                if args.model_hook_memory and export['module'].lower() in ('kernel32.dll', 'kernelbase.dll') and export['name'] in (
                        'VirtualAlloc', 'VirtualProtect', 'FlushInstructionCache'):
                    models[int(export['address'], 16)] = 'synthetic_hook_' + export['name']
        if args.model_renderer_setup:
            # The older standard-export inventory excludes GDI. Resolve the
            # exact current PE export read-only rather than hardcoding its VA.
            gdi = next(m['base'] for m in modules if Path(m['path']).name.lower() == 'gdi32.dll')
            header = process.read(gdi, 4096)
            pe = struct.unpack_from('<I', header, 60)[0]
            export_rva, export_size = struct.unpack_from('<II', header, pe + 24 + 96)
            nf, nn, af, an, ao = struct.unpack_from('<5I', process.read(gdi + export_rva, 40), 20)
            functions = struct.unpack('<' + 'I' * nf, process.read(gdi + af, nf * 4))
            names = struct.unpack('<' + 'I' * nn, process.read(gdi + an, nn * 4))
            ordinals = struct.unpack('<' + 'H' * nn, process.read(gdi + ao, nn * 2))
            for i, name_rva in enumerate(names):
                name = process.read(gdi + name_rva, 128).split(b'\0')[0].decode('ascii')
                if name != 'AddFontMemResourceEx':
                    continue
                function = functions[ordinals[i]]
                assert not export_rva <= function < export_rva + export_size, 'forwarded GDI export needs separate resolution'
                address = gdi + function
                assert (struct.unpack_from('<I', image, 0x9DB1AD)[0] + 0x0C3C6BBF) & 0xFFFFFFFF == address
                models[address] = 'synthetic_gdi_embedded_font_registration'
                observed_exports[address] = ['GDI32.dll!AddFontMemResourceEx']
        if args.model_empty_config:
            for export in exports:
                if export['name'] in ('CreateDirectoryW', 'FindFirstFileExW', 'FindNextFileW', 'FindClose') and not export['forwarded']:
                    models[int(export['address'], 16)] = 'virtual_empty_config_' + export['name']
                elif export['name'] in ('GetLastError', 'SetLastError') and not export['forwarded']:
                    models[int(export['address'], 16)] = {'GetLastError': 'get_last_error',
                                                         'SetLastError': 'set_last_error'}[export['name']]
        base = next(m['base'] for m in modules if Path(m['path']).name.lower() == 'kernelbase.dll')
        header = process.read(base, 4096)
        pe = struct.unpack_from('<I', header, 60)[0]
        directory = struct.unpack_from('<I', header, pe + 24 + 96)[0]
        nf, nn, af, an, ao = struct.unpack_from('<5I', process.read(base + directory, 40), 20)
        functions = struct.unpack('<' + 'I' * nf, process.read(base + af, nf * 4))
        names = struct.unpack('<' + 'I' * nn, process.read(base + an, nn * 4))
        ordinals = struct.unpack('<' + 'H' * nn, process.read(base + ao, nn * 2))
        for i, name_rva in enumerate(names):
            name = process.read(base + name_rva, 128).split(b'\0')[0].decode('ascii')
            kernel_exports[base + functions[ordinals[i]]] = name
        base = next(m['base'] for m in modules if Path(m['path']).name.lower() == 'user32.dll')
        header = process.read(base, 4096)
        pe = struct.unpack_from('<I', header, 60)[0]
        directory = struct.unpack_from('<I', header, pe + 24 + 96)[0]
        nf, nn, af, an, ao = struct.unpack_from('<5I', process.read(base + directory, 40), 20)
        functions = struct.unpack('<' + 'I' * nf, process.read(base + af, nf * 4))
        names = struct.unpack('<' + 'I' * nn, process.read(base + an, nn * 4))
        ordinals = struct.unpack('<' + 'H' * nn, process.read(base + ao, nn * 2))
        user32 = ctypes.WinDLL('user32')
        user32.LoadCursorW.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        user32.LoadCursorW.restype = ctypes.c_void_p
        # Query only OS-shared cursor resources; no SetCursor or target call.
        cursors = {hex(i): hex(user32.LoadCursorW(None, i) or 0)
                   for i in (0x7F00, 0x7F01, 0x7F02, 0x7F03, 0x7F04,
                             0x7F80, 0x7F81, 0x7F82, 0x7F83, 0x7F84,
                             0x7F86, 0x7F88, 0x7F89, 0x7F8A, 0x7F8B)}
        cursors = {k: v for k, v in cursors.items() if v != '0x0'}
        for i, name_rva in enumerate(names):
            name = process.read(base + name_rva, 128).split(b'\0')[0].decode('ascii')
            if name in ('LoadCursorA', 'LoadCursorW'):
                models[base + functions[ordinals[i]]] = 'shared_cursors:' + json.dumps(cursors)
        peb, = struct.unpack('<I', process.read(teb + 0x30, 4))
        build, = struct.unpack('<H', process.read(peb + 0xAC, 2))
        expected = (0x306C4 + 0x800 + (build << 7)) ^ 0x1C85CEC8
        fields = {0xB8: 0x59DB1540, 0x20: expected ^ 0x40DF21BC}
        result = probe.probe(process, bytes(image), teb, args.entry, 0, fields,
                             standard_models=models, step_budget=args.max_steps,
                             reject_null_reads=True,
                             synthetic_pages=synthetic_pages,
                             experiment_captured_destructors=True,
                             observed_exports=observed_exports,
                             observed_write_ranges=observed_write_ranges if args.model_hook_memory else (),
                             signature_adaptations=adaptations,
                             decoder_stop_targets=decoder_stop_targets,
                             snapshot_output=root / f'analysis/menu/{kind}-constructed-offline-{args.pid}.zip',
                             external_code_ranges=[(0x56300000, 0x56397000),
                                                   (0x75800000, 0x75920000)] +
                                [(m['base'], m['base'] + m['size']) for m in modules
                                 if Path(m['path']).name.lower() in ('kernelbase.dll', 'kernel32.dll', 'ntdll.dll', 'vcruntime140.dll') or
                                 (args.interpret_source_rng and Path(m['path']).name.lower() in ('vstdlib.dll', 'tier0.dll'))])
    result['experiment'] = 'OFFLINE copied MSVCP140 code; synthetic heap and guard state; no game writes/calls'
    result['source_pid'] = args.pid
    result['source_image_sha256'] = image_sha256
    suffix = (('-virtual-dirs' if args.model_empty_config else '') +
              ('-screen-font-model' if args.model_renderer_setup else '') +
              ('-scan11-model' if args.model_native_scan11 else '') +
              ('-hook-memory' if args.model_hook_memory else '') +
              ('-event-signature' if args.adapt_event_signature else '') +
              ('-captured-pair' if args.experiment_captured_hook_pair else ''))
    path = root / f'analysis/menu/{kind}-constructor-native-crt-{args.pid}{suffix}.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result.get(k) for k in ['instructions', 'stop_reason', 'last_eip_rva',
                                                'null_read', 'emulator_error', 'final_stack_words']}, indent=2))
    print('Modeled calls:', len(result['modeled_standard_calls']))


if __name__ == '__main__':
    main()
