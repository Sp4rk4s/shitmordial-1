"""Controlled synthetic GlobalData models for OFFLINE protected-prefix analysis.

This does not repair or execute the payload in the game. Target access is only
read-only. See protected_entry_probe.py for model limitations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import protected_entry_probe as probe

COUNT_OFFSET = 0xB8
COUNT_SALT = 0x59DB1541
HASH_OFFSET = 0x20
HASH_SALT = 0x40DF21BC
LOADER_POINTER = 0x1C85CEC8


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--teb', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    image = args.image.read_bytes()
    if struct.unpack_from('<I', image, 0x91BC4B)[0] != LOADER_POINTER:
        raise ValueError('This retained probe profile requires loader pointer1C85CEC8')
    with probe.Process(args.pid) as process:
        peb = struct.unpack_from('<I', process.read(args.teb, 4096), 0x30)[0]
        os_build = struct.unpack('<H', process.read(peb + 0xAC, 2))[0]
        zero = probe.probe(process, image, args.teb, 0x362970, 0x3629A0,
                           {COUNT_OFFSET: COUNT_SALT ^ 1, HASH_OFFSET: 0})
        cpu = zero['cpuid'][0]['result']
        eax, ebx = int(cpu['eax'], 16), int(cpu['ebx'], 16)
        if eax & 0xFF0 == 0xFE0:
            eax ^= 0x20
        expected = ((eax + (ebx & 0xFFFFFF) + (os_build << 7)) & 0xFFFFFFFF) ^ LOADER_POINTER
        encoded = expected ^ HASH_SALT
        fields = {COUNT_OFFSET: COUNT_SALT ^ 1, HASH_OFFSET: encoded}
        result = {
            'mode': 'CONTROLLED SYNTHETIC DATA IN OFFLINE EMULATOR ONLY',
            'image': str(args.image), 'sha256': hashlib.sha256(image).hexdigest(),
            'pid': args.pid, 'teb': hex(args.teb), 'peb': hex(peb),
            'os_build': hex(os_build), 'emulator_cpuid_leaf1': cpu,
            'contract': {'loader_pointer': hex(LOADER_POINTER),
                         'cpu_count_offset': hex(COUNT_OFFSET),
                         'cpu_count_xor_salt': hex(COUNT_SALT),
                         'cpu_hash_offset': hex(HASH_OFFSET),
                         'cpu_hash_stride': 4, 'cpu_hash_xor_salt': hex(HASH_SALT),
                         'model_expected_cpu_hash': hex(expected),
                         'model_encoded_hash': hex(encoded)},
            'limitations': ['Unicorn CPU identity, synthetic registers/stack/arguments.',
                            'Does not establish actual bootstrap caller or target call ABI.',
                            'Does not establish complete GlobalData layout or lifetime.',
                            'No live target writes, calls, debugger actions or repair.']}
        zero['case'] = 'one_wrong'
        result['worker_predicates'] = [zero]
        cases = [
            ('one_match', fields),
            ('one_second_match_unreachable', {COUNT_OFFSET: COUNT_SALT ^ 1, HASH_OFFSET: 0, HASH_OFFSET + 4: encoded}),
            ('two_second_match', {COUNT_OFFSET: COUNT_SALT ^ 2, HASH_OFFSET: 0, HASH_OFFSET + 4: encoded}),
            ('two_wrong', {COUNT_OFFSET: COUNT_SALT ^ 2, HASH_OFFSET: 0, HASH_OFFSET + 4: 0}),
            ('one_bit_mismatch', {COUNT_OFFSET: COUNT_SALT ^ 1, HASH_OFFSET: encoded ^ 1}),
        ]
        for label, model in cases:
            item = probe.probe(process, image, args.teb, 0x362970, 0x3629A0, model)
            item['case'] = label
            result['worker_predicates'].append(item)
        result['root_marker_prefixes'] = [
            probe.probe(process, image, args.teb, root, marker, fields)
            for root, marker in probe.ROOTS]
        result['root_first_external_boundaries'] = [
            probe.probe(process, image, args.teb, root, 0, fields)
            for root, _ in probe.ROOTS]
        result['synthetic_cpuid_predicates'] = []
        for label, input_eax, input_ebx in [
                ('mask_high_ebx', 0x306C4, 0xFF000800),
                ('other_cpu', 0x906EA, 0x7F123456),
                ('athlon_quirk', 0xFE0, 0x55000700),
                ('athlon_without_quirk_negative', 0xFE0, 0x55000700)]:
            adjusted = input_eax
            if adjusted & 0xFF0 == 0xFE0 and label != 'athlon_without_quirk_negative':
                adjusted ^= 0x20
            model_hash = ((adjusted + (input_ebx & 0xFFFFFF) + (os_build << 7)) & 0xFFFFFFFF) ^ LOADER_POINTER
            model = {COUNT_OFFSET: COUNT_SALT ^ 1, HASH_OFFSET: model_hash ^ HASH_SALT}
            item = probe.probe(process, image, args.teb, 0x362970, 0x3629A0, model,
                               {'eax': input_eax, 'ebx': input_ebx})
            item['case'] = label
            item['modeled_expected_hash'] = hex(model_hash)
            result['synthetic_cpuid_predicates'].append(item)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({
        'contract': result['contract'],
        'worker_cases': [{'case': r['case'], 'stop': r.get('stop_reason'),
                          'final_eip_rva': r['last_eip_rva'], 'eax': r['final_eax'],
                          'model_reads': [r['address'] for r in r['synthetic_field_reads']]}
                         for r in result['worker_predicates']],
        'root_markers': [{'root': r['entry_rva'], 'stop': r.get('stop_reason')}
                         for r in result['root_marker_prefixes']]}, indent=2))


if __name__ == '__main__':
    main()
