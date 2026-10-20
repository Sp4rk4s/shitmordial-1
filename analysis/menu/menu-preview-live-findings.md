# Original menu renders in the live game — 2026-10-20

The user explicitly authorized the menu-only initialization test, then confirmed
"Yes, the contents are visible" after the corrected preview was injected into
fresh legacy CS:GO PID25980. The game was launched with
`-windowed -insecure -steam`. Screenshot `menu-preview-25980.png` independently
shows original tabs/icons, labels, checkboxes, sliders and configuration panels.
This is real Source surface output, not the previous offline draw-intent model.

The preview is intentionally read-only. Insert toggles visibility; native option
input and gameplay startup are not enabled. The top-left logo is still a white
texture. No claim is made that gameplay features, original bootstrap, all tabs,
scrolling, configuration callbacks or normal shutdown have been validated.

## Exact loaded artifacts

- Production patcher and adjacent Windows-managed TLS companion are from
  `build/exception-log-fix/primordial-x86-native/`. This also corrects the misleading
  thread-naming exception reports; previous406D1388 output was not the menu crash.
- The sole preview DLL in25980 is
  `build/menu-preview-2/primordial-x86-native/menu_preview.dll`.
- All14 x86 and6 Steam tests passed in that build directory. These tests cover
  existing production behavior; the additional preview validation is the actual
  live font/constructor/layout/draw run and screenshot/user confirmation.
- `menu-preview-runtime-25980.json` and the build's `validation.json` retain paths,
  observed state, artifact identities and test limitations.

## Controlled initialization

`menu_preview.cpp` is a separate DLL, never automatically loaded by the production
loader or Steam stager. Exact code profiles and handoff/current PID/checksum are
checked before any menu mutation. The profile generator verifies the original
raw dump SHA256 and captures current Source method bytes read-only. It does not
inject emulator state or alter the original binary.

Source factories provide VGUI_Surface031 from vguimatsurface, VGUI_Panel009,
VEngineClient014, VClient018 and InputSystemVersion001. The native input button
name method40 is verified with Source button72 returning INS. Surface/engine/
input/timing globals are bound to current objects. Both intact embedded icon
fonts are registered through AddFontMemResourceEx;15 original font requests use
public Surface71/72/79 and real glyph measurements. The incompatible private
FontManager+28 language-pointer trick is not called.

A pinned MinHook detour forwards PaintTraverse41 and selects the actually observed
MatSystemTopPanel by its current name/handle. Initialization and drawing execute
on the paint thread7140. Native constructor2E04D0 is called once with a fresh
OS-selected guard page containing only the recovered+B8/count and+20/CPU hash.
The hash uses actual CPUID1 and the current x86 PEB build number, not emulator
CPU constants. The captured pointer slot91BC4B is restored to1C85CEC8 in a finally
block immediately after construction. No allocation at that stale address occurs.
Constructor returned and created all four original windows.

The native drawing path uses PushMakeCurrent/PopMakeCurrent, real dimensions,
clip reset3609A0 and primary window351AB0 at ECX447F33B0. Surface opacity is saved
and restored. The frame manager, periodic filesystem work and gameplay hook roots
are excluded. Actual draw returns are logged; first preview fault disables further
preview calls, though faults in original dump handlers can still terminate the
game (as observed in the earlier failed probe).

## Why the first frame was empty

The constructor does not initialize every geometry field; the ordinary window
updater normally fills them before drawing. The zero-filled offline allocator
concealed this dependency. In38484 the live body rectangle was
`68EF2782,35419685,0000000C,196648B0`, placing tabs/content outside useful bounds.
Native excerpt350DBC..350DFD establishes x,y+title,width,
height-title-(tabvector empty?8:55). The preview reproduces only those verified
stores and settled tab animation values. Pure helper34DB30 assigns the resulting
rectangle to the selected tab. This recovered the tabs and sidebar in a real
screenshot before any option-layout test.

The selected group similarly needs its rectangle: tab x+130,y,width-130,height,
as established by34CE73..34CEAE and pure setter34B860. One initial call to group
update34A4B0 with blocked-input=true lays out its native child controls. This is
only attempted when the group's initial scrolling flag+68 is zero; the preview
does not repeatedly enter the partially recovered scrolling/input path.

The first incremental option test38484 reached key-name rendering through
InputSystem4481E160, which that earlier preview had left zero. It faulted reading
A0 at33B646 and the game terminated. This is retained in
`menu-content-fault-38484.log/dmp`; the earlier title-only confirmation is not
presented as a successful menu. The corrected single-DLL preview binds current
input before construction/layout, successfully returns from first layout with
two columns, and displays the options in25980.

## Reproduce within the same approved scope

Use a fresh correctly staged/initialized legacy game and preserve the existing
Steam/IDA/debugger sessions. The earlier incremental layout/content probe DLLs
are historical experiments and should not be injected again.

1. Launch the verified legacy executable with `-windowed -insecure -steam`.
   Existing Steam staging may be reused because it copies the unchanged dump.
2. Initialize it with the current production patcher/TLS through test_menu.py.
   An initialized or failed mapping cannot be initialized a second time.
3. Run `py -3 tools/capture_source_interfaces.py <current_pid>`, then
   `py -3 tools/prepare_menu_preview.py` to regenerate exact current method guards.
4. Build with `powershell -File tools/build.ps1 -MenuPreview -OutputDirectory
   build/<new-output>`. Keep loaded artifacts unchanged.
5. Load only that build's x86 menu_preview.dll using the architecture-checked
   LoadLibraryA injector. Inspect primordial_menu_preview.log beside it and verify
   actual contents. The preview starts visible; Insert toggles it.

The prototype/name/comment findings for pure geometry setters34DB30/34B860,
control width setter3372A0, clip reset3609A0 and vector override155BD0 were applied
and read back in live IDA/indexed. New disk save remains unverified. Bounded
partial updater/key-name dependency comments were added separately; their full
functions are not claimed recovered.
