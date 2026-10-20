# Protected entry stub inventory

Original primordial.bin SHA256e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768. Offline static inspection only. RVA addresses; runtime base44340000, IDA analysis base44000000.

Byte scan of original native text1000..40D000 found307 E9 opcode leads whose rel32 targets fall in VMP region827000..140C000. Twenty-four satisfy at least one entry-boundary heuristic: existing IDA function start, preceding CC byte, or16-byte alignment. Four are existing IDA starts, ten additional entries have preceding CC, and ten are alignment-only weak candidates. This is an inventory, not a CFG or a list of verified functions. Existing IDA function inventory contains20000 entries through9D7F76, covering all original text candidate addresses.

None of the24 has CC padding immediately after its five-byte E9. Subsequent bytes frequently contain VM-handler/mutation fragments or apparent encrypted data. Thus native+5 must not be treated as a clean prologue. First following three-CC run is only a span heuristic, not a verified function end or proof of erasure. Entire menu constructor2E04D0 has no such run within64K.

| Entry RVA | Boundary evidence | VMP target RVA | First three-CC distance |
|---|---|---|---|
|0x9b730|alignment_only|0xcb204d|1898|
|0xa2ee0|alignment_only|0x975847|1112|
|0xa3240|alignment_only|0x10503af|248|
|0xa4490|alignment_only|0x8cfc5a|18|
|0xb5320|preceding_cc|0xbc18c1|850|
|0xe2030|preceding_cc|0xc1fe4e|890|
|0xe2290|alignment_only|0x86701c|282|
|0xf56f0|alignment_only|0xb633fd|697|
|0x1564b0|preceding_cc|0xb3f1e2|5418|
|0x162fa0|preceding_cc|0xade661|1058|
|0x27f100|alignment_only|0x91f114|101|
|0x2cab40|preceding_cc|0xb11d93|4106|
|0x2e04d0|preceding_cc|0xd3d3a6|None|
|0x352b80|known_ida_start|0xb1dbfc|4457|
|0x352dd0|alignment_only|0x90f6c8|3865|
|0x3542c0|known_ida_start|0xa17f01|619|
|0x35aee0|preceding_cc|0xa98ebd|109|
|0x360080|known_ida_start|0xd91059|1930|
|0x360790|alignment_only|0x1041ffc|122|
|0x362970|preceding_cc|0xcd422c|320|
|0x36e110|preceding_cc|0xcc71c9|92|
|0x3752f0|preceding_cc|0xa5fc08|1720|
|0x375300|alignment_only|0x89466a|1704|
|0x37bf70|known_ida_start|0xb1534a|301|

## Promising subset and concrete native callers

3542C0 is an IDA five-byte stub immediately following interface/cache helper354170. Its post-stub619-byte span shows VM-handler-style arithmetic/branches, not a clean native continuation. A real native CALL atA2291 reaches3542C0 from function candidateA21C0. The same native routine calls352B80 atA23AF and37BF70 atA23B4. A21C0 already depends on Source interfaces4481E174/E170 and globals4481E164, resets game-state values, sets convar-like methods using reciprocal tick interval, and rebuilds a container at4481E1F0. Native wrapper14FD60 first CALLs original pointer44B3B680 then JMPs A21C0 at14FD66. Adjacent14FD70 forwards another original pointer44B3B684 then clears the same game-state globals. These facts support a level-lifecycle role, but no exact API name/ABI or full reversal is claimed.

360080 is an IDA five-byte stub with1930 bytes before the firstCC run. Native wrapper154770 pushes two stack args, CALLs original44B3B880, then CALLs360080 at15477F and RET8. This establishes a callback dependency, not bootstrap ordering.

36E110 has a precedingCC boundary and92-byte span beforeCC; native continuation is absent in the inspected span.360790 is only16-byte aligned, with no precedingCC; it may be an internal fragment rather than a callable function. Follow-up offline emulation is owned by protected_callers agent, using documented synthetic guard fields only inside Unicorn.

1564B0 is a precedingCC entry immediately after a dense area of native hook callbacks, with5418 bytes beforeCC. Its true role is not established by proximity. The protected_callers agent's bounded synthetic model subsequently reached a native string assignment whose source decodes m_vecVelocity, supporting netvar/property processing as an initial activity. This does not establish whole-function behavior or a startup-call ABI.

For all24 candidates, a bounded direct E8/E9 byte scan found incoming branches only for352B80,3542C0,360080,37BF70 as described above. Full runtime-VA byte occurrences were otherwise absent except self-marker pairs for already-known config/menu/worker/localization roots. Protected/indirect callers remain unresolved. This new inventory does not establish a top-level coordinator.

No candidate was executed live; no payload/source/debugger state or IDA name/type/comment/index was changed. No new function was completely reversed. Evidence protected-entry-stub-inventory.json includes all307 raw leads,24 selected entries, boundary bytes, instruction previews, and reference leads; protected-entry-caller-boundaries.json/protected-entry-known-callers.json retain focused caller query results.
