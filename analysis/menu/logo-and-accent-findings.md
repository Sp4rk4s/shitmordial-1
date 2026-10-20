# Menu logo and accent colour — 2026-10-20

Binary SHA-256 e16dd0ec…9768. IDA analysis base 0x44000000; absolute operands
are runtime VAs (runtime = 0x44340000 + RVA).

## Accent colour did not propagate

- Accent picker value: RVA 540138 (runtime 44880138); default copy at 54013C.
  Default `FFA49AC1` = #c19aa4, matching the user-reported default.
- Global style accent read by ~60 control draw sites: RVA 4B33A8 (runtime 447F33A8).
- Only writers: original panel callback at 1550AA (`mov eax,[44880138]` /
  `mov [447F33A8],eax`, immediately before `call menu_render_frame` 32BA40) and a
  sibling at 32B80E. The loader calls 32BA40 itself, so the copy never ran.
- Fix: `run_ui_frame` stores 540138 -> 4B33A8 each frame, like the original.
- Live read of PID10284 (read-only) confirmed both fields hold the default.
- E3EF0 is unrelated: its XOR string decodes to `pos: x {} - y {} - z {}`
  (nade-helper label), so it remains excluded.

## White logo

- Embedded logo: 76x76 RGBA PNG at RVA 4108C8, size 1335h.
- Static initializer 654D0 (`static_init_logo_png_vector`) copies it into a
  byte vector at RVA 57ECD4; the live vector is valid in the current process.
- Consumer 34CF20 caches the surface texture ID at RVA 4B3D50 and only creates
  the texture / decodes the PNG when that ID is -1. Each frame it recolours the
  logo's accent pixels with [540138], then DrawSetTextureRGBA / DrawSetTexture.
- The dump captured ID 0x132 from the original process (also live in 10284), so
  creation was skipped and a never-filled ID was drawn (white); the per-frame
  upload also targeted the game's own texture 306 with an empty buffer.
- Fix: preflight requires the captured 0x132; `initialize_menu` writes -1 before
  the first frame. The other lazy texture slot (RVA 4B3D30, 2DF710) is already -1.

Not yet game-tested. REA could not analyze the headerless raw x86 dump on this
host (no Ghidra; REA's IDA route rejects raw binaries), so the user's live IDA
instance with the identical SHA-256 was used.
