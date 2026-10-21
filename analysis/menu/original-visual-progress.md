# Original visual recovery, 2026-10-21

The user confirmed **both fire and smoke colours change** in actual CS:GO11188.
Normal loader: `build/original-visual-particles/primordial-x86-native/primordial_loader.dll`,
SHA256 `2431590dc5d5fee6ce0aec88ee1e858323449de964a80e6efec07e46edc4d389`.
`particle_features.cpp` installs MinHook directly to complete original155F40 and155C30;
every original instruction and6CF70 helper remains unchanged. The actual Source
initializer receives original native RGB vectors; no replacement feature algorithm
or particle renderer exists. Installation receipt: `particle-live-11188.json`.
The removed mouse hook remains absent. Steam/IDA preserved; no push.

## Verified dependencies prepared after the confirmed build

- `original_team.cpp` executes guarded copies of original36B2B0,36ACC0,A06C0,
  36ABF0. Only verified old field operands and relative calls change. Actual Source
  bindings/modes enter scoped storage and all touched globals restore, including
  exceptional exits. It is currently used by isolated tests, **not a live chams hook**.
- Current RecvTables prove entity teamF4, survival10410, resource teamF38,
  life25F, resource alive1140, observer mode3388/target339C. Public ConVar registry
  independently identifies game_type/game_mode/mp_teammates_are_enemies and matches
  original354530 hashes. No desired team/mode values are forced.
- The active map supplied actual entity/networkable method evidence:
  `chams-live-player-methods-11188.json`. Networkable9 reads dormant+E5 after index
  validity;10 returns index+5C, agreeing with actual player indices. Entity-list3
  returns base entity via its real virtual6 thunk. This corrects the earlier
  disconnected-capture limitation; the player resource now exists.
- `chams-current-render-interfaces-11188.json` independently identifies
  VEngineModel016 slot21/current engine+13E6A0. Its real model-info+30 flag reads
  corroborate native152EF0. Slot1 forwards to StudioRender33 for forced material.
  RenderView4/6/9 and MaterialSystem84/115 are captured separately.
- `chams-current-input-11188.json`: actual CCSInput RTTI and methods34/35 prove
  camera vectorAC/B0/B4 and third-personA9. Original134B50 readsB0/B4/B8;
  an original-code adapter must repair this verified layout difference.
- Original134810 requests the two built-in debug materials and AddRefs them;
  original132FF0 owns material selection, ignore-depth passes, alpha and colour.
  Isolated tests execute these intact routines with explicit test-only dependencies.

Latest dependency build: `build/original-visual-team-tests/validation.json`,
**34 CTests pass** (28x86,6Steam). Native cases:13 projection,24 particle,
5 material initialization/fault,23 team/observer/indirection/restoration,
11 chams material/category/hidden/scoped/fallback cases. This build is not injected;
the user-confirmed particle build remains in11188. Test-only allocations at old
operand addresses in the isolated chams-pass executable must never be used live.

Completed projection, particle, material and team/observer routines are named,
typed, commented and read back in live IDA/index. Native132FF0 is retained as
`original_chams_entity_material_passes`; its exact outer entity/model categorization
remains separate research. Latest IDA disk save is unverified.

## Remaining work before player wallhack tests

Player ESP requires originalF0B30/F03E0 cache producers before originalF06C0 and
13ACC0->ED330->138130->137310 can consume valid state. Do not fill the records using
new feature behavior. Details: `original-esp-recovery-findings.md`.

Actual chams dispatcher133470 has an empty original script-vector branch in11188;
all19 vectors and registry are naturally empty (`chams-live-script-phase-11188.json`).
This avoids Lua execution without faking context. Nevertheless ordinary model
drawing forwards unless the original manual model phase7DC939 is active.

Native SceneEnd adapter154690 is exactly call[7FB854];JMP134B50.134B50 is the
real stack-aligning entry through13506F;134C00 is an interior phase block with EBP
live-ins, **not a callable entry**. The full root needs current Input camera
operands, EngineTrace, globals/entity interfaces, native iterator/sort/filter state,
immunity/view-offset fields, original camera helpers and a remaining old client
call decoded at13501D..13502E as **2DC6C5F0 on context4DE200**. Its current identity
and contract are unresolved. The old4DE200 pointer is32C0E788; do not rebase it or
skip its call to claim the complete phase works. Current public ClientLeafSystem002
was inspected as a lead, but its object/role does not establish this old call.

Within133470, the method25 query on state+8 chooses132FF0 versus133C90 during the
manual phase. The current live renderable25 reads an owner/parent handle; retain
this precise observation rather than assuming an old virtual method role matches.
Dispatcher ragdoll/owner/resource branches also require verified class IDs and
operands. No full SceneEnd/model hook has been enabled yet.

User-approved references are investigation leads only: hazedumper,
ValveSoftware/source-sdk-2013 and morgandusty/Osiris-release. Osiris reference clone
is pinned at b76ece65c0d622f61f41ab904379ad021af42ad8 under build/references;
no feature implementation was copied. Source SDK interface descriptions do not
prove CS:GO virtual slot compatibility; actual installed code remains authoritative.
