"""Bounded OFFLINE continuation from verified font-store instruction sites.

Synthetic middle-of-function states are not callable ABIs or startup evidence.
No process is opened. Stop at the first native-code transfer or missing memory;
retain concrete arguments as leads, never as recovered original font settings.
"""
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'build/analysis-python'))
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

BASE = 0x44340000
STORES = [0xBB3159, 0xB9D01B, 0xA89FC4, 0x9E5DED, 0xCFF184, 0x9E6BC9]


def trace(image, start, variant):
    assert image[start:start + 3] == b'\x66\x0f\xd6', 'Font-store instruction profile mismatch'
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(BASE, len(image))
    cpu.mem_write(BASE, image)
    cpu.mem_map(0x0FF00000, 0x200000)
    cpu.mem_write(0x10010000, struct.pack('<I', 0x12345678))
    cpu.reg_write(UC_X86_REG_ESP, 0x10010000)
    cpu.reg_write(UC_X86_REG_EFLAGS, 0x202 | variant)
    for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
        cpu.reg_write(reg, 0x10018000)
    cpu.reg_write(UC_X86_REG_XMM0, 0x1020304050607080)
    result = {'entry_rva': hex(start), 'synthetic_flags': hex(0x202 | variant),
              'instructions': 0, 'image_writes': []}
    recent = collections.deque(maxlen=35)
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)

    def code(uc, address, size, _):
        result['instructions'] += 1
        raw = bytes(uc.mem_read(address, size))
        ins = next(decoder.disasm(raw, address), None)
        recent.append({'rva': hex(address - BASE), 'bytes': raw.hex(),
                       'instruction': None if ins is None else ins.mnemonic + ' ' + ins.op_str})
        if not BASE <= address < BASE + len(image) or BASE + 0x1000 <= address < BASE + 0x40D000:
            result['stop_reason'] = 'native/external boundary; no callee executed'
            uc.emu_stop()

    def write(uc, access, address, size, value, _):
        if BASE <= address < BASE + len(image) and len(result['image_writes']) < 100:
            result['image_writes'].append({'rva': hex(address - BASE), 'size': size,
                                          'value': hex(value), 'instruction_rva': hex(uc.reg_read(UC_X86_REG_EIP) - BASE)})

    cpu.hook_add(UC_HOOK_CODE, code)
    cpu.hook_add(UC_HOOK_MEM_WRITE, write)
    try:
        cpu.emu_start(BASE + start, 0, count=100000)
    except UcError as error:
        result['error'] = str(error)
    result['last_code'] = list(recent)
    result['registers'] = {name: hex(cpu.reg_read(reg)) for name, reg in
                           [('eax', UC_X86_REG_EAX), ('ecx', UC_X86_REG_ECX),
                            ('edx', UC_X86_REG_EDX), ('esp', UC_X86_REG_ESP), ('eip', UC_X86_REG_EIP)]}
    try:
        result['stack_words'] = [hex(x) for x in struct.unpack('<20I', bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP), 80)))]
    except UcError:
        pass
    return result


if __name__ == '__main__':
    image = (ROOT / 'primordial.bin').read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    assert digest == 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'
    results = [trace(image, site, flags) for site in STORES for flags in (0, 0x8D5)]
    output = {'sha256': digest, 'scope': __doc__, 'results': results}
    (ROOT / 'analysis/menu/font-store-offline-continuations.json').write_text(json.dumps(output, indent=2) + '\n')
    for result in results:
        print(result['entry_rva'], result['synthetic_flags'], result['instructions'],
              result['registers']['eip'], result.get('stop_reason', result.get('error')))
