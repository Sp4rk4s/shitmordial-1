# Feature startup recovery — 2026-10-20

Superseded test status: see [first movement feature tests](first-movement-feature-tests.md).
Two bounded movement features are now installed for user testing in the preserved
game; whole original bootstrap remains unresolved. The event-pattern correction
reduces null targets7->6 in the newer OFFLINE inventory; it does not activate the
original startup or event hook. Historical results below remain evidence.

The user requested all mod gameplay features if possible and will perform actual
feature tests. **No gameplay feature build is ready for testing yet.** The
successful menu remains in CS:GO23896 with the existing menu-native-input loader.
No game restart, new live context binding, original hook installation, event
registration or protected subsystem call was performed in this follow-up.

## Concrete prerequisite repair: fourteen module objects

The original manifest stopped at twelve objects. The original dump actually
contains fourteen consecutive40-byte CModule records ending at RVA4DE158:

|RVA|Module|FNV1a hash|Captured old HMODULE|
|---|---|---|---|
|4DE108|panorama.dll|35153F28|1B290000|
|4DE130|localize.dll|FC70FD90|1BAB0000|

The names independently hash to the captured identifiers. Native CRT wrappers
RVA549A0 and53E00 each call the already retained
`module_export_cache_construct`9F790 with the corresponding object, register a
destructor with atexit, and return. The shared constructor starts at object+8,
so neither hash nor module handle is recreated. The22-byte wrappers and their
destructor references3EA520/3E9E90 are retained as guarded pending definitions;
live address comments include purpose and proposed prototypes.

The previous BSS reset preserved only twelve identifiers. Actual23896 has
hash/handle0/0 in both additional records even though both DLLs are loaded.
`game_modules.h` now validates/preserves/rebinds both additional modules along
with the original twelve. Publication remains transactional: all modules must
resolve before any handle is written. `tools/test_menu.py` waits for fourteen
modules, and `capture_payload_state.py` snapshots all fourteen records.

The startup test now explicitly verifies both new hashes/current handles, rejects
mutated new identities without image changes, and verifies that missing panorama
or localize near the end of lookup causes no partial handle publication.
All22 checks pass (16 x86,6 Steam) in `build/feature-resolver-14/`.
`validation.json` records source/artifact identities and the NOT game-loaded
status. Loader SHA256:
`935d549e54198185961a93296676a3e680e4e12241403f636f6255e572fadf3f`.

This is a real prerequisite repair, **not a gameplay bootstrap fix**. It was not
copied over the canonical successful menu binaries or injected expecting features
to appear. The current game and canonical artifacts still use the user-confirmed
menu-native-input build. A future default source build includes the new repair.

## Protected hook/event routine now bounded through return offline

The reused current-process capture helpers operate read-only. The Source factory
capture now optionally accepts additional loaded DLLs with `--module`; the same
verified registry-wrapper checks apply. Current snapshots include filesystem,
materialsystem, studiorender, vphysics and vstdlib registries in addition to the
five menu-related DLLs.

`analysis/menu/probe_feature_startup.py` replays copied process state in Unicorn.
It never calls target code or writes target memory. The CPU guard and allocations
are explicit emulator models. Hook helper1554F0 is byte-guarded against original
bytes and deliberately modeled as its allocation-failure return: zero record,
no original callback, no Source write, no successful trampoline. This lets the
trace inventory requests whose original decoder would otherwise dereference
invalid/null targets.

The following explicit **offline** context substitutions advance the routine:

|Payload field RVA|Observed current object / evidence|
|---|---|
|4DE168|VClient018 constant factory; slot37 request targets current57382B20 with detour14F740|
|4DE16C|VEngineVGui001, as established by original GetPanel usage; **not** IPanel|
|4DE198|ClientState from independently checked EngineClient26/78 getters; secondary vtable yields6C316110 /detour150350|
|4DE1AC|VEngineRenderView014; native1330xx blend/color calls corroborate slots4/6; slot9 yields6C208F40 /detour154690|
|4DE1FC|VFileSystem017; also consistent with retained localization file consumers|
|4DE188|GAMEEVENTSMANAGER002; native3560A0 registration uses virtual6(listener447F3D54,0)|

These roles are retained with their evidence and uncertainty; they were not
written into the live payload. Real Source event registration reaches locking
boundaries in emulation. An explicit hypothetical-success registration model
allows subsequent callback-map construction to be inspected without registering
anything or invoking feature callbacks.

With those interventions,1564B0 returns to the caller sentinel with balanced
stack after5,780,155 instructions. It requests24 inline hooks; seven targets
are zero. No additional CPU-context fields are read beyond the previously
recovered two on this path. This completes **one modeled path** through the
routine, not its startup ABI, actual installation, main gameplay initialization,
or proof of feature behavior. Failed hook records can also affect later branches;
the experiment does not substitute for a successful-installation trace.

Reproduction after current read-only snapshots:

```powershell
py -3 analysis/menu/probe_feature_startup.py --entry 0x1564B0 --dry-hooks --bind-client-panel --bind-client-state --bind-field 0x4DE1AC:engine.dll:VEngineRenderView014 --bind-field 0x4DE1FC:filesystem_stdio.dll:VFileSystem017 --bind-field 0x4DE188:engine.dll:GAMEEVENTSMANAGER002 --model-event-registration --max-steps 16000000
py -3 tools/capture_feature_readiness.py
```

The `--bind-client-panel` legacy flag actually binds VClient018 and EngineVGui in
the latest script; its explicit JSON records the interface names. Earlier
client-panel-only experiments included an incorrect IPanel hypothesis, corrected
before the final replay. Do not use an earlier hypothesis as verified binding.

## Seven missing hook targets

`feature-readiness-23896.json` joins the modeled inventory with read-only current
original callback slots, interface fields and all fourteen module records.
Original callback values remain zero live. Targets below were **not** hooked.

|Detour RVA|Original-pointer slot runtime VA|Missing source|
|---|---|---|
|155AD0|44B3B8EC|Old engine signature no longer matches|
|155BD0|44B3B8E4|Zeroed XOR pair RVAs8074F0/F4|
|153710|44B3B940|Zeroed XOR pair8074F8/FC|
|1532B0|44B3B7FC|Zeroed XOR pair807508/50C|
|155B80|44B3B8C4|Zeroed XOR pair807510/514|
|156260|44B3B8C8|Zeroed XOR pair807468/46C|
|1529E0|44B3B79C|Zeroed XOR pair807838/83C|

The original six pairs decode captured addresses2DCF53C0,2DCB6E90,2DCFB7A0,
2DCB8910,2DB71CA0 and15B41750. Captured module handles place the first five in
old client.dll at2D8F0000 and the last in old engine.dll at15910000. They are
old process addresses, not safe current pointers or demonstrated stable RVAs.
Do not restore them or blindly rebase them to the installed game.

The existing earlier `game-event-private-target-38484.json` provides a concrete
lead for the first signature: two public event wrappers converge on a changed
private dispatcher. The new probe deliberately retains the old signature rather
than silently adapting it. Callback155AD0's inspected native wrapper forwards
three stack arguments and preserves ECX/EDX, RET0C, optionally calling356130 when
third argument is true; containing semantics still need review before activation.

## Retention and remaining work

Two helpers were named, typed, commented, read back and indexed live:
`inline_hook_create_record`1554F0 and
`hash_keyed_pointer_cache_get_or_insert_null`354170. The latter allocates a
zero-valued cache entry on a miss; it does not itself resolve interfaces.
Partial dependency comments and fourteen-module evidence are applied/indexed.
Two missing CRT function definitions remain guarded pending replay because the
legacy API cannot create them; live address comments retain their complete
wrapper findings. Receipts: `feature-startup-helper-ida-applied.json` and
`feature-startup-notes-applied.json`. Latest IDA disk save remains unverified.

The original inline helper performs bespoke instruction decoding and raw
trampoline copying; relative instruction relocation and concurrent patching are
not demonstrated. Its zero-target decoder has no null check. It should not be
blindly invoked on the running game, even with some corrected addresses.

Further work must establish current targets for those six encoded entries,
verify/adapt the event dispatcher target, and recover the core gameplay callback
installation plus full context/offset initialization. A complete model return,
another resolver build, or menu visibility does not establish any gameplay
feature. The user will perform feature tests once an actual feature build exists.
