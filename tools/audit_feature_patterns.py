"""Read-only pattern/consumer audit against the running legacy game.

Reference matches are candidates, never automatic replacements. The one linked
event target is independently checked through two public interface wrappers.
No target calls, writes, injections or debugger changes are performed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from capture_source_interfaces import snapshot
from test_menu import Process, ROOT, one

REFERENCE_URL = 'https://github.com/cacamelio/OsirisAndExtra'
REFERENCE_COMMIT = 'c85f8301027b3707a1faf0f6c0352518fdc431b3'
OLD_EVENT = bytes.fromhex('558bec83e4f883ec0c538bd95657895c240c')


def word(data, offset=0):
    return struct.unpack_from('<I', data, offset)[0]


def module_sections(process, base, requested_flags):
    header = process.read(base, 4096)
    pe = word(header, 60)
    if header[:2] != b'MZ' or header[pe:pe + 4] != b'PE\0\0':
        raise RuntimeError('Invalid module PE')
    count = struct.unpack_from('<H', header, pe + 6)[0]
    optional_size = struct.unpack_from('<H', header, pe + 20)[0]
    if not 0 < count < 64 or pe + 24 + optional_size + count * 40 > len(header):
        raise RuntimeError('Unexpected section table')
    result = []
    for index in range(count):
        offset = pe + 24 + optional_size + index * 40
        size, rva = struct.unpack_from('<II', header, offset + 8)
        flags = word(header, offset + 36)
        if flags & requested_flags:
            if not 0 < size < 0x6000000:
                raise RuntimeError('Unexpected executable section size')
            result.append((base + rva, process.read(base + rva, size)))
    return result


def executable_sections(process, base):
    return module_sections(process, base, 0x20000000)


def player_vtable(process, facts, reference_rows):
    """RTTI anchors identify the primary class table, not a guessed slot owner."""
    base=int(facts['client.dll']['base'],16)
    data=module_sections(process,base,0x40000000)
    type_names=matches(data,list(b'.?AVC_CSPlayer@@\0'))
    if len(type_names)!=1:
        raise RuntimeError('C_CSPlayer RTTI type descriptor is absent or ambiguous')
    descriptor=type_names[0]-8
    locators=[]
    for reference in matches(data,list(struct.pack('<I',descriptor))):
        address=reference-12
        if address&3:
            continue
        value=process.read(address,20)
        # CompleteObjectLocator: signature0, primary subobject offset0,
        # constructor displacement0, exact type descriptor.
        if value[:16]==struct.pack('<4I',0,0,0,descriptor):
            locators.append(address)
    if len(locators)!=1:
        raise RuntimeError('Primary C_CSPlayer complete-object locator ambiguous')
    tables=[address+4 for address in matches(data,list(struct.pack('<I',locators[0]))) if not address&3]
    if len(tables)!=1:
        raise RuntimeError('Primary C_CSPlayer vtable ambiguous')
    vtable=tables[0]
    candidates=[]
    for name,slot in [('doExtraBoneProcessing',198),('updateClientSideAnimation',224),('eyeAngles',170),('calculateView',277)]:
        reference=next(row for row in reference_rows if row['reference_name']==name)
        target=word(process.read(vtable+4*slot,4))
        if reference['matches']!=[hex(target)]:
            raise RuntimeError(f'{name} reference no longer agrees with primary class table')
        candidates.append(dict(name=name,slot=slot,target=hex(target),target_rva=hex(target-base),
                               reference=reference['source'],
                               status='reference and class owner corroborated; original Primordial consumer mapping pending'))
    shoot=word(process.read(vtable+285*4,4))
    candidates.append(dict(name='slot285_vector_output',slot=285,target=hex(shoot),target_rva=hex(shoot-base),
                           prefix=process.read(shoot,50).hex(),
                           status='vector-output/RET4 candidate for detour155BD0; old function identity and operand dependencies unresolved'))
    return dict(type_descriptor=hex(descriptor),locator=hex(locators[0]),primary_vtable=hex(vtable),
                candidates=candidates,scope='Read-only class/target evidence; no encoded pointer pair changed')


def matches(sections, pattern):
    expression = re.compile(b''.join(b'.' if v is None else re.escape(bytes([v]))
                                     for v in pattern), re.DOTALL)
    # Lookahead counts overlapping matches too; ambiguity must never be hidden.
    expression = re.compile(b'(?=(' + expression.pattern + b'))', re.DOTALL)
    return [address + match.start() for address, data in sections
            for match in expression.finditer(data)]


def event_link(process, facts, sections):
    engine = facts['engine.dll']
    base = int(engine['base'], 16)
    record = next(e for e in engine['entries'] if e['name'] == 'GAMEEVENTSMANAGER002')
    getter = process.read(int(record['create'], 16), 6)
    if getter[0] != 0xB8 or getter[5] != 0xC3:
        raise RuntimeError('Unverified event manager singleton factory')
    singleton = word(getter, 1)
    vtable = word(process.read(singleton, 4))
    targets = []
    wrappers = []
    for slot, call_offset in ((8, 0x39), (9, 0xA)):
        address = word(process.read(vtable + slot * 4, 4))
        code = process.read(address, call_offset + 5)
        if code[:3] != bytes.fromhex('558bec') or code[call_offset] != 0xE8:
            raise RuntimeError('Event public wrapper call-site profile changed')
        if slot == 9 and code[:10] != bytes.fromhex('558bec6a016a00ff7508'):
            raise RuntimeError('FireEventClientSide argument forwarding changed')
        target = address + call_offset + 5 + struct.unpack_from('<i', code, call_offset + 1)[0]
        targets.append(target)
        wrappers.append(dict(slot=slot, address=hex(address), rva=hex(address-base),
                             bytes=code.hex(), call_offset=call_offset, target=hex(target)))
    if targets[0] != targets[1]:
        raise RuntimeError('Event wrappers no longer converge on one private target')
    target = targets[0]
    if not any(a <= target < a + len(d) for a, d in sections):
        raise RuntimeError('Private event target is outside engine executable sections')
    prefix = process.read(target, 18)
    expected = bytes.fromhex('558bec83e4f883ec0c8bc15356578db09800')
    if prefix != expected or matches(sections, list(prefix)) != [target]:
        raise RuntimeError('Current private event prefix is changed or ambiguous')
    return dict(status='target-linked; not activated', payload_detour_rva='0x155AD0',
                payload_original_slot_rva='0x7FB8EC', masked_scanner_rva='0x155840',
                masked_scan_return_rva='0x155352', mask=0,
                old_pattern=OLD_EVENT.hex(), old_matches=[hex(a) for a in matches(sections, list(OLD_EVENT))],
                replacement_pattern=prefix.hex(), expected_target=target, target_rva=hex(target-base),
                module='engine.dll', module_sha256=engine['disk_sha256'],
                singleton=hex(singleton), wrappers=wrappers,
                remaining='Verify full callback dependencies and startup contract before live installation')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=ROOT/'build/references/OsirisAndExtra')
    args = parser.parse_args()
    revision = subprocess.check_output(['git', '-C', str(args.reference), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REFERENCE_COMMIT:
        raise RuntimeError('Reference revision changed; review before updating pinned provenance')
    source_path = args.reference/'Osiris/Memory.cpp'
    pristine = subprocess.check_output(['git', '-C', str(args.reference), 'show', f'{revision}:Osiris/Memory.cpp'])
    if hashlib.sha256(source_path.read_bytes().replace(b'\r\n', b'\n')).digest() != hashlib.sha256(pristine.replace(b'\r\n', b'\n')).digest():
        raise RuntimeError('Reference Memory.cpp has local edits')
    pid = one('csgo.exe')
    if not pid:
        raise RuntimeError('Legacy CS:GO is not running')
    facts = snapshot(pid)['factories']
    rows = []
    with Process(pid) as process:
        sections = {name: executable_sections(process, int(facts[name]['base'], 16))
                    for name in ('client.dll', 'engine.dll')}
        for line_number, line in enumerate(source_path.read_text().splitlines(), 1):
            if line.lstrip().startswith('//'):
                continue
            for match in re.finditer(r'findPattern\((CLIENT_DLL|ENGINE_DLL), "([^"]+)"', line):
                literal = match.group(2)
                if re.sub(r'\\x[\da-fA-F]{2}|\?', '', literal):
                    raise RuntimeError('Unsupported reference pattern literal')
                pattern = [int(a, 16) if a else None
                           for a, wildcard in re.findall(r'\\x([\da-fA-F]{2})|(\?)', literal)]
                module = {'CLIENT_DLL':'client.dll', 'ENGINE_DLL':'engine.dll'}[match.group(1)]
                hits = matches(sections[module], pattern)
                rows.append(dict(reference_name=line.split('=')[0].strip(), module=module,
                                 source=f'{REFERENCE_URL}/blob/{revision}/Osiris/Memory.cpp#L{line_number}',
                                 expression=line.strip(), pattern=pattern,
                                 matches=[hex(a) for a in hits],
                                 status='unique candidate; consumer unlinked' if len(hits)==1 else 'missing or ambiguous; reject'))
        link = event_link(process, facts, sections['engine.dll'])
        player = player_vtable(process,facts,rows)
    report = dict(pid=pid, scope='Read-only non-atomic capture; no gameplay readiness claimed',
                  reference_revision=revision, reference_source_sha256=hashlib.sha256(pristine).hexdigest(),
                  modules={n:dict(path=facts[n]['path'], base=facts[n]['base'], sha256=facts[n]['disk_sha256'])
                           for n in sections}, reference_candidates=rows, consumer_links=[link],player_class_evidence=player)
    output = ROOT/f'analysis/menu/feature-pattern-audit-{pid}.json'
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(report=str(output), reference_patterns=len(rows),
                          unique_candidates=sum(len(r['matches'])==1 for r in rows),
                          event_target=hex(link['expected_target']), feature_testing_ready=False)))


if __name__ == '__main__':
    main()
