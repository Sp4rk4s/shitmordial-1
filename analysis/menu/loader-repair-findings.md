# Loader state and static TLS repair

The original TLS directory is RVA `0x461100`: template VA `0x447A46D4`
through `0x447A46DC` (eight zero bytes), index VA `0x44B66264`, empty callback
array VA `0x44750204`, zero fill 0, characteristics `0x300000`. Linker GCTL
metadata at RVA `0x464358` corroborates `.rdata$T`, `.tls` and `.CRT$XL*`.
Lua setup at analysis VA `0x44167332` accesses `FS:[0x2C]`, selects this index
and writes the Lua state at block+4. Reusing captured index zero is incorrect.

The patcher now loads its own `primordial_tls.dll` companion normally through
Windows. It has the recovered eight-byte template, an index assigned by the OS,
no CRT, and no payload code or TLS callbacks. Its tiny DLL entry retains a real
Kernel32 import. On the tested Windows 10 build an importless version's index
was not initialized; retaining the import corrected this in isolated tests.
The patcher copies the assigned index into the existing payload mapping. The
companion stays loaded. No thread-local array is replaced or manually freed.
Tests verify isolated storage in the caller, a thread created before loading
the companion, and a thread created afterward.

Linker metadata also identifies `.bss` at RVA `0x4DDBE0`, size `0x348A74`, end
`0x826654`. Captured counts survive static constructors that assume zeroed BSS.
In live PID 9928 the interface cache head changed from the dump's `0x3BEEA588`
to `0x33E7ED20`, while the captured count at runtime `0x44B486E8` remained 29.
The worker count at `0x44B49564` remained 11. The interface resolver's count
guard can suppress rebuilding the cache for the current process.

Startup preparation now clears this exact verified BSS range, preserving the
loader input `[0x4DDCE0,0x4DDEE5)` (512-byte username, DWORD ID, mode byte).
The cookie pair outside BSS is preserved. The alphabet string repair remains.
This supersedes the earlier rule to preserve all captured BSS. Import binding
and entry execution still operate only on the existing Steam mapping.

All 19 tests pass in the canonical build paths: 13 x86 tests and six Steam
launch tests. The new test covers static TLS, and the dump test checks BSS,
interface/worker counts, user-input preservation and unrelated-byte preservation.
The actual game menu has not yet been verified with these changes. Do not
report entry success or passing isolated tests as proof that the menu works.

Additional evidence: `constructors.json`, `interface-root-candidate.json`,
`ctor-helper-candidates.json`, and the earlier `analysis/startup-crash/` reports.
The recovered VMP section table at RVA `0x1293864` describes only non-writable
original sections; it is not a complete replacement PE section map.
