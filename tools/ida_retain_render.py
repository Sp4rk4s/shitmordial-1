"""Run INSIDE IDA: retain verified render findings without invoking Hex-Rays.

Preserves existing function boundaries/names and comments. Missing functions
are defined only within verified original-byte ranges, without deleting data.
Writes an application receipt and saves through IDA's own database API.
"""
from pathlib import Path
import hashlib
import json
import runpy

import ida_bytes
import ida_funcs
import ida_loader
import ida_name
import ida_nalt
import ida_ua
import idaapi
import idc

ROOT = Path(__file__).resolve().parent.parent


def merge(old, new):
    old = old or ''
    return old if new in old else (old + '\n\n' + new).strip()


def main():
    index_path = ROOT / 'analysis/ida/project-index.json'
    index = json.loads(index_path.read_text())
    original = (ROOT / 'primordial.bin').read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    if (digest != index['binary_sha256'] or
            bytes(ida_nalt.retrieve_input_file_sha256()).hex() != digest or
            idaapi.get_imagebase() != 0x44000000):
        raise RuntimeError('Wrong IDA input identity/base')

    # Restore the existing verified project annotations through IDA APIs.
    runpy.run_path(str(ROOT / 'tools/ida_cache_project.py'), run_name='__main__')
    records = []
    comments = []
    for filename in ['render-ida-pending.json', 'font-render-annotations-pending.json',
                     'selector-annotations-pending.json', 'font-refresh-annotations-pending.json',
                     'scan11-annotations-pending.json', 'hook-helper-annotations-pending.json']:
        batch = json.loads((ROOT / 'analysis/menu' / filename).read_text())
        if batch.get('binary_sha256', batch.get('sha256')) != digest:
            raise RuntimeError('Wrong pending annotation identity: ' + filename)
        records.extend(batch['functions'])
        comments.extend(batch.get('data_comments', []))
    known_ends = {0x4432BA40: 0x4432C125,
                  0x44360CE0: 0x44360CF4, 0x443609E0: 0x443609ED}
    receipt = {'binary_sha256': digest, 'functions': [], 'data_comments': [],
               'database_disk_save_verified': False}
    for record in records:
        address = int(record['address'], 16)
        end = int(record.get('end_address', hex(known_ends.get(address, 0))), 16)
        if not address < end <= 0x4540C000:
            raise RuntimeError('Missing verified function boundary: ' + record['address'])
        expected = original[address - 0x44000000:end - 0x44000000]
        if record.get('byte_sha256') and hashlib.sha256(expected).hexdigest() != record['byte_sha256']:
            raise RuntimeError('Pending function byte hash mismatch')
        if ida_bytes.get_bytes(address, end - address) != expected:
            raise RuntimeError('Live function bytes differ: ' + record['address'])
        function = ida_funcs.get_func(address)
        if function and (function.start_ea != address or function.end_ea != end):
            receipt['functions'].append({'address': record['address'],
                                        'status': 'preserved existing different function boundary'})
            continue
        if not function:
            if any(ida_funcs.get_func(ea) for ea in range(address, end)):
                receipt['functions'].append({'address': record['address'],
                                            'status': 'preserved overlapping analyst function'})
                continue
            cursor = address
            while cursor < end:
                if ida_bytes.is_data(ida_bytes.get_full_flags(cursor)):
                    break
                size = ida_ua.create_insn(cursor)
                if not size or cursor + size > end:
                    break
                cursor += size
            if cursor != end or not ida_funcs.add_func(address, end):
                receipt['functions'].append({'address': record['address'],
                                            'status': 'definition pending; existing data/decode preserved'})
                continue
        name = ida_funcs.get_func_name(address)
        if name == record['name'] or name in record.get('aliases', []) or name.startswith('sub_'):
            if name != record['name'] and not ida_name.set_name(address, record['name'], ida_name.SN_CHECK):
                raise RuntimeError('Cannot rename verified function')
            if not idc.SetType(address, record['prototype']):
                raise RuntimeError('Cannot apply verified prototype')
        # Store long comments on functions, leaving existing instruction comments intact.
        comment = merge(idc.get_func_cmt(address, True), record['comment'])
        if not idc.set_func_cmt(address, comment, True):
            raise RuntimeError('Cannot retain function comment')
        if record['comment'] not in (idc.get_func_cmt(address, True) or ''):
            raise RuntimeError('Function comment readback failed')
        updated = dict(record, name=ida_funcs.get_func_name(address), comment=comment)
        if updated['name'] != record['name']:
            updated.pop('prototype', None)
        index['functions'] = [r for r in index['functions']
                              if int(r['address'], 16) != address] + [updated]
        receipt['functions'].append({'address': hex(address), 'status': 'applied and read back',
                                    'name': updated['name'], 'type': idc.get_type(address),
                                    'comment': comment})
    for record in comments:
        address = int(record['address'], 16)
        comment = merge(ida_bytes.get_cmt(address, False), record['comment'])
        if not ida_bytes.set_cmt(address, comment, False):
            raise RuntimeError('Cannot retain partial finding')
        if ida_bytes.get_cmt(address, False) != comment:
            raise RuntimeError('Address comment readback failed')
        index['data_comments'] = [r for r in index['data_comments']
                                  if int(r['address'], 16) != address] + [dict(record, comment=comment)]
        receipt['data_comments'].append(dict(record, comment=comment))
    database = idc.get_idb_path()
    if not database or not ida_loader.save_database(database, 0):
        raise RuntimeError('IDA database save failed')
    receipt['database_disk_save_verified'] = True
    receipt['database'] = database
    index_path.write_text(json.dumps(index, indent=2) + '\n')
    (ROOT / 'analysis/menu/render-ida-applied.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Render findings retained, read back, and saved through IDA:', database)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        (ROOT / 'analysis/menu/render-ida-error.txt').write_text(repr(error))
        raise
