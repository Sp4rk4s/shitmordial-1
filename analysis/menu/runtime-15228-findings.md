# Missing menu after Source resolver correction — PID15228

Read-only process observation on 2026-10-20 confirms the replacement build is
active, not a stale canonical DLL:

- `build/resolver-fix/primordial-x86-native/primordial_loader.dll`, timestamp
  6AD68895, image size4E000; on-disk SHA256 matches resolver-fix/validation.json.
- Matching `primordial_tls.dll` is loaded; payload TLS index is51.
- Handoff protocol2 state3: the configured entry thread returned nonzero.
- All twelve game module hashes and HMODULEs are correct for the current process.
  Their reconstructed export caches have zero entries.
- The interface cache has a fresh head and count0; worker count is0.
- Alphabet storage is freshly allocated; no prior memcpy exception is reported.

Full evidence is `runtime-15228.json`. The read-only capture is now reproducible
using `py -3 tools/capture_payload_state.py <pid>`; it does not suspend threads,
write remote memory, invoke payload functions or change the debugger.

The user's menu remains absent. The previous repair addresses a real resolver
regression but is insufficient to establish menu startup. Cache/worker counts
are evidence of limited initialization, not a complete trace of every callback.

The configured CRT entry calls user DllMain; its attach branch calls only the
helper at RVA A1E50. Its observed body ends at A1FB4 and performs profile/string
and RNG state setup. No direct game bootstrap, render-hook installation or
thread creation was identified in that body. Actual game initialization may be
in protected code, an unrecognized callback or an external loader contract.
Do not substitute an arbitrary function address as a new entry point.

Further bounded investigation retained:

- `bootstrap-target-branches.json`: no direct E8/E9 edges to the interface lookup
  at RVA358E60 or worker setup entry362970. Protected indirect flow remains
  unresolved. This absence does not prove those functions are unused.
- `protected-constructor-calls.json`: byte-scanned leads, not a verified CFG.
  There are154 initializers with candidate protected calls. Many share a
  400-byte-stack-buffer/version2 pattern consistent with Winsock initialization;
  that interpretation is not fully traced through VMP. Do not call them menu
  bootstrap based on their protected targets alone.
- `loader-contract-name-cipher-scan.json`: the alleged g_aHooks/g_aOffsets/
  g_aInlinedOffsets/g_aNetOffsets/g_aImports names are absent under both plaintext
  and the known VMP import-name cipher search. Other encodings remain possible.
  Their alleged loader-export contract remains unverified.
- IDA currently has no recovered entry-point records (`ida-get_entry_points.json`).
  Existing named CRT entry was manually identified from its instructions.
- `payload-disassembly-carrier.exe` is a synthetic PE used only by dumpbin to
  disassemble bytes. Never execute it or treat its header as original metadata.

The user confirmed the original protected DLL and originating loader are not
available. Do not repeatedly request them. Continue from the retained binary,
IDA database and runtime evidence. This run adds no speculative behavioral patch
and does not require another rebuild/reinjection of the same code.
