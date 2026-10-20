# Bounded static bootstrap investigation, PID8068 session

Input primordial.bin SHA256 e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768 was rechecked. RVAs use original bytes; IDA base44000000, runtime44340000. This is static candidate inspection, not a verified startup CFG or completed function reversal. No payload/source or IDA annotations changed.

## Worker entry and inbound references

Worker candidate RVA362970 is E9 to CD422C. CD422C begins CALL DE1514 followed by apparent encrypted virtual-machine data. DE1514 begins PUSHFD, ADD [ESP+4],EF32BDCF, saves registers and enters an opaque branch/call network through AD66E9. A native prologue is not available at362970. Direct E8/E9 scan finds only362970 itself enteringCD422C. Near-Jcc/E8/E9 scan finds no incoming branch into native worker block36299B..362A95. Interface range358E60..359177 has ten internal near branches but no external near branch. Evidence bootstrap-body-branch-leads.json retains all matches and scan limitations. This does not exclude indirect calls, virtualized dispatch or short local branches.

Native worker continuation begins PUSH 446A2970 at3629A0 then CALL717E0 (existing null helper). Its own marker/end references to446A2970 are the only useful plaintext runtime pointers to the worker range. A third unaligned four-byte match at9CC429 points inside the immediate operand at362A04 and is not established as a pointer.

## Correction to protected-constructor-calls.json

That historical file contains154 initializer candidates,157 alleged protected calls. Only151 match CALL instruction boundaries in saved constructor-disassembly.txt. The six alleged calls in long initializers4AC0/6A90/70D0 are false positives:5294,5A11,5A34,6DD1,71D4,71F7 occur inside C7/C6 stack displacement operands. Thus these three long native initializers are not protected-call outliers. Their observed content constructs string/value collections (including color values).

All151 aligned protected calls have the same general Winsock-initializer shape: allocate190h or198h stack buffer, LEA buffer, PUSH buffer, PUSH2, optional saved register, protected call, then register an exit destructor. Shape is consistent with repeated WSADATA/WSAStartup setup, but effects of each protected callee remain unverified.

## Captured guard search

All1810 distinct initializer starts in constructors.json were found at instruction boundaries in saved constructor-disassembly.txt. Scanning each linear prefix through first RET or next initializer found no indirect CALL, indirect JMP, unconditional JMP, or absolute-memory CMP/TEST. Only six initializers contain conditional operations:6A90 container iteration,654D0 fixed stack-span subtraction/zero check,655C0 MT seed loop,682C0/68310/6C210 fixed clearing loops. No direct initializer-level captured once-flag guard was found. This bounded approach does not cover indirect/protected callees or alternate paths beyond a first linear RET.

Rare native callee pseudocode retained in constructor-rare-native-callees.json: candidates predominantly string construction, color/value map construction, vector allocation/copy, JSON-like tagged storage and stream construction. They do not directly establish game bootstrap. Candidate DC300 fails decompilation, but native bytes show input-byte dispatch through a jump table allocating/zeroing representations of different sizes, consistent with tagged value construction. No full ABI/type/name claims were added based on these inspections.

Auth patch caller at166400 is within Lua/script-state setup content: luaL_newstate-shaped helper380C90, Lua setup1671C0, registry indices-10000/-10002, ffi/user/name registration. It is not established as native game initialization.

## Remaining gap

Static entry/constructor examination does not identify an invocation of worker setup or interface enumeration. Protected callee effects and an external/virtualized bootstrap contract remain unresolved. No guessed function should be called based on this report.

## Lua global initializer699A0 follow-up

Parent live tracing established that constructor699A0 executed. Its native instructions call380C90 to create a Lua-state-shaped object, store it at runtime44B3BAF4, initialize adjacent wrapper44B3BAF8 via15F740, then call1671C0 with callbacks443DE5C0/4449A020/444A2830 and register destructor4472F9C0. This is a real exception to treating all constructors as passive storage-only initializers.

Plaintext absolute44B3BAF4 references in the original dump are only at699AC (store),699C6 (argument to1671C0),3EFA07 (destructor load). Adjacent wrapper44B3BAF8 occurs only699A7; fields44B3BAFC/44B3BB00/44B3BB04/44B3BB08 appear only in its destructor. Encoded/protected/computed accesses remain outside this scan's coverage.

Retained pseudocode for1671C0 sets callbacks/registry values under sol-prefixed keys; runtime44767CD8 contains sol.??,44767C70 contains sol.??,4475D4E4 contains sol.??. Its tail pushes callback444A1A10 then calls38D9E0(L,-1,0x110), consistent with LuaJIT WRAPCFUNC|ON mode. This latter API identity is an inference pending native/source corroboration. Finally it saves L to static TLS block+4. No script byte buffer, file load, or protected Lua call was identified in this setup helper. Thus state construction is verified statically; actual embedded/external script execution and any game-bootstrap effects are not established here.
