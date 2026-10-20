# Integrated native menu input — 2026-10-20

The user confirmed **“All four work”** for multi-select dropdowns, keybinds,
sliders and search in real legacy CS:GO PID23896. This uses the normal
`primordial_loader.dll`, with no separate preview DLL. Dragging, tabs and basic
checkbox input were confirmed in the preceding preview; the subsequent normal
loader initially had incomplete widget input, addressed here.

## Cause and change

The preceding integration supplied only the cursor and left mouse button. It
manually selected tabs and walked a whitelist of checkbox/slider/subtab controls,
skipping observers and attached children. It omitted active popup routing,
keyboard capture, text editing, wheel state and held-key duration. Geometry
refresh also forced group+68 to zero to skip scrolling. Those restrictions
explain why visible widgets were inconsistent or unresponsive.

`primordial_loader/src/menu_ui.cpp` now calls the original window updater
RVA3504B0 on each paint. It preserves native layout, input, observer/attachment
processing and scroll state. A UI-only coordinator follows the verified portions
of original frame32BA40: transient flags, focused-window ordering, the menu's
toggle keybind, Ctrl+F/Escape search routing, active control input/drawing and
deferred UI callback finalization. Original351AB0 draws the windows. Native shown
and opacity fields are no longer forced on each frame.

The input adapter supplies all114 Source button states (up/pressed/held/released),
held durations and wheel delta. The installed InputSystem slot46 getter at
RVA2C00 converts keyboard codes0..106 to Win32 virtual keys; its actual table and
11→A /72→Insert associations were checked. Slots40 and46 have stable disk-SHA
and relocation-aware code guards. Mouse codes107..111 map to the five Win32
buttons,112/113 to wheel direction. Foreground loss releases held inputs.
Window-message capture prevents the game's controls from also receiving menu
input; native text editing consumes button states instead of WM_CHAR.

The normal DLL installs the integration after payload entry succeeds. Source
guards now come from the exact installed PE files and support ASLR rather than
capturing a particular process's method bytes. `BUILD_MENU_PREVIEW` is optional
and uses the same production implementation; normal use requires only the
standard loader and its adjacent TLS DLL. `primordial_menu_state` distinguishes
first real menu draw (3) from mapping handoff entry completion (also3, separately).
`tools/test_menu.py` waits for actual UI readiness where the export exists.

## Supporting evidence

- Read-only capture `tools/capture_ui_dependencies.py` retained the UI thread,
  verified x86 TEB, named exports, Source key table and current GUI focus handles
  for the prior process39732. It made no target writes or target calls.
- Offline copied-state native window replays completed with a balanced caller
  stack for misc, aimbot and search, without loader_data field reads:
  `native-ui-update-probe-39732-tab3-search0.json`,
  `native-ui-update-probe-39732-tab0-search0.json`, and
  `native-ui-update-probe-39732-tab3-search1.json`.
  These used explicit synthetic allocation/text-metric models and copied Win32
  handle observations; they are not live execution or pixel proof.
- The protected calls in group34A776 resolve GetFocus and GetActiveWindow. They
  compare window focus and clear scroll velocity on mismatch. They do not read
  the stale CPU guard on the observed paths. The original scroll branch can
  therefore remain active instead of zeroing group+68.
- Native text edit34EB10 uses the Source keyboard state/duration arrays and
  InputSystem40 key names. Its raw excerpt is retained in
  `text-edit-input-native-excerpt.txt`; the containing routine remains partial.
- Fifteen small helpers have verified names, prototypes and purpose comments
  applied/read back/indexed: `native-ui-coordinator-ida-applied.json`. Six
  additional partial input/coordinator notes are applied/indexed in
  `native-ui-input-notes-applied.json`. Latest IDA disk save is unverified.

## Actual run and artifacts

Fresh PID23896 launched with `-windowed -insecure -steam` and loaded
`build/menu-native-input/primordial-x86-native/primordial_loader.dll` normally.
The actual constructor returned, restored the captured guard pointer, constructed
four original windows, and first real draw returned. UI state and mapping state
were both3; no preview DLL was loaded. The user then tested the four formerly
broken widget categories and confirmed they all work.

The exact tested loader and TLS companion were copied to
`build/primordial-x86-native/` and verified byte-identical to the live artifacts.
Loader SHA256:
`08d126eba18b9fe0298c1b779155db9fda36d96ced7c38252842b8a5a42ee1b8`.
All22 isolated checks passed in menu-native-input:16 x86 and6 Steam. New checks
exercise button edges/repeat duration/focus-loss release/wheel pulses and real
Windows relocation of guarded method prefixes. They do not execute the payload.
Recorded validation is in `build/menu-native-input/validation.json` and
`build/validation.json`, with source/artifact hashes and actual runtime evidence.

Runtime evidence:
`runtime-23896.json`, `menu-preview-runtime-23896.json`,
`native-input-23896.png`, `native-ui-input-runtime-log-23896.txt`.
The legacy name of the state capture tool does not imply a preview DLL was used.

## Scope still unresolved

Original gameplay bootstrap remains unresolved. The UI coordinator excludes
original frame cosmetic syncE3EF0, periodic gameplay roots and uninitialized
config selection; it does not call full feature-hotkey dispatcher3582C0.
Working menu keybind capture does not mean its gameplay feature runs. Config
filesystem action callbacks and every possible widget combination have not been
separately validated. Preserve the successful game and distinguish those tasks
from the now user-confirmed menu widget input.
