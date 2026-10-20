# Additional protected candidate prefixes (offline only)

2026-10-20. Uses the prepared PID21728 image and the explicitly synthetic
GlobalData CPU-guard model documented in `protected-guard-contract-findings.md`.
No target calls, writes, debugger actions or IDA/index edits. None of these
functions has a verified call ABI; prefix behavior with synthetic arguments
must not be treated as observed target execution or a complete reversal.

Evidence:

- `protected-coordinator-candidates-offline-21728.json`
- `protected-candidate-1564b0-offline-21728.json`

Current import identities were compared against the prepared image's339
destination DWORDs plus their manifest keys, not inferred solely from addresses.

| Candidate RVA | Modeled CPU guard | First boundary / concrete effect |
|---|---|---|
|3542C0|Pass; reads+B8 and+20|CurrentCRT malloc75830130, size10h. Also reads current target page27CF9000 through native helper150432. No bootstrap effect established.|
|360080|Pass; reads+B8 and+20|Eventually transfers to0 withEAX0, through native6C7D7. This differs from guard rejectionDEADC0DE; synthetic arguments/global state remain unresolved.|
|360790|No CPU guard reached|Only19 instructions; mutated RET transfers toC7326207. Initial synthetic return address was12345678. No function ABI or bootstrap role established.|
|36E110|Pass; reads+B8 and+20|CurrentKernel32 VirtualProtect77730760 with arguments(0,1000h,40h,&emulated_old_protect). Null address can be caused by absent synthetic arguments; no supported call inferred.|
|1564B0|Pass; reads+B8 and+20|After425128 instructions, reaches currentVCRuntime memcpy/memmove562F6520 with source string`m_vecVelocity`, length13. Calls existing known string-assign helper6D7A0.|

For1564B0 the first external call has emulator stack:

```
return = 443AD7D2 (inside existing string-assign helper6D7A0)
dest   = 1000FF28 (emulated stack)
source = 1000FFCA (emulated stack)
length = 0D
bytes  = 6D 5F 76 65 63 56 65 6C 6F 63 69 74 79 00
text   = m_vecVelocity\0
```

Thus1564B0 is a concrete lead for netvar/property processing. This does not yet
establish whether it is initialization, lookup, a hook installation coordinator
or a per-frame callback. Only the shared CPU fields are read before these first
boundaries; no other GlobalData fields were fabricated.

Reusable invocation:

```
import protected_entry_probe as probe
with probe.Process(21728) as process:
    result = probe.probe(process, prepared_image_bytes, 0x699000,
                         candidate_rva, 0,
                         {0xB8: 0x59DB1540, 0x20: 0x5C72AE30})
```

The constants are specific to the documented emulator CPU/current target PEB
build and captured pointer; use `protected_guard_models.py` to recompute the
model hash when those inputs change. Stop RVA0 means continue until an external
code boundary, unsupported memory access or the bounded instruction budget.
