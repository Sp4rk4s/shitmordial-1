"""Read-only snapshot of the user's running x86 game; never writes target memory."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    args = parser.parse_args()
    kernel = c.WinDLL("kernel32", use_last_error=True)
    psapi = c.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    psapi.EnumProcessModulesEx.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), w.DWORD]
    psapi.GetModuleFileNameExW.argtypes = [w.HANDLE, c.c_void_p, w.LPWSTR, w.DWORD]
    handle = kernel.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise c.WinError(c.get_last_error())

    def read(address, size):
        buffer = c.create_string_buffer(size)
        count = c.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buffer, size, c.byref(count)) or count.value != size:
            raise OSError(f"Cannot read {address:#x}+{size:#x}: {c.get_last_error()}")
        return buffer.raw

    result = {"pid": args.pid, "modules": [], "globals": {}, "resolvers": []}
    try:
        array = (c.c_void_p * 2048)()
        needed = w.DWORD()
        if not psapi.EnumProcessModulesEx(handle, array, c.sizeof(array), c.byref(needed), 3):
            raise c.WinError(c.get_last_error())
        for base in array[:needed.value // c.sizeof(c.c_void_p)]:
            name = c.create_unicode_buffer(32768)
            psapi.GetModuleFileNameExW(handle, base, name, len(name))
            item = {"base": hex(base), "path": name.value}
            if "primordial" in name.value.lower():
                header = read(base, 4096)
                pe = struct.unpack_from("<I", header, 60)[0]
                item["timestamp"] = hex(struct.unpack_from("<I", header, pe + 8)[0])
                item["image_size"] = hex(struct.unpack_from("<I", header, pe + 80)[0])
                item["disk_sha256"] = hashlib.sha256(Path(name.value).read_bytes()).hexdigest()
            result["modules"].append(item)
        for address, size in [(0x45751000, 40), (0x4481B634, 24), (0x44B66238, 64),
                              (0x44B486E4, 40), (0x44B49564, 24), (0x4470A002, 40),
                              (0x4448F360, 48), (0x443E1E50, 384), (0x4481DCE0, 520)]:
            try:
                result["globals"][hex(address)] = read(address, size).hex()
            except OSError as exc:
                result["globals"][hex(address)] = {"error": str(exc)}
        for rva in range(0x4DDF28, 0x4DE158, 40):
            data = read(0x44340000 + rva, 40)
            result["resolvers"].append({"rva": hex(rva), "words": [hex(x) for x in struct.unpack("<10I", data)]})
    finally:
        kernel.CloseHandle(handle)
    path = Path(__file__).resolve().parent.parent / f"analysis/menu/runtime-{args.pid}.json"
    path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"snapshot": str(path), "payload_modules": [x for x in result["modules"] if "primordial" in x["path"].lower()], "globals": result["globals"], "resolvers": result["resolvers"]}, indent=2))


if __name__ == "__main__":
    main()
