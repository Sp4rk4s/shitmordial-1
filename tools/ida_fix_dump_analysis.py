"""Run INSIDE IDA to correct immutable-segment assumptions for this raw dump.

Changes analysis metadata only: keeps addresses, bytes, functions, names, types,
comments and segment boundaries. Saves with IDA's own API and writes a receipt.
The current loader's mixed RWX mapping is a reconstruction, not evidence of the
original PE section boundaries.
"""
from pathlib import Path
import hashlib
import json
import runpy

import ida_hexrays
import ida_loader
import ida_nalt
import ida_segment
import idaapi
import idc

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x44000000
DIGEST = 'e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768'


def describe(segment):
    return {'start': hex(segment.start_ea), 'end': hex(segment.end_ea),
            'name': ida_segment.get_segm_name(segment),
            'class': ida_segment.get_segment_class(segment.start_ea),
            'permissions': int(segment.perm), 'type': int(segment.type)}


def main():
    if (idaapi.get_imagebase() != BASE or
            bytes(ida_nalt.retrieve_input_file_sha256()).hex() != DIGEST or
            hashlib.sha256((ROOT / 'primordial.bin').read_bytes()).hexdigest() != DIGEST):
        raise RuntimeError('Wrong database/input SHA256/base; no metadata changed')
    segment = ida_segment.getseg(BASE + 0x4DE15C)
    if (not segment or not BASE <= segment.start_ea <= BASE + 0x1000 or
            segment.end_ea != BASE + 0x140C000 or
            ida_segment.get_segm_name(segment) != 'seg000'):
        raise RuntimeError('Unexpected analyst segment layout; no metadata changed')
    before = describe(segment)
    if before['class'] not in ('CODE', 'CONST', 'DATA', '', 'PRMDUMP'):
        raise RuntimeError('Preserving unrecognized analyst segment class: ' + before['class'])
    evidence = json.loads((ROOT / 'analysis/menu/runtime-image-protections-11240.json').read_text())
    regions = evidence['regions']
    if (len(regions) != 1 or regions[0]['base'] != '0x44341000' or
            regions[0]['protect'] != '0x40' or regions[0]['size'] != '0x140f000'):
        raise RuntimeError('Runtime permission evidence mismatch; no metadata changed')

    # A neutral class prevents class-based constant folding. Explicit writable
    # permissions match the verified runtime dump mapping and mutable globals.
    # Preserve the segment's code/data type instead of imposing a PE layout.
    if not ida_segment.set_segment_class(segment.start_ea, 'PRMDUMP'):
        raise RuntimeError('Cannot update mixed-dump analysis class')
    segment = ida_segment.getseg(BASE + 0x4DE15C)
    segment.type = before['type']
    segment.perm = before['permissions'] | ida_segment.SEGPERM_WRITE
    if not ida_segment.update_segm(segment):
        raise RuntimeError('Cannot retain mutable segment permissions')
    after = describe(ida_segment.getseg(BASE + 0x4DE15C))
    if (after['class'] != 'PRMDUMP' or
            not after['permissions'] & ida_segment.SEGPERM_WRITE or
            any(after[k] != before[k] for k in ('start', 'end', 'name', 'type'))):
        raise RuntimeError('Segment metadata readback failed')
    # Flush generated pseudocode only; user annotations remain in the database.
    # No automatic decompilation of protected functions is requested here.
    if ida_hexrays.init_hexrays_plugin():
        ida_hexrays.clear_cached_cfuncs()
    runpy.run_path(str(ROOT / 'tools/ida_cache_project.py'), run_name='__main__')
    database = idc.get_idb_path()
    if not database or not ida_loader.save_database(database, 0):
        raise RuntimeError('Metadata applied, but database save failed')
    record = {'binary_sha256': DIGEST, 'analysis_base': hex(BASE),
              'before': before, 'after': after,
              'evidence': 'analysis/menu/runtime-image-protections-11240.json',
              'database': database, 'database_disk_save_verified': True,
              'scope': 'IDA metadata only; no input or target process byte changes'}
    (ROOT / 'analysis/ida/mutable-dump-segment-applied.json').write_text(json.dumps(record, indent=2) + '\n')
    index_path = ROOT / 'analysis/ida/project-index.json'
    index = json.loads(index_path.read_text())
    index['analysis_segment_profile'] = record
    index_path.write_text(json.dumps(index, indent=2) + '\n')
    print('Mutable raw-dump analysis metadata verified and database saved:', database)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        (ROOT / 'analysis/ida/mutable-dump-segment-error.txt').write_text(repr(error))
        raise
