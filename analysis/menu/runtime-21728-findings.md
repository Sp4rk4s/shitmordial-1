# Windowed debugger retest: menu absent (2026-10-20)

The user confirmed **No menu** after pressing Insert in PID21728. This is the
latest actual visibility result. The resolver-fix patcher and adjacent TLS
companion loaded successfully, but game/menu initialization remains unresolved.
PID21728 is an observation, not a permanent process identifier.

## Observed execution

- The controller launched the installed legacy executable with
  `-windowed -insecure -steam`, after ensuring the Steam stager was loaded.
- The configured three-argument CRT entry returned TRUE. Handoff state is3,
  static TLS index51, all12 resolver hashes/current HMODULEs are valid. Export
  cache counts, interface count and worker count remain0.
- Actual ucrt `_initterm` at7582AC10 received constructor bounds
  4474E5A8..447501F4. All1810 nonnull constructor targets ran and matched the
  saved table exactly; it returned at44709D9E. This rules out an omitted native
  constructor in that range, not missing effects inside its callees.
- Hardware execution breakpoints at interface lookup44698E60 and worker
  setup446A2970 received no hits through entry return. Later menu root446204D0
  and first-window accessor4461F6D0 also had no hits during observation.
- A paused sample of80 threads and654 bounded EBP frames contained no current
  instruction or recorded return in the payload. Frame-pointer omission and
  incomplete walks limit this negative observation.
- D3D9 device discovery found the same device through two shaderapidx9 matches.
  Reset/Present/EndScene targets did not point directly into the payload.
  This is not a complete hook audit. Cross-process WndProc retrieval returned0
  without a captured error; WndProc was unavailable, not verified unhooked.

Evidence: `runtime-21728.json`, `debug-21728-constructor-execution.json`,
`debug-21728-entry-return.json`, `debug-21728-threads.json`,
`debug-21728-framewalk.json`, `debug-21728-render-observation.json`.
Constructor execution was appended to the existing live IDA CRT attach comment
and read back; see `constructor-runtime-ida-applied.json` and the shared index.
Database disk save remains unverified.

## Debugger-induced exits corrected

Earlier windowed test processes8068 and10888 exited without user action. Steam's
game-process log recorded1080890248 (`0x406D1388`), the legacy thread-naming
notification. The debugger was passing that notification to the target.

A one-shot, x86 bridge-settings adapter was loaded into x32dbg, never into the
game. It appended only `406D1388-406D1388:nobreak:log:debugger` to the existing
exception policy, preserving other filters. The ini was backed up under
`analysis/debugger-setup/`; its updated policy was read back. PID21728 remained
stable after this correction. This repairs the test setup, not the menu.

## Additional protected-runtime dependency

Four protected config/menu/worker/localization entry prefixes were traced
**offline**, using a read-only copy of the prepared image and actual TEB/PEB
pages. They load image DWORD RVA91BC4B (runtime44C5BC4B), which still contains
captured pointer1C85CEC8, then dereference pointer+B8. That external page is
unreadable in the current game. None of the339 VMP import destinations repairs
this global.

VMProtect source and controlled emulator dataflow identify this as its CPU-hash
entry guard and strongly support the global's identity as `loader_data`.
Synthetic emulator-only state can exercise match/mismatch branches. It is not
a complete GlobalData reconstruction or evidence of a successful live call.
See `protected-entry-runtime-state-findings.md` and the subsequent, more precise
`protected-guard-contract-findings.md`. Controlled predicate cases recover count
at+B8 XOR59DB1541 and hash array at+20 XOR40DF21BC, including count bounds,
CPUID arithmetic and the Athlon adjustment. Those emulator CPU values must not
be copied into the live process.

A one-byte hardware read/write watch at44C5BC4B replaced the first-window
accessor breakpoint; see `debug-21728-loader-data-watch.json`. Interface, worker
and menu-root execution breakpoints remain. No speculative payload call,
allocation at an old address, or production behavior patch was made.

## Remaining work

Recover the actual protected/indirect game bootstrap caller and its invocation
contract. The DLL lifecycle path and constructor range do not establish one.
The recovered CPU guard is an additional prerequisite, not proof that repairing
it alone will make the menu appear. No reinjection of the same build is justified.
Use the current process only for read-only observations and debugger tracing.

Further bounded findings: `protected-entry-stub-inventory.json` inventories
native-to-VMP E9 leads; `protected-coordinator-prefix-findings.md` records five
additional offline probes. They expose callback/netvar/allocation/protection
leads, not a verified overall initializer. `bootstrap-lifecycle-followup-findings.md`
and `bootstrap-residual-contract-findings.md` record bounded table/caller searches.
`self-erasure-footprint-findings.md` finds no positive footprint of the recovered
self-erasure helper; erased bootstrap is currently unsupported. None establishes
a callable replacement startup sequence.
