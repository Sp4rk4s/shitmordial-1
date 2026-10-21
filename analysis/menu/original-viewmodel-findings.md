# Original viewmodel-FOV recovery, 2026-10-20

## Gameplay-confirmed FOV recovery

The user answered **"Both change correctly"** for render/viewmodel FOV. The
current session preserves CS:GO5336 and the confirmed movement normal loader.
Only the optional `build/original-fov/primordial-x86-native/fov_probe.dll` was
additionally loaded at33200000. It constructs no UI, reruns no startup and hooks
distinct ClientMode18/35 targets, leaving ClientMode24 movement intact.

Readback after confirmation:9129 camera and9129 weapon callbacks, state2
(active), world90->123 at render slider33, weapon60->43 at viewmodel slider16.
This agrees with original world-FOV addition and original weapon compensation
90-world+baseline+slider. Menu state3 and movement state2 remained healthy,
movement commands63942/changes4181. Non-atomic telemetry supplements the user's
visual confirmation; it does not establish other camera features work.

`original_fov.cpp` copies and SHA256-guards original callback14FE90 and all three
native dependenciesA1CD0/36ABF0/369370, relocating only external rel32 calls and
the verified old Source field operands. It also lifts original render-add leaf
1500BD..1500DB with a wrapper supplying/preserving ESI. The leaf itself reads
camera+B0, adds original slider531EF8 and writes back; its intervening AL read
at camera+148 is preserved. The original entry gate (resolved view entity)
is retained. Scope/zoom/third-person blocks are not enabled or recreated.

The Source adapter verifies installed module hashes, actual ClientMode getter,
method18/35 identities/prefixes, original slider labels and live receive-table
fields. Slot35 role is independently corroborated by its viewmodel_fov ConVar.
Player-resource slotclient+3231380 is checked against its read instruction,
constructor writer and null-writing destructor, plus C_PlayerResource RTTI.
The actual current Source pointer slot is supplied even when its object isNULL.
The native resource/alive/observer/weapon code then runs unchanged apart from
field-address operands. No test dependency implementation is linked into either
production DLL or optional FOV extension.

During a callback the adapter scopes/restores engine/entity/resource-slot,
camera, cached-local and saved-original globals. The view hook supplies the
actual current camera argument. Outside that argument's lifetime, exact bytes
copied from the real camera are retained for same-thread weapon queries; no
camera values or synthetic padding are invented. Source original callbacks are
forwarded; the original feature code performs all FOV arithmetic and exclusions.
Runtime camera field90 and both independently changed values were observed.

Twenty production-capsule viewmodel cases and five render cases pass, with
original player/observer/weapon helpers and explicit isolated public API
fixtures. Profile mismatch rejection and original mapped-code preservation are
checked. All28 CTests pass (22x86,6Steam). Record is local
build/original-fov/validation.json. The new normal loader also includes this
adapter and starts its worker after menu installation; that newly compiled
normal DLL has not been game-loaded. The currently loaded extension uses the
same native backend and binding code.

Build the optional extension with
`tools/build.ps1 -OutputDirectory build/original-fov -FovProbe`.
Inspect using `py -3 tools/capture_fov_state.py --pid 5336`. Do not load a second
copy or replace a pinned extension in place. Native leaf comments were retained
and read back in live IDA/index; latest database disk save is unverified.

Test sliders separately: original weapon-FOV code compensates for world FOV,
so equal additions can cancel its numeric change. Original null-player guards
also deliberately retain defaults outside a loaded map. Load a local map and
use `joingame; jointeam 2` before comparing the actual views.

## Latest dependency follow-up

The test now runs original A1CD0 (local/observer resolution),36ABF0
(alive/resource fallback) and369370 (weapon handle lookup), with guarded old
field operands redirected to explicit fixture offset cells. Those original
helpers are no longer replaced by pointer-return stubs. Source public engine,
entity-list and networkable methods remain isolated API fixtures. Twenty native
cases pass, including living-local precedence, observer mode4 target selection,
mode6/0 exclusions, invalid/null target fallback, and dormant/resource-alive
branches. All28 CTests pass; this remains isolated evidence, not a live FOV test.

Native execution exposed a required indirection: payload4DE1B0 must hold the
ADDRESS OF a player-resource pointer slot. Even when the object is absent,
that slot address must remain readable. Setting the payload field directly to
NULL or to the resource object is wrong. The initial expanded test faulted on
the original dereference at36AC31 until the fixture represented this contract
correctly. No game process was modified or faulted by that test.

Current Source RecvTables independently expose m_bAlive at1140 in
DT_PlayerResource, inherited by CCSPlayerResource. The existing diagnostic
pattern has one current match atclient+74DF3D and yields pointer slot
client+3231380 (runtime5A351380 in5336). The object wasNULL at capture, so a
live object RTTI check is still pending. Separate read-only Source instruction
inspection found a nonnull writer atclient+23C729 and null writers at23C746
and3F3BE6. Those null-writing destructor paths install the primary vtable
whose RTTI is C_PlayerResource. This corroborates the slot role without
equating one pattern match with a complete binding proof.

A1CD0 is retained in live IDA as resolve_local_or_observed_player and36ABF0 as
player_alive_with_resource_fallback, with verified native ABI, pointer-slot
contract, globals/branches and uncertainty comments. Readback/replay index are
updated; latest IDA disk save remains unverified. No visual hook installed;
the working movement game remains untouched. Camera setup/callback ordering
and final production bindings are the next verification step.

Movement gameplay confirmation and the mandatory original-binary rule are pushed
on feature/movement-restoration as56d3850 and4f7378e. Subsequent camera work is
preparation only; the confirmed movement game5336 was preserved.

The live original slider532030 decodes to `viewmodel fov add`; its value is at
532160. The sole direct operand reference is inside original callback14FE90,
size92. The separate `render fov add` slider531DC8 has its value at531EF8 and
is consumed by14FF30; that larger camera callback includes additional unresolved
camera/weapon/third-person dependencies and must not be activated wholesale.

Native14FE90 first calls saved original callback7FB6B8, retaining the returned
x87 float. It resolves a view entity viaA1CD0, requires current camera setup
4DE1E0 and weapon lookup369370, and reads scoped/item-definition fields through
old Source operand locations2DBDB5D3 and2DA842AE. Scoped item39/8, item49, or
missing entity/view/weapon return the original baseline. Otherwise the original
instructions compute90-cameraFov(view+B0)+baseline+slider532160.

IDA decompilation incorrectly folds mutable slider storage into garbage floating
constants. Native assembly and native tests establish the actual arithmetic;
do not translate those folded pseudocode constants into code.

`tests/original_viewmodel_tests.cpp` executes the original full92-byte callback
at its expected base. Twelve cases cover every callback branch: missing entity,
view or weapon; scoped/unscoped item8/39; item49; varied camera FOV, slider and
baseline values. Only the two field-address operands are redirected to explicit
fixture cells. Original source-baseline, view-entity and weapon lookup dependencies
are TEST fixtures; they are not live implementations or production substitutions.
No new feature formula supplies the callback's result. Expected numeric results
are test assertions only.

All28 CTests pass in build/original-viewmodel (22x86,6Steam). No visual feature
hook has been installed and this build is not injected. The live game retains
build/original-gameplay and its confirmed original movement behavior.

Read-only current Source evidence corroborates ClientMode35 as the viewmodel-FOV
target: methodclient+278BD0 reads the ConVar objectclient+5245C18, whose name
pointer at+C resolves to `viewmodel_fov`. The method returns x87 floats and also
handles player-specific adjustments. Source35 and original callback14FE90 each
take no stack arguments and forward incoming ECX to their respective callees.
The original startup-to-vtable hook linkage remains pending; do not present this
as recovered full hook installation.

`tools/capture_visual_fields.py` walks the actual current Source class list and
receive tables without executing target code. It verifies:

| Field | Current offset |
| --- | --- |
| CCSPlayer m_bIsScoped | 9974 |
| CCSPlayer m_hActiveWeapon | 2F08 |
| CCSPlayer m_lifeState | 25F |
| CCSPlayer m_iObserverMode | 3388 |
| CCSPlayer m_hObserverTarget | 339C |
| Weapon m_iItemDefinitionIndex | 2FBA |

These are current table facts, not an automatic mapping of every old operand.
A1CD0 includes dead-player observer-target fallback and calls36ABF0; the latter
uses a player-resource dependency and additional old Source operands. The current
camera setup must also be captured through a verified original/current caller.
Resolve/test those dependencies and preserve the native gates before enabling
the original callback. Do not substitute an agent-authored FOV calculation.

Original14FE90 is retained in live IDA as original_viewmodel_fov, with its
no-stack-argument/x87-return ABI and ECX forwarding recorded. Native weapon-handle
lookup369370 is retained as lookup_player_weapon_handle_entity; its exact current
field mapping remains a separately stated dependency. Names/prototypes/comments
were read back and indexed. A1CD0 and36ABF0 remain partial dependencies, not fully
reversed/renamed functions. Latest IDA database disk save is unverified.
