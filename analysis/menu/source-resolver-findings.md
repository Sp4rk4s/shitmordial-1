# Source module resolver correction — 2026-10-20

The six hashes in the supplied report are ordinary FNV-1a of Source engine
export names, independently matched against the installed x86 DLL export tables:

| Hash | Module | Export |
| --- | --- | --- |
| DA7D68CB | vstdlib.dll | KeyValuesSystem |
| 90354280 | vstdlib.dll | RandomFloat |
| FA877C5B | vstdlib.dll | RandomInt |
| 3F32C6A3 | vstdlib.dll | RandomSeed |
| 1EF9492B | tier0.dll | AllocateThreadID |
| A7043A9E | tier0.dll | FreeThreadID |

Evidence: `source-export-hashes.json` retains both installed export-name lists
and their hashes. The normal FNV basis/prime are 811C9DC5/01000193. The supplied
claim of VMP-private identifiers is disproved for all six listed values.

The report also confused analysis and runtime bases when reading resolver data.
At original dump RVA4DDF78 (runtime4481DF78), the first DWORD is 990704D7, the
FNV-1a hash of `vstdlib.dll`, followed by captured HMODULE7B010000. RVA4DE068
(runtime4481E068) begins with 2935FA11 (`tier0.dll`), handle7AD00000. These records
are not all zero in the original dump.

There are twelve contiguous 40-byte resolver objects at RVA4DDF28..4DE108.
All twelve module identifiers match installed/loaded DLL names; see the guarded
table in `primordial_loader/src/game_modules.h`. Current module paths/bases are
retained in `runtime-27952.json`.

IDA analysis4409F790 (`module_export_cache_construct`) reconstructs only the
unordered_map at object+8. It leaves module hash +0 and HMODULE +4 untouched.
Analysis4406D450 (`module_exports_visit_fnv1a`) reads module at this+4, enumerates
the PE export directory and hashes names using ordinary FNV-1a. Analysis4406D3B0
(`module_export_resolve_fnv1a`) accepts ECX=this and stack(out*, hash), returns
out* and pops eight bytes. The decompiler's previous char hash type was wrong.
Names/comments were applied and read back in the matching live IDA database;
annotations are also retained in `analysis/ida/project-index.json`. Saving the
live database to disk remains unverified.

The earlier whole-BSS reset accidentally erased these module identifiers.
PID27952 confirms zeros in the vstdlib object's first eight bytes even after CRT
constructors, despite a fresh cache container. This is a confirmed repair
regression, independent of the unresolved native menu bootstrap.

The patcher now validates all twelve original module hashes before any startup
writes, retains only those four-byte identifiers while clearing captured BSS,
and transactionally binds all twelve current process HMODULEs before entry.
It never reuses captured HMODULEs, hash-map heap pointers or cached exports.
Missing game modules produce a named error before entry; binding does not load
game DLLs. Existing 339 VMP import fixups remain active separately.

Tests cover preserved hash inputs, cleared handles/cache state, fresh binding,
missing-module failure without partial writes, repeated-binding rejection, and
bad-original-hash failure without startup writes. Tests do not execute the dump.

The reported g_aHooks/g_aOffsets/g_aInlinedOffsets/g_aNetOffsets names are absent
as plaintext in the unchanged dump. Their alleged loader-export contract remains
unverified; absence of plaintext alone does not disprove encrypted lookup.
No fabricated export arrays or guessed native bootstrap call were added.

CSGO PID27952 remains running with the previous canonical build. New binaries
are built separately under `build/resolver-fix/` to avoid overwriting loaded
DLLs. A fresh game mapping is required to test this correction. Menu success is
not established by entry returning TRUE or isolated tests.
