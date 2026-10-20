# First bounded movement backend and linked patterns — 2026-10-20

User requested all gameplay features if possible, will test behavior themselves,
and explicitly asked to link proper replacement patterns rather than assume
pattern changes solve startup. First **test scope is bunnyhop and infinite duck
stamina only**. Other features and the original full bootstrap remain unresolved.
The user subsequently confirmed: "both bhop and stamina works". This is real
gameplay confirmation for these two features only. Other features remain unresolved.

## Actual current game

Existing CS:GO23896 and its user-confirmed menu remain running. Do not assume the
PID persists. Normal loader is still `build/menu-native-input/primordial-x86-native/`
and menu state remains3. Canonical `build/primordial-x86-native/` was not replaced.
`build/movement-first/primordial-x86-native/movement_probe.dll` was additionally
loaded at990000 to check the new backend without restarting this game. It imports
real menu readiness from the already loaded normal loader; it constructs no menu,
loads no dump, and does not rerun original startup. It shares the production
movement source. Hook installation passed all profile/field checks, state1.
Latest command/change counts are0/0: no valid map callback or gameplay effect
has yet been observed. The installed ClientMode callback begins with its new
MinHook jump. No original gameplay hook/event initializer was called.

User was told to join a local map, enable bunnyhop/duck stamina, and close the
menu with Insert before testing. Controls default off. No settings were toggled
programmatically. `py -3 tools/capture_movement_state.py` reads state/counters,
menu visibility and the two checkbox bytes without changing target memory.

## Production integration and verification

`movement_features.cpp` is also built into the normal loader and installed after
menu-hook installation. The probe is optional (`build.ps1 -FeatureProbe`), not
required for normal fresh-process injection. Both DLLs use the exact same source.
Do not install a second movement hook into the already instrumented test process.
Normal replacement artifacts are `build/movement-first/{primordial-x86-native,
steam-x64-native}`; keep loader/TLS adjacent. They are compiled but the normal
replacement DLL itself has NOT been game-loaded. No canonical binary was replaced.

All25 native CTest checks pass (19x86,6Steam), plus5 Python provenance tests.
`movement-original-x86` executes the original bunnyhop decision excerpt
F8674..F870E in an isolated x86 test process. Local modeled entity/offset/getter
fixtures replace only its documented dependencies;320 cases agree with the
bounded backend. This is not full original movement-coordinator execution or
real gameplay validation. Separate tests verify death, ladder/noclip/observer,
water, unrelated-button preservation and checkbox-off behavior. The backend
does not alter commands while the original menu is shown.

Current VClient018 slot10 bytes expose ClientMode via the actual HudProcessInput
tailcall. ClientMode24 target is client RVA276D20. The actual CInput caller at
RVA305C76 pushes command/sample time and calls virtual+60; later native code
copies the verified command and computes CRC at307260. The backend calls the
original callback, preserves its boolean return and changes only buttons+30.
It does not change angles, weapon selection, packet transmission or verified CRC.
Installed Source file hashes, exact callback/caller/getter profiles and interface
identity are checked before installation. On mismatch the menu remains usable.

Receive tables are traversed from the actual CCSPlayer class, not guessed static
offsets. Current unique fields: health100, flags104, life25F, water25E, render25B.
Move type is render+1, corroborated by the reference NETVAR_OFFSET and installed
native byte access at client42C129. The original menu controls are identified by
vtable447505D4 and their XOR-decoded labels before their enabled bytes are used.
Names, source snippets and current field checks are retained in the workspace;
full-source-game patterns are not silently copied into Primordial globals.

## Patterns linked to actual consumers

`tools/audit_feature_patterns.py` checks the user-supplied OsirisAndExtra reference
at commit c85f8301027b3707a1faf0f6c0352518fdc431b3. In this installed game,115 client/
engine reference patterns produce109 unique candidates and6 ambiguous results.
Zero are missing. A unique match is explicitly NOT treated as a Primordial link.
Class RTTI also identifies the primary C_CSPlayer vtable and corroborates several
animation/view reference targets. They remain unbound until original-consumer
semantics are established. The vector-output slot285 is a candidate for155BD0;
class/ABI similarity alone was not used to write its cleared encoded pair.

Downloaded hazedumper config SHA256
bd4e995cc44f104308981ac8296584aede2684aca0077676cb6158c78dce360a:
all58 patterns already match this project's diagnostic catalog exactly.
Replacing that catalog would not wire original feature consumers.

One replacement IS linked: current GAMEEVENTSMANAGER002 public slots8/9 each
call private engine RVA211C70. The old18-byte signature matches0 times; new18-byte
prefix matches exactly once. Original consumer is compiled masked scanner155840,
return sites155352/155375, detour155AD0, original slot7FB8EC. This is distinct from
ASCII scanner21E560. `event_pattern.cpp` adapts only that exact old pattern/mask0,
those caller sites and a wholly contained current engine executable scan range.
It validates installed Source hashes, the entire original helper SHA256, both
public-wrapper calls and uniqueness before enabling its MinHook adapter.
Other patterns and ranges preserve original scanner behavior and end bounds.
This adapter is in the new normal DLL, NOT loaded by movement_probe.

Native tests execute the original18-byte scanner to check output/RET12 ABI and
the replacement. Python tests reject changed factories, diverging public calls
and duplicate targets, and check overlapping/wildcard matches. The updated
OFFLINE1564B0 experiment with `--adapt-event-signature` completes its modeled
failed-hook inventory with24 requests/6nulls instead of7nulls. No real event hook
or registration results from that experiment.

## Original startup blockers now identified more precisely

Original F8510 reads the bunnyhop, fast-stop and duck-stamina controls and is
called at379A58 by native callback3798C0. F92D0 reads the autostrafe checkbox and
smoothing value, with further per-command state/math dependencies. The whole
original CreateMove callback is NOT installed: its CInput buffer reads use+F4/F8,
whereas the current game uses+F0/F4, and it still calls captured client checksum
address2DBE2740. It also reaches many unrecovered feature roots. These are actual
layout/operand dependencies beyond finding a replacement hook function.

Several other native consumers dereference old-client instruction/header
locations to obtain offsets. For example155B80 reads2DAA11A5 before an entity
write; helper36A7C0 reads2D906360/2DAA0FB0/2D8FBEC0 before vector accesses.
Do not blindly rebase these operand locations or install those callbacks merely
because a reference function pattern has one match.

Helper A06C0 is fully retained live as `refresh_cached_local_player`, verified
thiscall prototype/comment: EngineClient12 -> ClientEntityList3, writes context+2C,
returns entity, plainRET. Seven scanner/partial-function notes were appended and
read back, indexed by tools/retain_pattern_consumer_findings.py. Existing names
and comments were preserved. Legacy IDA comment readback truncates at1024 chars,
so new bounded notes are on the next instruction. Latest database disk save is
unverified. Full movement/autostrafe/core callbacks remain partial, not renamed
or assigned speculative prototypes.
