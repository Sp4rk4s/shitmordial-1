# Original renderer screen/font refresh recovered, 2026-10-20

Latest real result remains **NO MENU**. No payload was loaded, no target memory
was written and no target routine was called during this follow-up. All new
execution results below are offline Unicorn interpretation with explicit models.
Original input SHA256 remains
e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
RVA addresses, runtime base44340000, IDA base44000000.

## Identified original path

Six main-menu font-record addresses have actual MOVQ stores inside protected
code, rather than just native drawing references. Earlier immediate-only
previews beginning at opcode05 were misaligned: the full instructions begin
660FD605. Bounded continuation from these stores led to native3606B3, inside
candidate360080. Synthetic middle-of-function continuation is a lead only;
font-store-offline-continuations.json preserves its limits and both flag states.

Following360080 from its real entry with the previously documented synthetic
CPU guard first reaches EngineClient4481E174 virtual5. An explicit screen-query
model writes1920x1080, copied from the actual game client rectangle, into
44B487AC/B0. Subsequent original code requests AddFontMemResourceEx for BOTH
recovered fonts, using their exact pointers/sizes447F3D58/3500 and447F7258/23EA4.

The GDI endpoint was resolved from the current GDI32 PE export table and checked
against the existing keyed operand9DB1AD. Its key is0C3C6BBF;6A841981 is the
manifest's original encoded operand, not its key. No import mechanism was changed.
Asset hashes match embedded-font-scan.json.

With hypothetical successful registration, Source font IDs, glyph setup and
uniform measurement models, this original routine requests15 cached font
configurations through35F040 and reaches the caller sentinel after3,980,473
instructions, ESP10010004. No input-stack arguments or incoming-register buffer
accesses were observed. It reads only the existing modeled guard fields+B8/+20.
This is a completed modeled path, not recovery of the complete VMP runtime.

Evidence: candidate-360080-constructor-native-crt-11240-screen-font-model.json
and candidate-360080-constructed-offline-11240.zip. Never inject that snapshot.

## Exact six main-menu font requests

Full15-request data and associations are in font-refresh-settings.json. IDs,
widths and heights returned by the models are synthetic and must not be reused.

|Runtime record|Face|Tall|Weight|Flags|Character range|
|---|---|---:|---:|---:|---|
|448C0F34|Tahoma|13|400|80h|0..65535|
|448C0F40|Tahoma|13|400|80h|0..0|
|44B3B610|Tahoma|12|700|10h|0..0|
|44B45430|Tahoma|11|400|80h|0..0|
|44B45480|Tahoma|14|400|80h|0..0|
|44B454C8|primordial_menu_icons|26|500|90h|0..0|

All requests use blur0 and scanlines0. Other requests include menu icons13,
CSGO icons30/16/12, Tahoma14 and13 bold, Small Fonts13/10 and Bahnschrift20.
The original glyph constructor still temporarily writes a POINTER to
FontManager+28, incompatible with the current module's inline language string.
The emulator supplies an explicitly synthetic OLD pointer-layout object. This
experiment does not resolve that live compatibility problem.

Valve's public ISurface contract corroborates the glyph argument meanings and
contains a two-argument OnScreenSizeChanged method, but does not establish an
installed CS:GO slot or payload hook identity:
https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/vgui/ISurface.h

## Native callback and remaining caller gap

Native154770..154787 forwards ECX and its two stack arguments through original
pointer44B3B880, calls360080 with no stack arguments, then RET8. It is now named
forward_two_args_then_refresh_renderer. OnScreenSizeChanged is a supported
semantic candidate, not a verified installed hook or recovered installer.

360080 is named renderer_refresh_screen_fonts with a no-argument cdecl prototype.
Its original5-byte IDA entry boundary is preserved; no contiguous protected
function boundary was invented. Both names/prototypes/comments were applied to
live IDA and read back, indexed, and included in replay. Disk save of these newest
annotations remains unverified. Receipt: font-refresh-ida-applied.json.
Six new font data notes remain indexed/pending replay, not applied live.

Literal scans found44B3B880 only in the forwarding call and44B3B884 only in the
paint callback; no plaintext runtime callback-pointer references were recovered.
A further CC-padded E8-entry inventory is retained separately in
protected-call-entry-candidates.json. Most lie in exit-destructor ranges; this is
not a new bootstrap or verified CFG. The original caller that installs callbacks
and invokes the subsystem roots remains unresolved. Refresh360080 does not
construct the four menu windows or install a paint callback in the modeled path.

## Actual game/debugger state

Read-only discovery still found game11240, debugger30472, IDA8872. Actual Source
interface pointers, both original callback pointers, dimensions, window begin
and all six font records remain ZERO. Handoff3 still means only entry returned.
See font-refresh-live-state-11240.json.

x32dbg remains PAUSED at77A034A0, NtTerminateProcess, for requested self-exit100.
exit-stop-11240.json retains registers/stack. The crash log records80000004 at
the temporary Source paint breakpoint before the later engine exit. Debugger
continuation contaminated that observation; the precise exit cause is not fully
established. No new payload build was loaded. Do not resume a pending termination
or rewrite its arguments as routine cleanup, or blanket-mask single-step errors.

The existing restriction against live subsystem calls/reconstructed guard state
remains in force. The pending menu-only scope choice was not answered, and no
approval is inferred. Original bootstrap investigation remains the authorized
route. Python compilation checks pass; no production C++ source changed, so no
new production rebuild or CTest run was required.
