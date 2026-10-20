# xdbg MCP setup — 2026-10-20

Installed upstream SetsunaYukiOvO/x64dbg-mcp release v1.0.11 into the user's
existing debugger distribution. Both downloaded plugin SHA-256 values match
the GitHub release asset digests; architecture headers were checked before copy.
See `installation.json` for hashes, exact paths and upstream release URL.

| Debugger | Executable | MCP endpoint | Codex server name |
| --- | --- | --- | --- |
| x32dbg | `C:\Users\k\Desktop\re\release\x32\x32dbg.exe` | `http://127.0.0.1:3000/mcp` | `x32dbg` |
| x64dbg | `C:\Users\k\Desktop\re\release\x64\kiausas.exe` | `http://127.0.0.1:3001/mcp` | `x64dbg` |

The x64 executable is renamed in this installation; its version information
identifies the product as x64dbg. CS:GO is x86, so use the x32 endpoint for it.

Plugin configurations are under each architecture's `plugins` directory:
`x32dbg-mcp/config.json` and `x64dbg-mcp/config.json`. Both enable automatic MCP
startup and bind only to 127.0.0.1. Upstream permissions retain breakpoint
modification and execution control, with arbitrary memory writes, register writes
and script execution disabled. No firewall changes were made.

Registered both Streamable HTTP endpoints with `codex mcp add`. The existing
`C:\Users\k\.codex\config.toml` was backed up beside itself with a
`config.toml.before-xdbg-<timestamp>.bak` name before registration; the existing
REA registration remains present. No IDA configuration was changed.

Both debuggers were launched without a target using hidden-window startup.
Both endpoints passed initialize, tools/list and tools/call(debug_get_state).
Each advertised 80 tools and returned state `stopped`. Complete inventories and
smoke responses are retained in `x32-tools.json`, `x64-tools.json`,
`x32-smoke.json` and `x64-smoke.json`. These checks establish bridge availability,
not successful attachment, thread-context access or game debugging.

IDA metadata was independently queried read-only: input primordial.bin, baseline
SHA-256 e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768,
analysis base 0x44000000. Its GUI/database were left open and unchanged.

The current agent can call the MCP endpoints through local HTTP JSON-RPC using
the discovered schemas. A Codex client reconnect/restart may be needed before
the newly registered servers appear as native tools in an existing conversation.
Use tools/list to discover exact schemas, then tools/call with name/arguments;
do not assume the older IDA bridge uses the same protocol.

No game was launched, attached, suspended, resumed or patched for this setup.
No payload or synthetic analysis carrier was executed. Future game debugging
must discover the current PID and preserve existing debugger sessions.
