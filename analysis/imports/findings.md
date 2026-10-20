# VMProtect import reconstruction

Input: `primordial.bin`, SHA-256
`e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768`.
IDA analysis base: `0x44000000`; raw size: `0x140C000`.

## Matched source format

The user-supplied `C:/Users/k/Desktop/vmprotect-3.9.1-master` is a format reference.
Matching this format does not establish the binary's exact protector version.
Source hashes are retained in `recovery-summary.json`.

- `runtime/loader.h:34` and `:39`: DLL name RVA followed by 12-byte import records
  `{Name, Address, Key}` and a zero name terminator.
- `core/intel.cc:22386` / `runtime/loader.cc:11`: name byte `i` is XORed with
  `uint8_t(rol32(name_key, i) + i)`, including the null terminator.
- `core/intel.cc:21024` and following: protected import reference/key generation.
- `core/intel.cc:23320` and following: loader import record serialization.
- `runtime/loader.cc:2337` through `:2364`: module grouping, name/ordinal lookup
  and import writes using `resolved_export - Key`.

The full key `0x357B2A2D` was independently recovered from known plaintext
`_Thrd_hardware_concurrency` at RVA `0x1156349` and
`api-ms-win-crt-runtime-l1-1-0.dll` at RVA `0x1150165`. References to these encrypted
names led to the same loader table. All groups decode to bounded ASCII names;
destination RVAs are in bounds with no duplicates.

| Item | Recovered value |
| --- | --- |
| Name encryption key | `0x357B2A2D` |
| Table RVA/file offset | `0x10AD0CF` |
| Table size | `0x1074` |
| DLL groups / import records | 18 / 339 |
| Zero-key / ordinal records | 16 / 2 |

| DLL | Records |
| --- | ---: |
| KERNEL32.dll | 47 |
| USER32.dll | 8 |
| GDI32.dll | 1 |
| SHELL32.dll | 1 |
| MSVCP140.dll | 129 |
| WS2_32.dll | 2 |
| VCRUNTIME140.dll | 22 |
| api-ms-win-crt-runtime-l1-1-0.dll | 18 |
| api-ms-win-crt-heap-l1-1-0.dll | 7 |
| api-ms-win-crt-convert-l1-1-0.dll | 7 |
| api-ms-win-crt-stdio-l1-1-0.dll | 46 |
| api-ms-win-crt-filesystem-l1-1-0.dll | 4 |
| api-ms-win-crt-math-l1-1-0.dll | 28 |
| api-ms-win-crt-locale-l1-1-0.dll | 3 |
| api-ms-win-crt-string-l1-1-0.dll | 7 |
| api-ms-win-crt-utility-l1-1-0.dll | 1 |
| api-ms-win-crt-time-l1-1-0.dll | 7 |
| api-ms-win-crt-environment-l1-1-0.dll | 1 |

Examples:

- `KERNEL32!GetProcAddress`: destination RVA `0x40E00C`, key zero.
- `KERNEL32!GetTickCount`: destination RVA `0xE8DC34`, key `0x50F35ABA`.
- `MSVCP140!_Thrd_hardware_concurrency`: RVA `0xD1801E`, key `0x44860B9C`.
- `USER32!LoadCursorW`: destination RVA `0xB541D5`, key `0x68612A51`.

Encoded slots retain the encoding expected by the protected code:
`stored = uint32_t(resolved_export) - key`, with 32-bit wraparound.

## Earlier submitted map

`submitted.json` preserves the supplied 36 records. `audit.json` records bytes at
each site using `0x3CB10000` as the reference base inferred from the supplied
entry argument; the other model's actual analysis base/input hash were not supplied.

Under that interpretation, **0 of 11** claimed E8 operands have an E8 opcode
immediately before the displacement. For example, `0x3CCAC366` becomes RVA
`0x19C366`, beginning `8B C6 5F 5E`; `0x3CC5B1CC` becomes RVA `0x14B1CC` in CC
padding. A search for the seven E8 sites at `0x27` spacing found only one matching
sequence, implying a non-page-aligned bias `0x3C930B11` which still failed the
other four relative sites. No common flat translation validated the relative list.

The proposed entry `0x3CCAFA70` becomes RVA `0x19FA70` / IDA VA `0x4419FA70`,
inside `sub_4419C730` (size `0x62DC`), not an independently identified entry.
The configured entry remains RVA `0x3CA002`; the replacement was not adopted.
The submitted absolute symbol-to-site assignments also lack validating references.
`dbghelp.dll` and `AddVectoredExceptionHandler` are absent from this recovered
table; that does not exclude dynamic resolution elsewhere.

The submitted list is **inactive in production**. Its guarded validator remains
for auditing and tests, where it rejects this dump without modifying any bytes.

## Implementation and validation

`imports_vmp_manifest.inc` contains the 339 decoded destinations, original encoded
values, keys and export identities. `imports.cpp` checks all bounds, byte guards
and overlapping writes, then resolves every export before committing any writes.
It handles decorated data exports, API-set modules and `#ordinal` names, retaining
module references after success.

The patcher calls `apply_recovered()` on the existing writable mapping before the
other patches, section protections, scanner hook and entry thread. It does not
load or remap a payload file. Import failure prevents entry execution.

All **17 tests passed**. The x86 import test resolves all 339 actual exports and
patches a data-only copy of the dump. It verifies keyed/unkeyed values, executes
a corrected E8 export call in a synthetic stub, checks absolute calls and byte
guards, and tests failures without partial writes. Existing mapping fixtures stub
import binding; the separate import test exercises the real implementation.

These tests do not execute the real protected payload. Recovery covers this loader
table, not all dynamic/delay/internal imports, TLS or VMP startup. Image base and
original section layout still need separate validation: for example, a captured
call refers to `0x4474E00C` while the recovered GetProcAddress slot is RVA
`0x40E00C`. Export resolution does not explain that address difference. No
speculative base or layout change was made here.

## Reproduce extraction

```powershell
py -3 tools/recover_vmp_imports.py primordial.bin --output analysis/imports/vmp-name-candidates.json
py -3 tools/recover_vmp_imports.py primordial.bin --decode-key 0x357b2a2d --table-rva 0x10ad0cf --output analysis/imports/vmp-decoded.json --emit-manifest primordial_loader/src/imports_vmp_manifest.inc
```

The tool reads input bytes only. Review the input identity and generated profile
before rebuilding. Retained JSON contains the decoded records, rejected submitted
map, name-key candidates, IDA target/entry observations and source hashes.
