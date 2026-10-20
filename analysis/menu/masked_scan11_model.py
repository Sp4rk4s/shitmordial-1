"""Pure offline models of an exact native masked-scan template family."""
from pathlib import Path
import hashlib
import json
import random
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
PROFILES = {
    0xD47A0: (13, '47982356a882f0fbea56df42c39cc6baefbf63547a2bcc334ee8fec728cf9f84'),
    0xFB720: (10, 'a4e27ed74a053d8af3ea94386c88338a50e738531556c365d541f48da8a01aa9'),
    0x132D10: (8, '7f7e103e8755b542814e3ae3c2e2bf84b07ec803d4220442cef7bef7238d0f48'),
    0x132DA0: (12, '72d2a4ed0c0165f846814119bd0e67b5b733ef9dbd17444109262f6d42fa2cfe'),
    0x155720: (5, '9f7d2f60d839378a79349575f93268e2519ebe899ee866666b9232d5252537a8'),
    0x1557B0: (9, '322e4e909d23fae90016a644bc53e3365e0aa920fb5fff8f390cd8f129b36323'),
    0x155840: (18, '469c004665d1efd1747065ca5fddd31e2956b29257cf38c16e4b47170acdf596'),
    0x1558D0: (20, '22b44b663092b8601f79c2afec8b8e3544b8aab3d41ab8755a9d54880f2ed54a'),
    0x155960: (11, '52025afe7f66d40621e47ad383bd9bfb13fafebc97bd44162f51e1bb6efe9624'),
}


def find_offset(data, pattern, mask):
    width = len(pattern)
    if width not in {value[0] for value in PROFILES.values()}:
        raise ValueError('Pattern length outside verified native template family')
    # Original checks position < start+length-width. Preserve the excluded final
    # possible position and the unconditional first-byte comparison (bit0 is
    # ignored), rather than silently repairing either behavior in the model.
    limit = max(0, len(data) - width)
    position = data.find(pattern[:1], 0, limit)
    while position >= 0:
        if all(mask & (1 << i) or data[position + i] == pattern[i] for i in range(1, width)):
            return position
        position = data.find(pattern[:1], position + 1, limit)
    return None


def verify_profile(raw, rva, width, digest):
    sys.path.insert(0, str(ROOT / 'build/analysis-python'))
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX
    code = raw[rva:rva + 0x8C]
    assert hashlib.sha256(code).hexdigest() == digest
    expected_template = bytearray(raw[0x155960:0x1559EC])
    expected_template[0x10] = (-width) & 255
    expected_template[0x2D] = (width + 3) & ~3
    expected_template[0x40] = expected_template[0x51] = width
    struct.pack_into('<i', expected_template, 0x87, 0xD1C60 - (rva + 0x8B))
    assert code == expected_template, 'Unexpected instruction difference beyond verified template parameters'
    cases = [
        ('too-short', b'A' * (width - 1), b'A' * width, 0),
        ('equal-length-excluded', b'A' * width, b'A' * width, 0),
        ('first-position', b'A' * (width + 1), b'A' * width, 0),
        ('last-position-excluded', b'Z' + b'A' * width, b'A' * width, 0),
        ('first-mask-bit-ignored', b'Z' + b'A' * (width * 2), b'Q' + b'A' * (width - 1), 1),
        ('rest-wildcards', b'Z' + b'A' * (width * 2), b'A' + b'Q' * (width - 1), ((1 << width) - 1) & ~1),
    ]
    rng = random.Random(155960)
    for i in range(100):
        data = bytes(rng.randrange(6) for _ in range(rng.randrange(width + 1, 100)))
        offset = rng.randrange(len(data) - width + 1)
        pattern = bytearray(data[offset:offset + width])
        mask = rng.getrandbits(32)
        for j in range(1, width):
            if mask & (1 << j):
                pattern[j] ^= 0x80
        cases.append((f'random-{i}', data, bytes(pattern), mask))
    for label, data, pattern, mask in cases:
        cpu = Uc(UC_ARCH_X86, UC_MODE_32)
        entry = 0x44000000 + rva
        cpu.mem_map(entry & ~4095, 4096)
        cpu.mem_write(entry, code)
        cpu.mem_map(0x10000000, 0x30000)
        cpu.mem_write(0x10001000, pattern + bytes((-width) & 3) + struct.pack('<I', mask))
        cpu.mem_write(0x10002000, data)
        cpu.mem_write(0x10010000, struct.pack('<4I', 0x12345678, 0x10003000, 0x10002000, len(data)))
        cpu.reg_write(UC_X86_REG_ESP, 0x10010000)
        cpu.reg_write(UC_X86_REG_ECX, 0x10001000)
        cpu.emu_start(entry, 0x12345678, count=100000)
        actual = struct.unpack('<I', bytes(cpu.mem_read(0x10003000, 4)))[0]
        expected = find_offset(data, pattern, mask)
        assert actual == (0 if expected is None else 0x10002000 + expected), label
        assert cpu.reg_read(UC_X86_REG_EAX) == 0x10003000, label
        assert cpu.reg_read(UC_X86_REG_ESP) == 0x10010010, label
    return {'rva': hex(rva), 'width': width, 'helper_sha256': digest,
              'cases_passed': len(cases), 'external_callees_executed': 0,
              'strict_end_and_first_mask_bit_preserved': True}


def verify():
    raw = (ROOT / 'primordial.bin').read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    profiles = [verify_profile(raw, rva, width, digest) for rva, (width, digest) in PROFILES.items()]
    result = {'mode': 'OFFLINE original x86 bytes interpreted without semantic hooks; exact template comparison',
              'raw_sha256': digest, 'profiles': profiles,
              'total_cases_passed': sum(p['cases_passed'] for p in profiles)}
    (ROOT / 'analysis/menu/masked-scan-family-model-validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    verify()
