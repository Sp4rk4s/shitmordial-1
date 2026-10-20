# Original movement code execution, 2026-10-20

The user requires original Primordial feature logic. The unpublished approximate
C++ autostrafer/fast-stop code was removed. The existing user-tested bunnyhop/duck
bridge is unchanged; it is not represented as original native feature execution.

`tests/original_motion_probe.cpp` now verifies the exact dump SHA256, maps its bytes
inside an isolated x86 test process, repairs the existing 339 recovered imports,
and executes original autostrafer RVA F92D0. It calls the original command-record
helper3748D0 and original normalization/math code. It then executes original
movement correction F7A50, as the native core does at37A049 with command angles,
saved movement angles and NULL optional-command argument. No DLL entry,
constructors, hooks or protected startup roots are invoked by this test.

Explicit test fixtures replace only the local-player lookup A06C0 and inactive
slow-walk binding getter33AE00. Three guarded stale Source operands in F92D0
are directed at fixture cells for water25E, flags104 and velocity114. The
original special-handling predicate byte7DBEFE is set false. These are dependency
models, not recovered production initialization and not a state to inject.

Twelve native scenarios pass: baseline, disabled control, water, ground,
IN_SPEED, low speed, opposite alternation, forward input, side input, lateral
velocity, oblique velocity and nonzero vertical velocity. Seven active cases
also execute the original correction and verify finite output, horizontal
movement magnitude450 and unchanged view angles. Baseline native strafe writes
side450 and desired yaw3.38199997; original correction produces forward26.5467434,
side449.216278. Opposite alternation reverses side while preserving forward.
These assertions check original-code contracts; no replacement feature formula
is used to generate the feature output. Full optional special-handling paths and
real UI binding dependency are not covered.

The isolated executable is registered as `original-autostrafe-contract` in CTest.
`tools/build.ps1 -OutputDirectory build/original-motion` passed26 tests:
20x86 and6Steam. Native probe output and binary hashes are recorded locally in
`build/original-motion/validation.json`.

Verified functions were named, prototyped, commented and read back in live IDA:
`command_state_ring_record`3748D0, `read_feature_state_7dbefe`10ACC0,
`correct_command_movement`F7A50. The project replay index is updated. IDA database
disk save remains unverified. Ring lookup uses signed remainder150 and stride44;
negative command numbers are not normalized and must not be supplied live.

No new gameplay DLL was injected. Current game still uses the previously tested
bridge. Original-code gameplay is not ready for user testing: current interface
binding, field operand repairs, UI dependency behavior, original movement gates
and integration with the existing callback still need verification. Do not enable
the whole3798C0 callback: old CInput layout, prediction and many unrelated feature
dependencies remain incompatible/unresolved.
