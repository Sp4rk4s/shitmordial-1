# Original movement ready for user testing, 2026-10-20

Latest user result: **"your movemant fix works"**. This confirms the original
movement build generally. No separate confirmation for every individual control
was supplied. Subsequent request: commit/push this work and continue recovering
other original features. Earlier pending-confirmation statements below are
historical observations.

## Team-selection follow-up

User reported `jointeam 2`/`jointeam 3` submitted through the console but left
them spectating. Read-only capture showed a fully connected map (signon6),
unassigned local player, health0/life2; original movement had not yet run:
changes0 and its bound entity-list slot4DE170 remained0. Menu was hidden.
This excludes original movement feature calculations as the cause of that attempt;
successful console submission also excludes swallowed Enter for that attempt.

User then confirmed `joingame; jointeam 2` lets them join. Use that initial join
step after map load, or `joingame; jointeam 3` for CT. This resolves the reported
test without a menu or movement behavior patch. It does not establish why the
normal welcome/team-selection flow was skipped, or prove every menu path correct.

After joining, native movement commands47008/changes449, state2 and menu state3
were read back in PID5336. State2 is the current active-player/gated state, not a
fault (fault=4). The menu was visible during this final capture. This proves
native command changes occurred, not that each feature behaves correctly; user
gameplay confirmation for the individual original features remains pending.

Fresh CS:GO PID5336 was launched with `-windowed -insecure -steam` and loaded
`build/original-gameplay/primordial-x86-native/primordial_loader.dll` normally.
Adjacent `primordial_tls.dll` remains required. No preview or movement-probe DLL
is loaded in this process. Integrated menu state3 and movement installation
state1 were read back. No in-map callback or user gameplay confirmation has yet
been observed for this new build. Discover the current PID before inspection.

Test scope is original bunnyhop, infinite duck stamina, autostrafer and fast stop.
Join a local map/team, enable the desired controls, close the menu with Insert,
then test. Existing controls belong to the original UI; no automatic toggles or
synthetic commands were issued. Other gameplay features remain unresolved.

Production `original_movement.cpp` creates executable copies of exact original
code blocks F8674..F870E (bunnyhop), F85F3..F8623 (duck), F92D0..F976D
(autostrafer), F8713..F888E (fast stop) and369540..369561 (ground lookup).
It repairs verified old Source operand addresses to owned current offset cells,
and relocates external rel32 calls. Source bytes, dependent math/helpers and
native keybind getter/table are SHA256 guarded. Internal branch displacements
and original decisions/constants are preserved. The generator proves the
unrelated weapon island F86D1..F86EE is unreachable from the bunnyhop entry.
Its bytes are retained, but its outgoing jumps are not executable from this entry.

Original local-player helper A06C0 calls current EngineClient slot12 and
ClientEntityList slot3; original ground helper uses slot4. The adapter binds only
the verified entity-list global4DE170, checks the UI's current engine4DE174,
scopes/restores command4DE214 and cached-local4DE23C, and seeds original ring
angles exactly as the core does. Field offsets are resolved from CCSPlayer
RecvTables (water25E,flags104,velocity114,ground150 in this installed build).
Original keybind getter33AE00, input states, math, normalization, ring3748D0 and
movement correctionF7A50 run live. No test dependency substitutes are compiled
into the game loader. No new movement feature formula is used by this backend.

The callback adapter validates in-game/alive state and excludes unsupported
movement types8/9/10. Both special-mode bytes7DBEFE and519560 being nonzero
refuses the command before any feature runs; this avoids F7A50's unresolved old
Source special-mode operand. Exceptions restore the command and stop this
backend. The full original core callback, prediction and unrelated actions remain
disabled. This is a bounded movement recovery, not full bootstrap restoration.

The original fast-stop block produces approximately unit braking for velocity250
in the isolated fixture. This magnitude was retained, not replaced with a new
speed-scaled implementation. Assess its actual gameplay effect during testing.

All27 CTests pass (21x86,6Steam), including17 production-capsule scenarios with
the real original public lookup/keybind/math helpers and explicit isolated Source
interface fixtures. Cases cover command-ring wrap, bunnyhop water/ground/flag
gates, duck suppression/bind override, strafe/bind gates, original correction,
fast-stop ground/input/speed gates, profile rejection and scoped state restoration.
The separate12-case original autostrafer probe also passes. These tests establish
bounded native behavior, not user-confirmed gameplay.

Old game23896 did not exit after WM_CLOSE; its exact executable was rechecked,
then it was terminated for this required fresh-process test. Steam and IDA were
preserved; debugger reported stopped. Existing Steam staging hooks prepared the
new process. Live original helpers matched their raw source before restarting.

New completed reversals (autostraferF92D0, ground lookup369540 and keybind getter
33AE00) were named/prototyped/commented in live IDA, read back and indexed via
`tools/retain_original_movement_findings.py`. Existing meaningful comments were
preserved. Latest IDA database disk save is unverified.

Use `py -3 tools/capture_movement_state.py --pid 5336` for read-only readiness,
checkbox and command/change counters. Build identity and loaded paths are in
`build/original-gameplay/validation.json` and local runtime5336 captures.
