# Active-player ESP native cache blocks (2026-10-21)

Read-only/offline continuation of original-esp-recovery-findings.md. No production edits, live hooks/calls, IDA changes or shared index mutations. Original bytes own every producer decision/math below; no replacement cache algorithm exists.

## Concrete producer closure

The original large F0B30 can be narrowed by **verified original code blocks**, preserving their register/frame contracts. `probe_original_active_esp_blocks.py` executed14 cases without process access and retained SHA256/ranges/exact writes in original-active-esp-block-proof.json. This is a native-block dependency proof, not a complete ESP/gameplay result.

Record address is448BAB40 + index*95E0, so index1 is448C4120. Every block uses EDI=index*95E0 (not direct record pointer). All record addressing and arithmetic remain native.

| Original block, end exclusive | Required live-ins | Native output / exact instruction writers |
| --- | --- | --- |
| F0D1F..F0D39 |EDI;record+4 actualentity;original36ACC0(false)| Calls original enemyhelper atF0D27; F0D33 stores record+148 as original booleancategory. No other feature decisions. |
| F10E2..F1123 |EDI;record+4 actualentity;realGlobals4DE164;entity+4 Renderable vtable1 GetRenderOrigin| F10F2 writes record+92=Globals realtime0;F10F8 writes+88=0;F110B/F1114/F111D write+36/+40/+44 originreturned by native Source. No name/weapon/animation dependency. |
| F13BF..F147A |EDI;Globalsrealtime;record+96 lastdormanttime;+100 dormantopacity;EBP doubles-80/-88/-C0|F13D9/F1472 write onlyrecord+104 activeopacity. All interpolation/clamps and gateelapsed>.25 are original instructions; scalarSSE sin import3CA7A3 is the only external call. |
| F147A..F1581 |EDI;resource POINTER SLOT4DE1B0;nativehealthoffset cache7DB920/924;networkable index10;Globals realtime| Native health reads/clamps0..100. If changed,F1503 writes+76 time,F150E writes+80 previoushealth,F157B writes+84 currentclampedhealth. No lagcomp/history-ring dependencies. |

The opacity EBP live-ins are copied **directly from original F0B30 prologue**, not guessed numeric state:

- F0BA9 copies double447A0D78 to[EBP-80].
- F0BBF copies double447A0D58 to[EBP-88].
- F0BD5 copies double44753E40 to[EBP-C0].

Original origin block is the native **non-dormant branch** atF0FE8 ->F10E2. It must never be used as an agent-written shortcut to populate dormant locations. F03E0 is NOT an alternative active producer: it mostly repeats entity assignment and handles dormant occlusion/opacity timeout. It does not populate active origin/category/health/history.

## Resource health dependency resolved

Fresh read-only actual receive tables show DT_PlayerResource m_iHealth **1184** in both CPlayerResource and CCSPlayerResource; CBasePlayer m_iHealth100 is a different field. Original37B6E0 lookup hash **40A2AF50** resolves the resource health array. IDA decompilation incorrectly shows low constant80; raw PUSH40A2AF50 atF149A/F1527/13A326 is authoritative. Producer and draw routines share cache7DB920 (offset)/7DB924(initializedbit). An offset adapter may bind a proven current field here; it must not translate health behavior.

Original native helper13A2F0 renders health **bar**;13A660 renders health **text**, not armor. Both consume the same resource health index and existing native geometry, native health-history fields76/80/84 and realtime for animateddamage display. Their formulas remain native. Earlier broad report calling these health/armor candidates is superseded.

## Draw live-ins for bounded name/health path

137310 gates original enablekey object runtime44860BA8 + category*178 (vtable8), obtains dormant flag through actual entity networkable9, uses opacity+100 dormant /+104 active, and copies record category148,origin36/40/44,geometry152/156/160/164 into original script draw context. Native geometry invalid/offscreen flag108 stops normal on-screen draw. F06C0 itself computes geometry and flag108; do not fill rectangles in adaptercode.

- Name13A9F0 obtains original name via369A50 -> Engine GetPlayerInfo8, native string sanitization/shortening and UTF16 conversion; no cachepopulatedname is required. Actual helper ABI/raw protected continuation still needs closure.
- Health13A2F0/13A660 require resource-pointer indirection plus1184, networkable index10 and nativehealthhistory fieldsabove. Parent already has original alive/team/observer nativecapsules for requiredclassification.
- Existing live ESP fonts are populated in11188: runtime44B1C8EC={handle130,0,17},44B1C8F8={handle132,7,14},44B1C904={handle134,6,11}. These are real readbacks, not a synthetic font requirement. Menu/font compatibility can provide valid Surface context.
- Full137310 also visits original script callbackbucket13 through158B70/162D60. An empty actualnative initializedscriptlist may take natural emptybranch; production must not install fake fixturelists or bypass callbacks to hide unresolvedstate.
- Its unconditional139110 is a large **flags/text** routine, not a simple bounding-box renderer; raw branch139165 jumps over inlinebytes to1391E6. It has extra resourcefieldhashes and a stale directfunction2DCB8890 when certain flags are enabled. Keepunsupported options guarded by verifiednativegates, not blanketfunctionsubstitution.
- Native skeleton13AE00 consumes original bone/cache+24848 validity and studio/modeldependencies; these active-cache snippets do not close skeletonESP. Weapon13AFB0 requires nativeweaponwstring120/valid116/item144. Do not claim these work from origin-onlyblockproof.

## Exact current original UI controls

Read-only native encoded label inventory is retained esp-native-current-control-labels.json. Main enemy/team pairs use separate contiguous objects and nativecategory strides, not a new UI:

- runtime44860BA8 `enable key`,44860D20 secondcategory `enable key`.
-44860FD8 /44861108 `background frame`;44861238/44861498 colors.
-448616F8/44861834 `name`;44861970/44861BD0 `name color`.
-44862118/44862254 health `bar`;44862390/448624CC health `text`;44862608/44862744 `hide when full hp`.
-44863200/4486335C `active weapon`;44863978 etc `ammo`.
-448654C0/44865634 `flags`; individualflagobjects44866128 armor,448663A0 money,44866618 ping,44866890 bomb,44866B08 defuser.

Do not infer that every property is independently enabled merely from its control responding. Native keybind dispatcher/gates and nested controls must execute original code.

## Concrete next capsule/replay

Build an ISOLATED x86 native harness using intact original ED330 entityiterator, actual original local/alive/team helpers, and an explicit TEST callback that supplies the verified register/frame live-ins to the four unchanged blocks. For an explicitly scoped **active-player** test, original ED330 flags0 naturally filters dormant/dead entities; source13ACC0 normally uses22, so this is not a claim of full original dormant-ESP restoration. Adapter entity iteration/filtering must not replace ED330. Preserve recordbuffers between frames to validate native opacity/healthhistory evolution and actual pointer indirection.

Next execute originalF06C0 geometry with nativeprojection362810 (parent already verified) and then untouched13ACC0/137310 with only name/health options enabled under genuine originalmenu state. A production subset requires proof that all invoked native script/font/flag helpers have closed dependencies. Verify codeblock entrygates/lifecycle before isolatedfragments enter livefeatureplumbing; do not copy fixtures or synthesize unusedcachefields. FullF0B30 lagcomp mutation remains excluded, but these specific basicvisual producers no longer depend on it.

The14case offline receipt verifies categoryfalse/true, twoexactSourceorigins, fiveoriginalopacity branches and fivehealthclamp/history branches. Its math-import model is explicit ordinary sin, and external category/renderable/index fixtures are testonly. It does not establish actual rendering, Source ABI guards or fullESP readiness.
