"""Run inside the user's IDA database (File -> Script file), not system Python.

Replay the project annotation index without redoing analysis and save using IDA.
Never changes input bytes or replaces an existing function. Missing functions
are created only for exact byte-guarded entries in pending_functions.
"""
from pathlib import Path
import json
import hashlib

import ida_bytes
import ida_funcs
import ida_loader
import ida_name
import ida_nalt
import idaapi
import ida_ua
import idc


def merge_comment(existing, addition):
    existing = existing or ""
    return existing if addition in existing else (existing + "\n\n" + addition).strip()


def main():
    root = Path(__file__).resolve().parent.parent
    index = json.loads((root / "analysis/ida/project-index.json").read_text())
    digest = bytes(ida_nalt.retrieve_input_file_sha256()).hex()
    if not digest:
        digest = hashlib.sha256(Path(ida_nalt.get_input_file_path()).read_bytes()).hexdigest()
    if digest != index["binary_sha256"] or idaapi.get_imagebase() != int(index["analysis_base"], 16):
        raise RuntimeError("IDA target identity/base does not match the annotation index")

    pending = index.get("pending_functions", [])
    for entry in pending:
        address = int(entry["address"], 16)
        end = int(entry["end_address"], 16)
        expected = bytes.fromhex(entry["expected_bytes"])
        if len(expected) != end - address or ida_bytes.get_bytes(address, len(expected)) != expected:
            raise RuntimeError("Missing function byte guard failed: " + entry["address"])
        existing = ida_funcs.get_func(address)
        if existing:
            if existing.start_ea != address:
                raise RuntimeError("Preserving overlapping analyst function at " + entry["address"])
            continue
        if any(ida_funcs.get_func(ea) for ea in range(address, end)):
            raise RuntimeError("Preserving overlapping analyst function at " + entry["address"])
        cursor = address
        while cursor < end:
            length = ida_ua.create_insn(cursor)
            if not length or cursor + length > end:
                raise RuntimeError("Cannot decode guarded initializer at " + hex(cursor))
            cursor += length
        if not ida_funcs.add_func(address, end):
            raise RuntimeError("Cannot define guarded initializer at " + entry["address"])

    for entry in index["functions"] + pending:
        address = int(entry["address"], 16)
        function = ida_funcs.get_func(address)
        if not function or function.start_ea != address:
            raise RuntimeError("Expected function boundary missing: " + entry["address"])
        current = ida_funcs.get_func_name(address)
        if current != entry["name"]:
            if current in entry.get("aliases", []) or current.startswith("sub_"):
                if not ida_name.set_name(address, entry["name"], ida_name.SN_CHECK):
                    raise RuntimeError("Cannot set function name: " + entry["name"])
            else:
                print("Preserving analyst name:", current, "at", entry["address"])
        comment = merge_comment(ida_bytes.get_cmt(address, False), entry["comment"])
        if not ida_bytes.set_cmt(address, comment, False):
            raise RuntimeError("Cannot set comment at " + entry["address"])
        comment = merge_comment(idc.get_func_cmt(address, True), entry["comment"])
        idc.set_func_cmt(address, comment, True)
        if entry.get("prototype") and ida_funcs.get_func_name(address) == entry["name"]:
            if not idc.SetType(address, entry["prototype"]):
                raise RuntimeError("Cannot apply verified prototype: " + entry["name"])

    # Merge annotations, including group/header records sharing an address.
    for entry in index["data_comments"]:
        address = int(entry["address"], 16)
        comment = merge_comment(ida_bytes.get_cmt(address, False), entry["comment"])
        if not ida_bytes.set_cmt(address, comment, False):
            raise RuntimeError("Cannot annotate import metadata at " + entry["address"])

    database = idc.get_idb_path()
    if not database or not ida_loader.save_database(database, 0):
        raise RuntimeError("Annotations applied, but IDA could not save the database")
    print("Project annotations retained and IDA database saved:", database)


if __name__ == "__main__":
    main()
