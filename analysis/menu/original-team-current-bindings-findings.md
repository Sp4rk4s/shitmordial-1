# Original team-gate current bindings (2026-10-21)

Read-only PID11188, independently rediscovered by csgo.exe. No writes/calls/map loading. Source client identity is recorded in chams-team-fields-11188.json.

Original36B2B0 old operand cells map by actual current RecvTables:2DB22972 -> m_iTeam resource array0xF38 (DT_CSPlayerResource/DT_PlayerResource and standaloneDT_PlayerResource);2D92D648 -> m_iTeamNum0xF4 (nested DT_CSPlayer/.../DT_BaseEntity);2DB5D56F -> m_nSurvivalTeam0x10410 (DT_CSPlayer). Resource m_bAlive remains1140. Current classIDs CCSPlayer40,CCSPlayerResource41,CPlayerResource132. No guessed offsets from a reference catalog are used.

vstdlib.dll public CreateInterface registration VEngineCvar007 factory734D33A0 is MOV EAX,7350D310;RET. Its registered-command list begins object+30, corroborated by native CCvar methods that read +30 and traverse item+4. Walked5179 unique nodes and read exact names at node+0C:

- game_type object5A396348, FNV1a371BF260, integer0,string0.
- game_mode object5A3962E0, FNV1aD2C5001B, integer0,string0.
- mp_teammates_are_enemies object5A37FD80, FNV1aCF4E1864, integer0,string0.

All parents at ConVar+1C self-point. Actual virtual13 routine59728060 ultimately returns DWORD[this+30] XOR this, or forwards parent virtual13. Values independently agree with strings. Method also performs consistency/check callbacks; raw decoding is read-only observation, not authorization to emulate its side effects live. Native original354530 stores these exact three hashed ConVar virtual13 results into4DE310/314 and boolean4DE318. Supply actual public Source values to an original code capsule; never force them to choose desired teams.

Original entity-list factoryVClientEntityList003 -> singleton5E3B51DC. Current slot3 at597FD970 checks index bounds, loads networkable entry, and calls returned object's virtual+18 for baseentity; slot4 at597FDA50 gets handle and performs same baseentityconversion. No player entities1..64 are present in this disconnected capture. Native localcontext4481E210 is zero; resourceclient+3231380 zero. Therefore actual player networkable9/10 runtime method identities cannot yet be confirmed from a live player. No map/team change was made just for capture.

Public Engine014 local-index12 method6EB26800 obtains splitscreencontext from6F00F198 virtual5; current method6EACBAF0 is XOR EAX,EAX;RET, then accesses table6F00F19C index0 -> contextFBCD0010 +180 and adds1 (currently index1). This does not establish an actual entityexists. Full capture files chams-current-factories.json,chams-current-cvars-11188.json,chams-team-fields-11188.json retain addresses/bytes/paths.
