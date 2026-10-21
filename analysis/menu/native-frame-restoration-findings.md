# Original menu frame restored — 2026-10-21

The user confirmed **“Renders and controls work”** in fresh CS:GO10260. The
loaded normal loader is `build/native-menu-frame/primordial-x86-native/primordial_loader.dll`,
SHA256 `490689ac9dedf655d517ff801323d96cd76ec4d3bec54a8f5a221bf34af0f28c`.
It executes the intact original frame at RVA32BA40. The manually reconstructed
C++ frame coordinator has now been removed from source; ordinary builds use
this native path without an optional mode or separate preview DLL.

## Native behavior and dependency repairs

The existing Source panel callback supplies the current drawing context and
actual input. Original32BA40 now performs window ordering, centering, focus,
search shortcuts, active-control dispatch, deferred callbacks, native cursor
selection/capture publication and periodic asset refreshes. Window/control
update and rendering remain original3504B0/351AB0 and their native callees.
The accent copy540138→4B33A8 remains in the adapter because native panel1550AA
performs it immediately before calling32BA40.

Original one-time initializers2CAB40 and162FA0 run before original menu2E04D0,
with the existing scoped menu-constructor CPU guard, restored immediately
afterward. They establish `primordial/configs` and `primordial/scripts`;162FA0
also creates `scripts/include`. No Lua VM or gameplay startup was substituted.
The recovered path initializer changes only the script-directory string within
the image on the tested offline path; it does not initialize feature hooks.

The original frame's periodic14B330→147ED0 path needs MaterialSystem at4DE19C.
Its original call requests `effects/spark` through virtual84. The installed
`VMaterialSystem080` factory, object and slot84/materialsystem+1AF30 were checked
read-only in the game. Stable PE hash/metadata and relocation-aware code guards
were generated from the actual installed DLL. Production binds the real public
interface, with no material implementation or replacement rendering algorithm.

Two additional captured wstrings require exact ownership repair before entry:

|RVA|Captured pointer|Size/capacity|Original consumer|
|---|---|---|---|
|4B3C74|1D865D48|18/23|162FA0 script-directory assignment|
|4B3C14|417EA748|20/23|147ED0 periodic path/label assignment|

Startup validates all24 captured bytes transactionally and replaces each with
an empty owned x86 wstring. It never frees captured heap addresses. Native code
then performs the real allocations and assignments. Existing startup tests cover
both repairs, byte preservation and refusal on mismatched profiles.

## Verification and first failure

All29 native CTests pass:23x86 and6Steam. A new isolated native test executes the
unchanged complete32BA40 instructions with explicit external dependency fixtures.
Seven cases cover focus-last ordering, original search shortcuts, capture
publication, periodic refresh timing, toggle binding and alternate UI dispatch.
These fixtures are not linked into production DLLs.

The complete retained offline frame returns with ESP10010004 after1092676
instructions. Its original frame bytes remain unchanged. Directory handles,
empty enumeration, CRT allocation, surface output, an opaque material result
and cursor APIs are explicit models, never real pixel or gameplay evidence.
Original162FA0 separately returns balanced after1297475 instructions under
frozen-page/CPU/CRT/empty-directory/ASCII-conversion fixtures.

The first live native-frame attempt in12820 crashed when147ED0 freed captured
417EA748. This repair was already visible in the offline experiment's repair
list but was initially missed in startup. The crash/minidump were preserved;
the complete required repair list was audited and4B3C14 added. The subsequent
fresh-process test10260 succeeded and received the user's visual/input confirmation.

Live readback (`native-frame-live-10260.json`): handoff3, menu3, centered flag1,
unchanged original32BA40 bytes, real current MaterialSystem727D2AC8, fresh strings
`primordial/configs`, `primordial/scripts` and `primordial/hitsounds`. Periodic
refresh time advanced to103.01026. Original Surface LockCursor slot7FB878 remains0:
the removed mouse hook was not restored. FOV callbacks remain active/state2.

The baseline21392 was closed for the fresh-loader test; it did not exit after
WM_CLOSE and required termination. Steam and IDA were preserved. Windows then
quarantined the injector before12820 loaded; the user changed their security
setting and requested reinjection. The same helper was rebuilt without changing
security settings, renaming it, or changing its behavior.

## Retention and practical limits

Original config/script initializer entry stubs now have verified logical names,
cdecl prototypes and evidence comments in IDA. Their discontiguous protected
bodies were not assigned invented contiguous boundaries. User ran
`tools/ida_cache_project.py`, reported its successful database save, and both
names were read back. Frame dependency notes are retained/read back/indexed.

Default source now runs the native frame. The subsequent standard build is
`build/native-menu-default/`; it removes the old coordinator and optional native
mode. The preserved running game uses the equivalent earlier native-frame build,
not this newly compiled file. Validation receipts distinguish the two.

Source interface/font/input bindings are still compatibility adapters. Full
original DllMain/protected game bootstrap and full paint callback154790 remain
unrecovered. This does not claim other gameplay features or script execution
are fixed. A DirectX-only callback is not the recovered menu renderer: this
original path draws through Source ISurface in its panel drawing context.
