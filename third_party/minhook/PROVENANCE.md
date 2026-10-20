# MinHook dependency

Upstream: https://github.com/TsudaKageyu/minhook

The environment blocked the attempted v1.3.4 archive download. This project instead
vendors the existing local x86 MinHook distribution, including its copyright and
license notices. These are the 2017-era v141 libraries; no claim is made that they
are v1.3.4. They are linked statically into the x86 Primordial DLL and hook test.
Steam does not link MinHook. No machine-wide package installation is required.

Copied header and libraries from
`C:/Users/k/Desktop/Fatality.win-Source-main/internal_hvh/misc/`:

| Original | Vendored path | SHA-256 |
| --- | --- | --- |
| `MinHook.h` | `include/MinHook.h` | `62dff05e63155c6bf8135c36a0d79dedbdcaaede9da26fe38daf0bea34323e38` |
| `libMinHook-x86-v141-mt.lib` | `lib/minhook-x86-release.lib` | `f7d0e85c6865cf6d9bdcf6c9090317a58aa19a177096013493bc206b6700df83` |
| `libMinHook-x86-v141-mtd.lib` | `lib/minhook-x86-debug.lib` | `7fabacbc14dbc79c8b030f9a5bc14e2872ea62dd6ebf3e82af8085d17d6acac3` |

`LICENSE.txt` was copied from the local MinHook distribution at
`C:/Users/k/Desktop/daun/example/minhook/LICENSE.txt`, SHA-256
`621d5961e3e462ccae8faa39edca119362aa4ca69a73ccaa7592e2986c8eed11`.
It contains the MinHook and bundled component notices; preserve it with binaries.

COFF inspection confirms x86 (`0x14C`) for the release library. The CMake import
selects the matching debug/release static-CRT library. Primordial's targets use
`MultiThreaded` / `MultiThreadedDebug` to match these libraries.
