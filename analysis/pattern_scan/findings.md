# Primordial module pattern scanner

Target: `primordial.bin`, SHA-256
`e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768`.
IDA base: `0x44000000`; raw size: `0x140C000`.
Analysis used direct read-only JSON-RPC calls to `http://127.0.0.1:13337/mcp`.
No target bytes, names or comments were changed in IDA.

## Findings

- **Scanner:** `sub_4421E560`, VA `0x4421E560`, RVA/file offset `0x21E560`, size `0x133`.
- **Pattern parser:** `sub_4421DAC0`, VA `0x4421DAC0`, RVA/file offset `0x21DAC0`, size `0xEC`.
- The parser builds a three-pointer vector of 32-bit tokens. `?` and `??` become
  `-1`; other tokens use a base-16 conversion call. Its assembly ends in `ret 8`.
- The scanner hashes its first stack argument with 32-bit FNV-1a and passes the
  hash to `sub_4406D020`; subsequent code reads a returned module's `e_lfanew` and
  PE `SizeOfImage`. It invokes the parser at `0x4421E5E0`, compares image bytes
  against parsed tokens, accepts `-1` as wildcard and returns a matched VA or zero.

The module lookup's full behavior and whether this scanner is called during a
real startup have not been observed. IDA reports no incoming cross-references to
the scanner; a raw search also found no direct `E8` calls or absolute scanner-VA
constants. Indirect/protected calls remain unresolved. This is a verified scanner
implementation, not a claim that it is the only scan routine in the binary.

## Calling convention

Assembly reads module name from `[ebp+8]` and pattern text from `[ebp+10h]`.
The intervening `[ebp+0Ch]` stack slot is not read in the observed body. The
function returns its result through EAX and ends with plain `ret`, so the hook
preserves three caller-cleaned stack arguments:

```cpp
uintptr_t __cdecl scan(const char* module_name, uintptr_t reserved,
                      const char* pattern);
```

Hex-Rays prints an additional incoming EBX `__usercall` argument. In assembly,
EBX is saved before being overwritten with the parsed token count and restored
after the scan. Its later use on an invalid-allocation abort path is not evidence
of a normal incoming semantic argument. The detour preserves the x86 nonvolatile
register convention and forwards the intervening stack slot without assigning
meaning to it.

Verified first 16 raw bytes:

```text
55 8B EC 64 A1 00 00 00 00 6A FF 68 ED B1 71 44
```

## Hook implementation and limits

`primordial_loader/src/pattern_hook.cpp` checks mapping/executable memory and this
prefix, then uses actual MinHook create/enable operations. The mapper installs
the hook after raw-checksum validation, existing patches and protection changes,
before creating the entry thread. It never maps or reads another payload file.

The detour calls the trampoline, logs the bounded module/pattern text, reserved
argument and returned address through stdout and `OutputDebugStringA`, and returns
the original result. It preserves the original call's Win32 last-error value.
Unreadable string pointers are handled by the diagnostic logger. No scan failures
are suppressed and no matching algorithm is replaced. The patcher module is
pinned after installation so payload threads cannot call an unloaded detour.
Restart the game to unload it. Removal is provided for quiescent test/maintenance
use and is not automatically attempted while payload threads might be scanning.

## Retained observations

- `0x4421dac0.json`: parser pseudocode returned by `decompile_function`.
- `disassemble_function_parser.json`: parser assembly.
- `get_xrefs_to_parser.json`: observed call from the scanner at `0x4421E5E0`.
- `decompile_function_scanner.json`: scanner pseudocode.
- `disassemble_function_scanner.json`: scanner assembly and stack frame.
- `get_xrefs_to_scanner.json`: bounded empty IDA incoming-reference result.
- Other JSON files record rejected candidates and function inventory used during
  the search; their presence does not identify them as pattern scanners.

Synthetic hook tests use the same 16-byte prologue and a deliberately different,
self-contained body. They validate MinHook patching, trampoline execution, all
three stack arguments, zero/nonzero returns, bounded logging and byte restoration.
They do not execute the real scanner or establish real-game compatibility.
