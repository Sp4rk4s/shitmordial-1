# Captured alphabet string write failure

Observed paused csgo PID 17904, thread 19884. Handoff protocol2, runtime image
base0x44340000. The prior CRT-state failure is passed; C++ initializers are running.
No target memory changed, thread resumed, or process terminated during inspection.

Runtime copy fault: vcruntime140!0x529B6A09, EDI=0x431F76D8, ESI=0x4479E408,
ECX=0x3F. Destination VirtualQueryEx: MEM_FREE, PAGE_NOACCESS; source lies in
our committed image. Stack return into string assign is 0x443AD7D2, followed
by the alphabet initializer's return0x44344691 and CRT initializer-loop return
0x44709D9E. Complete bounded registers/stack/frame chain are in memcpy-17904.json.

The dump has exactly one literal pointer0x431F76D8, at RVA0x4DB634 (runtime
0x4481B634, IDA0x444DB634). It is a24-byte MSVC x86 narrow string object:
heap pointer at+0, size63 at+16, capacity63 at+20. Initializer RVA0x4680 (runtime
0x44344680, IDA0x44004680) directly assigns literalRVA0x45E408, length63:
abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 plus a space.
Constructor-table recordRVA0x410130 references this initializer. Because the
captured capacity is sufficient, string assign reuses the captured pointer,
then crashes copying to memory belonging to the original process.

startup::prepare now checks all24 captured bytes before any startup writes,
then restores only this object to empty inline storage: zero buffer/size,
capacity15. It never frees the captured pointer. The initializer can allocate
new storage using its existing allocator. This does not restore all captured
heap objects; other heap globals in the dump remain unverified.

Regression validation: 18 tests pass in build/string-fix and in the canonical
build paths. The captured CRT test checks exact object layout, performs native
MSVC string assignment with the repaired empty representation, and confirms
all unrelated image bytes remain unchanged. A changed alphabet profile fails
without partial startup writes. The real protected payload was not rerun with
this new patch during this turn.

IDA: msvc_string_assign_bytes renamed/prototyped/commented at0x4406D7A0, verified
by readback. Initializer, object and table record have live address comments.
The initializer was not defined as a function in the existing database; the
legacy bridge cannot create functions. project-index.json retains a guarded
pending definition and tools/ida_cache_project.py can define/name/type it when
run inside IDA. Replay script syntax checked, not executed in IDA. On-disk
IDA save unverified. Original dump and existing analysis remain intact.

The canonical DLL paths are now rebuilt successfully (previously locked stale
outputs were no longer locked). Use build/primordial-x86-native/primordial_loader.dll
for the next fresh CS:GO process. Already loaded protocol2 Steam stager remains
compatible; the new string fix is entirely in the patcher.
