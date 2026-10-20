# Loader design

`steam_module.dll` embeds the unmodified `primordial.bin`. Load it into the parent
`steam.exe` before starting the game. It hooks currently loaded modules' imported
`CreateProcessA/W` functions, refreshes hooks for newly loaded modules, and stages
the dump at `0x44340000` while intercepted CS:GO launches are suspended. It preserves
the caller's suspension flag and resumes normal launches after staging. A process
watcher also handles existing games and launches through other paths. Each new
game process gets its own mapping; the watcher tracks process handles through exit.

After the game is ready, inject the **x86** `primordial_loader.dll` into `csgo.exe`.
This DLL validates the mapping handoff and raw checksum, applies the original
authentication patch, preserves the captured cookie pair, resets verified CRT startup
fields, builds a minimal PE header, makes captured code executable, runs diagnostics, and
starts a separate thread that calls `0x4470A002` with the DLL entry arguments.
The patcher waits for that thread's result before publishing initialized/failed
state. It only modifies the existing mapping; it never reads or remaps
`primordial.bin`. Captured BSS is reset while retaining verified loader inputs and
module hashes; fourteen resolver handles are rebound to current game modules. The Steam DLL does not
patch the payload or call its entry. Successful initialization happens once per
process; failed initialization requires restarting the game.

After entry succeeds, the same loader installs the menu paint/input integration,
registers the original embedded fonts and constructs the original windows on the
paint thread. Original native window and control updates handle dragging,
resizing, checkboxes, attached controls, dropdowns, keybind capture, sliders,
scrolling and search. Keyboard/button transitions, held duration and mouse wheel
are supplied without invoking the original gameplay hotkey dispatcher. The UI
coordinator excludes unresolved cosmetic/gameplay startup and periodic calls.

`primordial_menu_state` separately reports UI readiness: 0 dormant, 1 waiting
for paint, 2 constructing, 3 first draw returned, 4 failed. Handoff state 3 only
means the payload entry completed. `tools/test_menu.py run` checks both states;
actual appearance and interaction still require testing in the game. Menu
integration logs to `primordial_menu.log` beside `csgo.exe`.


The patcher also repairs **339 VMProtect import records across 18 DLLs** before
entry. These records were decoded from this dump using the supplied VMProtect
source: name key `0x357B2A2D`, table RVA `0x10AD0CF`, size `0x1074`. Fixups retain
each protected slot's encoding as `resolved_export - key`, including unkeyed
pointers, decorated data exports and ordinal imports. Every destination and
original operand is checked, and all exports resolve before any import writes.
See [import findings](../analysis/imports/findings.md) and the read-only extraction
tool `tools/recover_vmp_imports.py` for reproducible evidence.

The earlier supplied 36-patch list is inactive: its 11 claimed E8 operands did not
match under the stated base. Its proposed entry was not adopted. Import recovery
alone does not validate real VMP startup. Original base evidence and CRT state repair
are recorded in [startup findings](../analysis/startup-crash/findings.md); exact original
section boundaries remain unknown.

Before starting the entry thread, Primordial installs a MinHook detour on the
module pattern scanner at **`0x4455E560`** (RVA `0x21E560`). It forwards the original
three caller-cleaned stack arguments and return value, logging module name,
pattern and result to stdout and `OutputDebugStringA`. It does not replace the
scanner algorithm or suppress missing matches. Scanner bytes are checked before
installation; a mismatch prevents initialization. The patcher DLL is pinned while
the hook can be called, so restart the game to unload it.

The parser at `0x4455DAC0` handles hex tokens and `?`/`??` wildcards. See
[analysis findings](../analysis/pattern_scan/findings.md) for assembly evidence,
calling-convention details and unresolved runtime callers. Real scanner execution
in CS:GO has not been observed.

MinHook is vendored under `third_party/minhook` from an existing local x86
distribution because network download was blocked. The patcher uses its matching
static CRT configuration. See [dependency provenance](../third_party/minhook/PROVENANCE.md)
and distribute the generated `MinHook-LICENSE.txt` alongside the DLL.

The staging allocation includes the original image/padding and one additional
metadata page at `0x45751000`. Neither component overwrites unrecognized allocations
at the preferred base. There is no relocation support. Launches that bypass the
import hooks use the watcher and can still lose the preferred-address race; an
occupied base produces an error instead of overwriting memory. Restart Steam to
remove the launch hooks: the DLL is pinned while its hooks are active.

The requested references are structural references only:
[moneybot](https://github.com/PinkKing2/moneybot) and
[i_was_coding_this_while_drunk](https://github.com/4wi/i_was_coding_this_while_drunk).
Their binary-specific patches, offsets, import fixes, and server behavior are not
used here. Import reconstruction currently covers the recovered VMP loader table;
it does not establish that the dump can initialize on the installed game build.
