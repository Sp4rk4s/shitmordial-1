# Fresh menu retest: PID11240, 2026-10-20

User authorized closing PID21728 and reinjecting the latest patch. PID21728 was
terminated, then the verified legacy executable launched with `-windowed
-insecure -steam`. The pinned resolver-fix Steam stager was retained; its unchanged
dump staging remains separate from the newer in-game patcher.

PID11240 loaded `build/render-state-fix/primordial-x86-native/primordial_loader.dll`
and adjacent primordial_tls.dll. Entry returned TRUE, handoff3. The user explicitly
reported **no menu**. Snapshot: runtime-11240.json. The three guarded ownership
repairs are present in actual game memory (render-preconditions-11240.json).

All inspected renderer interface globals, the original paint callback slot,
window vector and screen dimensions remain zero. The installed Source interfaces
exist independently (source-interface-state-11240.json). The debugger is attached
and running; retained hardware observations cover payload paint44494790, menu
constructor446204D0, frame renderer4466BA40 and loader-context slot44C5BC4B.
No candidate function was called, no payload code was forced to run and no
GlobalData replacement was written into the game.

## Completed offline constructor experiment

continue_menu_constructor.py now accepts --pid and verifies the captured thread
TEB self pointer. The PID11240 input is a fresh read-only image capture, not a
reuse of PID21728 pages. All target-page writes and modeled calls occur in
Unicorn only. Models include synthetic allocations, a derived synthetic CPU
guard context, copied CRT code, verified ASCII conversion cases and uniform
synthetic text metrics. Shared system cursor resources are queried in the analysis
process, never loaded or selected by calling target code.

The trace reached native RET RVA32AA0D and sentinel12345678 with ESP10010004,
after19,012,637 instructions. No original stack arguments or incoming-register
buffer accesses were recorded. This supports a no-explicit-arguments contract
for this construction path; it does not establish the missing bootstrap caller
or complete behavior under other settings. The constructor creates four window
objects, first at runtime447F33B0 with Source Insert key72. All three inspected
default-font records remain zero. Thus construction itself does not supply the
missing font initialization.

Detailed evidence: menu-constructor-native-crt-11240.json; reusable, strictly
offline memory state: menu-constructed-offline-11240.zip. These contain synthetic
addresses and must never be injected or treated as a target memory image.

Three additional captured wstrings at RVA4B349C/4B3584/4B366C reach native storage
release A11A0. Exact pointer/size/capacity profiles and replacements are recorded
under experimental_captured_object_resets. startup.cpp now guards and empties
these alongside the earlier title/selection fields. New build:
`build/menu-ownership-fix/`; all13 x86 and6 Steam tests pass. It has NOT been
loaded into PID11240 and is not a verified menu fix. Another identical startup
flow still does not supply the missing interfaces, fonts or paint installation.

## IDA warning from the user's log

The user ran ida_retain_render.py and reported its successful save. Their log
also reports class-based read-only treatment of seg000, causing Hex-Rays to
constant-fold data references. This weakens pseudocode claims about mutable
captured state; original instruction/runtime evidence remains separate.

Read-only VirtualQueryEx independently verified the current image body is one
committed private RWX region44341000..45750000. Evidence:
runtime-image-protections-11240.json. tools/ida_fix_dump_analysis.py is prepared
to guard input identity/segment bounds, use neutral class PRMDUMP with write
permission while preserving type/boundaries/names, flush generated pseudocode,
replay indexed annotations and save through IDA. It changes no binary/game bytes.
Application is pending until a successful receipt is written. The legacy RPC
bridge exposes no segment-metadata/script-execution method; automated GUI script
submission did not succeed, so no application is claimed.
