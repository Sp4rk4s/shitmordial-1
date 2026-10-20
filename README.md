# Primordial revival

Windows C++20 loader for the supplied x86 Primordial dump and legacy CS:GO.
The normal loader includes the original menu: Insert toggles it, and the user
confirmed dragging, tabs, checkboxes, multi-selects, keybinds, sliders and search.
**First feature test scope: bunnyhop and infinite duck stamina.** These two
controls now have a bounded native backend, and the user confirmed both work
in the actual game. The other gameplay features are not restored yet.

Current source also repairs all fourteen module resolver inputs and one proven
event-dispatch signature. All 25 native checks pass. The latest normal DLLs are
under `build/movement-first/`; that normal loader has not been game-loaded yet.
The current game retains its working menu loader with a movement-only diagnostic
using the same backend. See
[current feature tests](analysis/menu/first-movement-feature-tests.md),
[feature recovery](analysis/menu/feature-startup-recovery-findings.md) and
[menu findings](analysis/menu/native-ui-input-findings.md).

## Requirements

- Windows x64 with legacy **32-bit `csgo.exe`** installed and Steam signed in.
- Visual Studio or Build Tools with **Desktop development with C++**, the x86/x64
  MSVC toolchains, and a Windows 10/11 SDK.
- CMake 3.20 or newer available on `PATH`.
- Python 3 and the Windows `py` launcher for the injector tests and controller.
- The exact `primordial.bin` included at the repository root. Required SHA-256:
  `e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768`.

Run the commands below in **PowerShell from the repository root**. The build
scripts discover MSVC and the SDK themselves; a Developer Command Prompt is not
required. No IDA, x32dbg, MCP server, Unicorn or Capstone is required to compile
or use normal injection. The current menu profile targets the exact installed
Source DLL build documented in
[the profile](analysis/menu/menu-integration-profile.json).

## Compile

Build the loader, TLS companion and x64 Steam stager, then run the 25 checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
```

Build both architecture-matching LoadLibraryA helpers and run their isolated tests:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-injector.ps1
```

| Output | Purpose |
| --- | --- |
| `build/steam-x64-native/steam_module.dll` | Load into x64 `steam.exe` before game launch. |
| `build/primordial-x86-native/primordial_loader.dll` | Load into x86 `csgo.exe` after staging. |
| `build/primordial-x86-native/primordial_tls.dll` | Keep beside the loader; loaded automatically. |
| `build/primordial-x86-native/MinHook-LICENSE.txt` | Include when distributing the loader. |
| `build/injector/x64/loadlibrary_injector.exe` | Injector for the x64 Steam stager. |
| `build/injector/x86/loadlibrary_injector.exe` | Injector for the x86 game loader. |

Use `-VisualStudioRoot "C:\path\to\Visual Studio"` on either build script if
automatic discovery fails. `build.ps1 -OutputDirectory build/my-build` changes
both DLL output folders; select the same root with `test_menu.py --release`.
`-SkipTests` skips checks. `-SteamArchitecture x86` is only for a 32-bit Steam
installation; the game loader must always remain x86. The automated controller
currently supports x64 Steam.

Optional: `build.ps1 -MenuPreview` also compiles a separate diagnostic preview.
Normal injection uses the integrated menu and does not need that DLL.

Keep `primordial_loader.dll` and `primordial_tls.dll` together when moving them.
The Steam DLL can live in a separate folder. Inject the loader only; it loads
its TLS companion automatically.

## Inject automatically

Start with Steam running and signed in, and CS:GO closed. If Steam already has a
stager from another build folder pinned, restart Steam manually first.

Check current processes and any existing payload state:

```powershell
py -3 .\tools\test_menu.py status
```

Load the Steam stager, launch legacy CS:GO, wait for staging/game modules, and
inject the normal loader:

```powershell
py -3 .\tools\test_menu.py run --launch --game "C:\Program Files (x86)\Steam\steamapps\common\csgo legacy\csgo.exe"
```

Change `--game` to your actual legacy installation. The controller defaults to
`--release build`, matching the compile commands above. For an alternate build:

```powershell
py -3 .\tools\test_menu.py run --launch --release build/my-build --game "D:\Games\csgo legacy\csgo.exe"
```

Every automated launch passes **`-windowed -insecure -steam`**. Do not use Steam's
app 730 launch command here: on this installation that selects CS2.

After the controller reports that the integrated menu completed its first draw,
press **Insert** to show/hide it. Gameplay features remain unresolved even when
menu controls respond.

If the correct Steam stager already prepared a fresh running game, inject just
the normal loader through the controller:

```powershell
$gamePid = (Get-Process csgo).Id
py -3 .\tools\test_menu.py run --game-pid $gamePid --game "C:\Program Files (x86)\Steam\steamapps\common\csgo legacy\csgo.exe"
```

Select one explicit game PID if multiple processes exist. Each game process can
initialize only once. After changing the loader or an initialization failure,
close that game and launch a fresh process; repeatedly injecting into an already
initialized or failed mapping will not reload it.

## Inject manually

Use this order if you prefer the native helpers directly. Start with Steam signed
in and the game closed. These commands assume one `steam.exe` and one game.

1. Load the x64 stager into Steam:

   ```powershell
   $steamPid = (Get-Process steam).Id
   $stager = (Resolve-Path .\build\steam-x64-native\steam_module.dll).Path
   .\build\injector\x64\loadlibrary_injector.exe $steamPid $stager
   ```

2. Launch the actual legacy game in windowed insecure mode:

   ```powershell
   $gameExe = "C:\Program Files (x86)\Steam\steamapps\common\csgo legacy\csgo.exe"
   Start-Process -FilePath $gameExe -WorkingDirectory (Split-Path $gameExe) -ArgumentList '-windowed', '-insecure', '-steam'
   ```

3. Once the game has loaded, use `test_menu.py status` to verify that
   `handoff_state` is 1 (staged). Then load the x86 loader, keeping its TLS DLL
   beside it:

   ```powershell
   py -3 .\tools\test_menu.py status
   $gamePid = (Get-Process csgo).Id
   $loader = (Resolve-Path .\build\primordial-x86-native\primordial_loader.dll).Path
   .\build\injector\x86\loadlibrary_injector.exe $gamePid $loader
   py -3 .\tools\test_menu.py status
   ```

A helper's successful LoadLibrary result only means its DLL was loaded. Use the
controller/status and game log to distinguish payload entry completion from an
actual rendered menu. See [injector details](tools/injector/README.md) for helper
exit codes and optional debugger attachment.

## Status and troubleshooting

- `handoff_state`:1 staged,2 patching,3 entry completed,4 failed.
- `menu_state`:0 dormant,1 waiting for paint,2 constructing,3 first draw returned,
  4 failed. A 3 handoff alone does not prove menu success.
- `primordial_menu.log` beside `csgo.exe` records UI setup and errors.
- Crash diagnostics/minidumps are written beside the game by the crash logger.
- Missing staging: ensure the Steam stager was loaded before game launch and use
  a fresh game process. The game loader does not allocate or remap the raw dump.
- A Source profile mismatch means the installed DLL build differs or its guarded
  entry is modified. Review the exact profile instead of disabling its checks.
- Load `steam_module.dll` into Steam and `primordial_loader.dll` into CS:GO, with
  the matching architectures. Do not inject `primordial_tls.dll` separately.
- Restart Steam to replace its pinned launch-hook DLL. The controller preserves
  existing game/debugger sessions and reports conflicts instead of restarting them.

## Repository contents

Source, tests, tools, research notes, annotation indexes, the original payload and
vendored MinHook libraries are tracked. Generated builds, IDA databases, Visual
Studio caches, crash dumps and machine-specific runtime captures stay local via
[.gitignore](.gitignore). Existing local analysis artifacts are not deleted.
The optional forensic scripts may need fresh captures or ignored archived inputs;
normal compilation and injection do not depend on those captures.

See [loader design](docs/loader-design.md) for the staging protocol, import repair
and fixed-base limitations, and [AGENTS.md](AGENTS.md) for the detailed handoff.
