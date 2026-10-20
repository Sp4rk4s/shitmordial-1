"""Read-only VMProtect import-name/table investigation for the supplied raw dump.

Name cipher is derived from the user-supplied VMProtect 3.9.1 source:
core/intel.cc EncryptString and runtime/loader.cc DECRYPT.
This does not execute the target or patch the input file.
"""
from __future__ import annotations
import argparse
import json
import struct
import hashlib
from pathlib import Path


def rol32(value: int, count: int) -> int:
    count &= 31
    return ((value << count) | (value >> ((32 - count) & 31))) & 0xFFFFFFFF


def recover_key(cipher: bytes, plain: bytes) -> tuple[int, int] | None:
    key = known = 0
    for index, (encoded, decoded) in enumerate(zip(cipher, plain)):
        rotated = ((encoded ^ decoded) - index) & 255
        for bit in range(8):
            position = (bit - index) & 31
            mask = 1 << position
            value = ((rotated >> bit) & 1) << position
            if known & mask and (key & mask) != value:
                return None
            known |= mask
            key |= value
    return key, known


def find_name(data: bytes, name: str):
    plain = name.encode("ascii") + b"\0"
    first, second = plain[:2]
    for offset in range(len(data) - len(plain) + 1):
        low = data[offset] ^ first
        shifted = ((data[offset + 1] ^ second) - 1) & 255
        if shifted & 254 != (low << 1) & 254:
            continue
        # Each subsequent byte constrains seven overlapping key bits.
        previous = shifted
        for index in range(2, len(plain)):
            value = ((data[offset + index] ^ plain[index]) - index) & 255
            if value & 254 != (previous << 1) & 254:
                break
            previous = value
        else:
            candidate = recover_key(data[offset:offset + len(plain)], plain)
            if candidate:
                yield {"name": name, "rva": hex(offset), "key": hex(candidate[0]), "known_bits": hex(candidate[1])}


def decode_name(data: bytes, offset: int, key: int) -> str:
    if not 0 <= offset < len(data):
        raise ValueError("name RVA outside binary")
    result = bytearray()
    for index in range(min(512, len(data) - offset)):
        value = data[offset + index] ^ ((rol32(key, index) + index) & 255)
        if value == 0:
            return result.decode("ascii")
        if not 32 <= value < 127:
            raise ValueError("decoded name is not printable ASCII")
        result.append(value)
    raise ValueError("name lacks a bounded terminator")


def decode_table(data: bytes, start: int, key: int) -> dict:
    def u32(offset):
        if not 0 <= offset <= len(data) - 4:
            raise ValueError("record outside binary")
        return struct.unpack_from("<I", data, offset)[0]
    position = start
    groups = []
    destinations = set()
    while position + 4 <= len(data):
        group_start = position
        try:
            module = decode_name(data, u32(position), key)
        except ValueError:
            break
        if not module.lower().endswith((".dll", ".drv")):
            break
        position += 4
        imports = []
        while u32(position):
            name_rva, destination, operand_key = struct.unpack_from("<III", data, position)
            if destination > len(data) - 4 or destination in destinations:
                raise ValueError("invalid or duplicate import destination")
            destinations.add(destination)
            symbol = "#" + str(name_rva & 65535) if name_rva & 0x80000000 else decode_name(data, name_rva, key)
            original = u32(destination)
            imports.append({"record_rva": hex(position), "name_rva": hex(name_rva),
                "symbol": symbol, "destination_rva": hex(destination), "key": hex(operand_key),
                "stored": hex(original), "decoded_existing": hex((original + operand_key) & 0xFFFFFFFF)})
            position += 12
        position += 4
        groups.append({"dll": module, "record_rva": hex(group_start), "imports": imports})
    if not groups:
        raise ValueError("no valid DLL groups")
    return {"binary_sha256": hashlib.sha256(data).hexdigest(), "key": hex(key),
            "table_rva": hex(start), "table_size": hex(position - start), "groups": groups}


def emit_manifest(result: dict, destination: Path):
    lines = ["// Generated from this dump's decoded VMProtect IMPORT_INFO records.",
             "// SHA-256: " + result["binary_sha256"],
             "// stored = resolved_export - key (32-bit modulo arithmetic)."]
    for group in result["groups"]:
        for entry in group["imports"]:
            lines.append("    { " + entry["destination_rva"] + ", Kind::keyed, " +
                json.dumps(group["dll"]) + ", " + json.dumps(entry["symbol"]) + ", " +
                entry["stored"] + "u, " + entry["key"] + "u },")
    destination.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--name", action="append")
    parser.add_argument("--decode-key", type=lambda text: int(text, 0))
    parser.add_argument("--table-rva", type=lambda text: int(text, 0))
    parser.add_argument("--emit-manifest", type=Path)
    args = parser.parse_args()
    data = args.binary.read_bytes()
    if args.decode_key is not None:
        if args.table_rva is None:
            parser.error("--decode-key requires --table-rva")
        result = decode_table(data, args.table_rva, args.decode_key)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n")
        if args.emit_manifest:
            emit_manifest(result, args.emit_manifest)
        print("decoded", len(result["groups"]), "DLLs,",
              sum(len(group["imports"]) for group in result["groups"]), "imports")
        return
    names = args.name or ["AddVectoredExceptionHandler", "_Thrd_hardware_concurrency",
                          "api-ms-win-crt-runtime-l1-1-0.dll", "GetProcAddress"]
    records = []
    for name in names:
        found = list(find_name(data, name))
        records.extend(found)
        print(name, json.dumps(found), flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    main()
