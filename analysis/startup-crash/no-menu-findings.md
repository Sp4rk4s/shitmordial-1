# No menu after successful configured entry

Live csgo PID9928, 2026-10-19. Loaded patcher path is the canonical
build/primordial-x86-native/primordial_loader.dll. Protocol2 handoff at0x45751000
has state3 (initialized), so mapper collected a nonzero configured entry return.
The alphabet object now points to newly allocated buffer0x3403FEB0 with size/cap63;
the previous captured-pointer fault is resolved for this run. This does not prove
that every captured object or actual menu initialization is correct.

Thread start-address queries succeeded for all enumerated threads. None starts
inside the mapped payload range. This does not rule out an external CRT wrapper,
an exited worker, or callbacks into payload code. Attempts to obtain combined
thread suspension/context access were denied (OpenThread error5); no threads
were actually suspended or resumed and no live memory was modified. Thus no
reliable running instruction/stack sample was obtained.

The existing device pattern found the same global in two locations in shaderapidx9:
global0x683062C0 -> device0x190FB900 -> vtable0x190FE85C. Slots16/17/42 point to
d3d9.dll Reset/Present/EndScene, not directly to the payload. Reset/Present have
external E9 detours; this check alone cannot exclude all rendering interception.
EndScene starts with ordinary function bytes. Evidence is in no-menu-device-9928.json
and no-menu-d3d-prologues-9928.json, thread starts in no-menu-thread-starts-9928.json.

The DLL attach helper at analysis0x440A1E50 is not defined as a function in IDA.
A bounded offline disassembly was made from a synthetic PE slice of the original
bytes; startup-slice.exe is an analysis carrier, never executed. Its observed
body processes captured user/config data and initializes MT19937-style RNG state,
then returns. It has no direct worker creation or rendering-hook installation.
That observation does not cover the entire constructor table or protected startup.
The real menu/hook initialization path, captured once-only state, and any loader
initialization beyond this CRT wrapper remain unresolved. Do not guess a new entry
address or call arbitrary candidate functions to force a menu.

mt19937_seed_state at analysis0x4409ECC0 was renamed/prototyped/commented in live
IDA; helper partial findings were retained as an address comment and indexed.
Disk database save remains unverified. No payload behavior was patched during
this investigation. Initialization state3 must not be described as proof that
the menu is functioning.
