# Latest missing-menu run: PID27952

The canonical primordial_loader.dll and primordial_tls.dll are both loaded.
Loader PE timestamp6AD685CB, image size4D000; companion timestamp6AD685A9.
Protocol2 handoff at45751000 has state3, establishing a nonzero configured entry
return. TLS index at44B66264 is51. Alphabet storage is fresh at38D85B90.
The prior loader repairs are active in this run.

Interface cache44B486E4 now has fresh head31C3E2D0 and count0. Worker count
44B49564 is0. The captured counts29/11 are gone, but native initialization has
not populated them. Querying all77 thread start addresses found none in the
payload's mapped range; this does not exclude CRT trampolines or callbacks.
Raw observations and loaded module paths are retained in runtime-27952.json.

The game menu remains absent. Tests and loader-return success are insufficient
to establish a functioning payload. The configured unprotected CRT attach path
has not been shown to invoke the actual game/render bootstrap. Original protected
entry, an external initialization contract, and constructor failure remain
possibilities; none is established enough to patch or execute a new address.
The original protected DLL or originating loader would provide important missing
entry/bootstrap evidence. Asked user for a local path while continuing analysis.
No further behavioral patch or arbitrary candidate function execution was done
for this report.
