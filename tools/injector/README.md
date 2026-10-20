# Local game test harness

Build the standalone x86/x64 LoadLibraryA helpers and run their isolated tests:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-injector.ps1
```

Detect Steam/CS:GO and inspect the staged handoff without loading anything:

```powershell
py -3 tools/test_menu.py status
```

Launch the installed legacy CS:GO if absent, load the Steam stager first, wait
for staging and fourteen required modules, attach the idle x32dbg MCP bridge,
and inject the current normal loader:

```powershell
py -3 tools/test_menu.py run --launch --release build --debug
```

Automated launches always pass **`-windowed -insecure -steam`**. The default
executable is the verified `csgo legacy/csgo.exe` installation, not Steam's app
730 launch action, which currently selects CS2. Override with `--game <path>`.
Steam must already be running and signed in. `--release <directory>` chooses
an alternate prepared build; the default is `build`.

The controller never automatically closes or restarts existing games, Steam,
IDA or debugger sessions. It refuses multiple ambiguous game processes (select
one with `--game-pid`), a different pinned Steam stager, an occupied debugger,
or a handoff that has already attempted initialization. Restart those sessions
deliberately when needed. Existing games must already have a fresh Steam mapping.

Debug mode uses a hardware execution breakpoint at the verified payload entry
0x4470A002, preserving staged bytes and checksum validation. Auto-created
software breakpoints named `TLS Callback ...` are disabled only in the fresh
debugger session created by the controller; all other breakpoints are preserved.
Any resulting pause is recorded for inspection rather than blindly continued.
Evidence is written to `analysis/menu/debug-start-<pid>.json`.

The active debugger was configured to consume the legacy thread-naming
notification `0x406D1388` using the specific exception filter
`406D1388-406D1388:nobreak:log:debugger`. Passing this notification to the game
caused two test processes to exit. The previous ini is backed up under
`analysis/debugger-setup/`. The x86 `debugger_exception_policy.dll` target is a
one-shot adapter for the debugger's own bridge settings; it belongs in the
debugger process only. This setting does not suppress other exceptions.

Omit `--debug` to load and wait for the handoff result, then collect a read-only
runtime snapshot. Entry TRUE is not proof of a working menu; the user still
confirms visibility. The normal loader initializes the integrated menu after entry; the controller
checks UI readiness separately from the handoff.

The native helpers accept explicit PIDs and DLL paths:

```powershell
build/injector/x86/loadlibrary_injector.exe <csgo-pid> <absolute-patcher-dll>
build/injector/x64/loadlibrary_injector.exe <steam-pid> <absolute-stager-dll>
```

They validate target/DLL architecture, locate the actual LoadLibraryA owner
(including KernelBase forwarding), translate its RVA to the target module, and
verify the loaded DLL by full path. They avoid repeated loads of the same DLL.
Paths must be representable in the current Windows ANSI code page.

Exit codes: 0 means loaded/already loaded; 1 means failure; 2 means usage error;
3 means the remote thread did not finish within the timeout. A pending thread
may be paused by the debugger: its argument allocation is deliberately retained,
and neither the thread nor target is terminated. Inspect it before retrying.
The small retained path allocation is reclaimed at target exit.

Tests inject a harmless DLL into disposable same-architecture hosts, verify
module visibility from the host, reject missing/non-DLL paths and avoid duplicate
loads. Separate validation rejected x86-to-x64 and x64-to-x86 injection attempts.
The helpers do not map primordial.bin or modify its loading protocol.

Latest user-confirmed menu result: PID23896 with the integrated normal loader;
multi-selects, keybinds, sliders and search work. Gameplay feature startup remains
unresolved. Current source adds the fourteen-module resolver repair, which passes
isolated checks but has not been game-loaded. Follow the root
[README](../../README.md) for the current compile/inject workflow.
