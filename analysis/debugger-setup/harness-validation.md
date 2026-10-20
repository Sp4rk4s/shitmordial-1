# LoadLibraryA harness verification — 2026-10-20

Built standalone helpers under build/injector/x86 and build/injector/x64 with
MSVC 19.52.37025. Both CTest roundtrips pass against disposable probe hosts.
Manual cross-architecture probes reject both x86-to-x64 and x64-to-x86 targets
before remote allocation/thread creation.

Production verification:

- Steam PID324 accepted the x64 resolver-fix steam_module.dll through LoadLibraryA.
- First game PID28232 exposed debugger-generated TLS callback breakpoints in
  nvgpucomp32.dll before the remote thread reached LoadLibraryA. The helper
  returned pending (3) and retained its live argument instead of freeing it.
  Read-only modules/stack/disassembly/breakpoints are in
  analysis/menu/debug-launch-28232.json. This is a debugger stop, not a verified
  payload crash or completed reversal of the NVIDIA callback.
- The user required windowed mode and -insecure. Automated launches now include
  -windowed -insecure -steam. The first agent-created test instance was stopped
  through x32dbg and relaunched to apply those flags.
- Updated debug setup disables the fresh debugger's auto-created TLS software
  breakpoints and uses a hardware execution breakpoint at payload entry. This
  avoids changing staged bytes before raw checksum validation.
- The corrected run launched PID8068, loaded the x86 patcher at563B0000, and
  paused at EIP4470A002 with handoff state2. Register/stack access is now available.
  See analysis/menu/debug-start-8068.json. The process is intentionally paused
  before entry; no menu success is asserted.

No payload behavioral changes or rebuilds were made. Existing resolver-fix DLLs
and the original dump remain unchanged. Native entry/constructors still need
tracing to identify the missing game initialization path. IDA remains open;
this setup did not reverse or annotate any new payload functions.
