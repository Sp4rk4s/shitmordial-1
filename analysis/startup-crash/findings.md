# CRT startup crash, 2026-10-19

Input SHA256: e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
IDA analysis base remains 0x44000000. Original runtime base is 0x44340000.

## Observed in the debugger-paused process

Read-only OpenProcess/ReadProcessMemory and Wow64GetThreadContext inspection of
csgo PID 21136, thread 36828 found EIP 0x443CA427, ECX=7, EBP=0x32DBF8A8,
ESP=0x32DBF580. At EBP+4 the return address was 0x443C9E1B, followed by argument
7. The caller at 0x443C9E14 pushes 7 and calls the CRT fail-fast routine after
its test of absolute startup state 0x44B66238 fails. This is CRT startup state
failure, not evidence of a security-cookie mismatch. No thread resumed, process
terminated, live memory modified, or debugger detached during this investigation.

## Original base evidence

* RVA 0x3CA350 pushes absolute SEH handler 0x4470A395. The matching function is
  at RVA 0x3CA395. Subtraction gives 0x44340000.
* CRT absolute GetProcAddress slot 0x4474E00C corresponds to recovered import RVA
  0x40E00C, independently yielding the same base.
* Constructor array at RVA 0x40E5A8 has first nonzero entry 0x443ACCD8. With the
  corrected base it targets RVA 0x6CCD8, a push/call/pop/ret initializer. Under
  the previous base it targeted the middle of an unrelated function.
* CRT state at absolute 0x44B66238 corresponds to RVA 0x826238 (captured value 2),
  not the protected instruction bytes at RVA 0xB66238 in the old mapping.
* Cookie/complement absolute addresses 0x4481B628/0x4481B624 correspond to RVA
  0x4DB628/0x4DB624 and hold a consistent captured pair B05DA3EB/4FA25C14.

The old map ran relative calls at the analysis base but its absolute references
addressed unrelated/empty bytes. Changing only the CRT state at the old VA would
corrupt protected code. The new mapping base fixes those references without
rewriting payload operands or changing the original dump.

## Startup preparation

The memory dump already contains initialized CRT state. Before calling its CRT
entry, startup.cpp validates the exact captured 52-byte startup profile and the
cookie pair, then clears these verified fields only:

* RVA 0x826238: native startup state (2 -> 0).
* RVA 0x82623C: startup lock (captured zero; remains zero).
* RVA 0x826241: onexit initialized flag (1 -> 0).
* RVA 0x826244 and 0x826250: two 12-byte onexit tables containing old heap state.
* RVA 0x826268: DLL process attach count (1 -> 0).

The original cookie pair is retained because dumped pointers may be encoded with
it. No blanket BSS reset or fast-fail suppression is used. Guards reject a changed
profile before this helper writes anything. Mapping checksum still runs first.
Imports are then bound before entry as before. Handoff protocol version is now 2.
The earlier guessed section boundaries were also wrong: RVA 0x400000/0x40B000
contain instructions. A single synthetic RWX .dump section describes the captured
image until original section boundaries can be recovered; it is not an assertion
that the original PE used these permissions. Header stays read/write.

## Validation and limitations

Both DLLs rebuilt under build/startup-fix because the original loader DLL remains
locked by the paused game/debugger. 18 tests passed (12 x86, 6 Steam launch).
The new data-only dump test verifies original-base relationships, exact state
changes, preservation of every unrelated byte including cookies, rejection of
changed state/cookies, bounds, and repeated preparation. Existing tests cover
339 real export resolutions, MinHook trampoline behavior, and synthetic launches.
These tests do not execute the real payload. Further captured heap, TLS, runtime
or game state may need repair after this startup failure is resolved.

Six newly reversed functions were named/prototyped/commented in live IDA and
read back (ida-applied.json); the replay index is updated. IDA on-disk save remains
unverified because the legacy bridge has no save method. Existing analysis VAs
are deliberately preserved, not silently rebased.

Restart Steam and CS:GO for a fresh mapping. Load the new steam-x64-native DLL
before starting CS:GO, then inject the new primordial-x86-native DLL into the game.
The old paused process cannot be repaired by reinjecting the patcher.
