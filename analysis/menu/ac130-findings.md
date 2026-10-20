# ac-130 build tier — 2026-10-20

Binary SHA-256 e16dd0ec…9768. IDA analysis base 0x44000000; absolute operands
are runtime VAs (runtime = 0x44340000 + RVA).

## Flag

- Loader input block RVA 4DDCE0..4DDEE5 = username[512], id (DWORD), mode byte.
- DllMain attach helper A1E72 copies the mode byte [4481DEE4] (RVA 4DDEE4) to the
  ac-130 flag 4481E245 (RVA 4DE245). That is the only writer.
- Live test PID30260: writing 01 to 4481E245 via x32dbg made the title-bar
  "ac-130" tag appear (user screenshot). Extra controls did not appear because
  the menu had already been constructed.
- Loader fix: startup.cpp sets the mode byte to 1 after restoring loader input,
  so the attach helper sets the flag before the menu constructor runs.

## Menu options built only when the flag is set at construction

Labels decoded from their generator functions (word 0 low byte = XOR key,
following words = characters ^ key; decode helper 2DF560).

Menu constructor block 2E2DE6 (`jz` skips 1859h bytes when 0) — group
"debug (ac130)": show aimpoints, show safepoint hitboxes, disable resolver
samples, disable resolver, show aimbot state, console logs, hitchance spread
crosshair, hc spreadcrosshair color, debug safepoints, debug records, OOGA,
always on tickshift, dont flip slide legs, disable on_demand_bone_setup,
no shift extrapoltion, cl_interp override.

Block 2FFB6F (`jz` skips 190h bytes) — option "slippy walk".

## Other readers (runtime gates, not menu construction)

B1783, C0C7B, C16B9, CD86F, F2683, 1069B9, 137A8D, 139B64, 139C2D — mostly paired
with a separate config bool at 44820090+offset; gameplay/visual paths whose
bootstrap is still unresolved, so their effects are not expected yet.
351E4D draws the title-bar tag.

Not yet game-tested with the loader change. REA could not analyze the raw dump
(no Ghidra install; REA's IDA route rejects headerless binaries); the user's IDA
database with identical SHA-256 plus Capstone decoding of undefined constructor
bytes were used instead.
