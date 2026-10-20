# Latest real-game retest: 38484, menu absent

User explicitly requested testing/reinjecting the latest build. Old11240 was
closed; a pending debugger stop required debug_stop to finish that requested
closure. x32dbg and IDA were left open. Fresh csgo.exe38484 launched from the
verified legacy installation with -windowed -insecure -steam, without attaching
the debugger. Steam324 retained its existing resolver-fix stager; fresh raw
handoff/checksum validation succeeded. No second stager was loaded.

Loaded patcher: build/menu-render-guard/primordial-x86-native/primordial_loader.dll,
SHA256dc289cf58adde35f8cd5c2b38404aa181341634a876745d1178c6a71d2e62cf7,
with adjacent primordial_tls.dll. Handoff3/entryTRUE. The user confirmed **NO MENU**
after pressing Insert. This supersedes the earlier not-game-loaded status for
menu-render-guard, without establishing a resolved menu.

menu-render-guard-retest-38484.json verifies all seven captured strings reset,
the vector zeroed and selector branch7F active. Surface/Input/EngineClient/timing,
both original screen/paint callbacks, screen dimensions and window vector remain
ZERO. Actual missing initialization persists despite the supported bounds and
ownership repairs. runtime-38484.json records modules/resolvers and disk hashes;
latest-menu-render-guard-retest.log records the run. Existing validation.json now
records the actual no-menu result.

## Reported exception406D1388 was not a process crash

The user supplied a19:41:11 report labeled CRASH/UNKNOWN with exception406D1388,
KernelBase address7667B702, and four parameters beginning1000 and ending0. This
matches Microsoft's legacy x86 thread-naming protocol:
https://learn.microsoft.com/en-us/visualstudio/debugger/tips-for-debugging-threads

The actual game38484 remained running and responding after that observation.
A first-chance vectored observation is not itself evidence of an unhandled
crash. The old logger labeled every nonfiltered observation CRASH and wrote a
dump, including valid thread-name and Unicode debug-string notifications.

crashlog.cpp now omits exact valid thread-naming records plus ANSI/Unicode
debug-string notifications, retaining EXCEPTION_CONTINUE_SEARCH throughout.
Malformed thread-name records still produce diagnostics; other observed faults
are labeled EXCEPTION OBSERVED because fatality is not determined at that stage.
Single-step has a diagnostic name, execute-access faults are labeled correctly,
and region labels use the configured synthetic section rather than old guesses.
No debugger policy, exception consumption or payload startup behavior changed.

New native exception-notifications test raises these notifications under an
ordinary SEH handler, verifies they still arrive at that handler and do not
overwrite a retained dump, then checks malformed thread-name and access-violation
diagnostics/minidump. All14 x86 and6 Steam tests pass under build/exception-log-fix;
artifact hashes and results are in its validation.json. That logger build is
NOT loaded into38484, and is not a menu bootstrap repair. Keep the current game
instead of reinjecting a logging-only build expecting menu initialization.

## Continued original-bootstrap investigation

Fresh process pages/exports and a read-only, Self/ClientId-verified x86 TEB were
captured for offline work. No debugger attach or game write occurred. Pure helper
155960 is now retained live in IDA as masked_pattern11_find_first, preserving its
original boundary. Its strict upper bound and unconditional first-byte compare
were checked against original x86 bytes under106 independent synthetic emulator
cases. This is an analysis acceleration model, not a production scan fix.

Protected1564B0, previously only known to touch m_vecVelocity, scans current client
code for558BEC83EC18568BF1C745, finds57A797A0, then reaches VirtualAlloc for an
11-byte executable trampoline via native15566C. This concretely identifies a
hook-construction activity, not yet the whole initializer or original caller.
Evidence candidate-1564b0-constructor-native-crt-38484-scan11-model.json.
Any further allocation/protection success models are explicitly emulator-only;
no target allocation, hook, subsystem call or guard substitution is authorized
or performed by those models. Full protected caller and bootstrap remain open.
