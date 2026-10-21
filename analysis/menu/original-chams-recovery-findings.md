# Original chams and particle hook recovery (2026-10-21)

Original-byte investigation of `primordial.bin`; this delegated work made no live game hook or production change. Closed functions were subsequently retained in live IDA as described below. Raw RVA equals file offset. IDA base 44000000; runtime operand base 44340000. The four fully closed functions now have logical names, verified prototypes and comments read back/indexed in live IDA; partial chams candidates retain uncertainty. Several IDA function boundaries/decompiler ABIs are wrong, so raw instruction evidence controls.

## The two first installer callbacks are verified particle color initializers

155F40..156242 and 155C30..155F3A are almost identical wrappers. They save ECX-object RGB vectors at +5C/+60/+64 and +68/+6C/+70, traverse first stack argument +88 chains, read definition +48 -> string +134, hash the name using FNV constants811C9DC5/01000193 against12/7/2 hashes, and conditionally replace both vectors before calling saved originals7FB8F8/7FB8FC. Afterward they restore every saved component. The first accepts five stack arguments/RET14; second six/RET18. EDX is saved and forwarded. These are NOT established Direct3D Present hooks or player chams. Their paired sibling153E70 calls original7FB824 and accesses particle-style four-wide attribute storage +F8/+FC, strides+158/+15C and particle count+30 using identical hashes/gates. Final current-client RTTI evidence below establishes C_INIT_RandomColor slots29/30 independently.

Runtime control cells448790C8 (byte),44879328..2A RGB,44877420 (integer mode),448776A8..AA RGB; these are runtime VAs, corresponding original RVAs5390C8,539328,537420,5376A8. No matched plain-text particle names were found in this dump. Do not substitute invented categories or a guessed name list.

## Actual native chams hook chain

152EF0 is a different model hook, NOT one of the24 requests already inventoried from1564B0. Raw bytes prove ECX receiver, preserved EDX and four stack parameters (RET10). It examines third argument +30 flags and +34 entity index, uses EntityList4DE170 slot3, compares network class IDs, conditionally forwards through7DC950, or calls133470. It then clears forced material through4DE1A0 slot1. This matches IVModelRender DrawModelExecute-style ABI; exact current VEngineModel016 slot21 target must still be independently verified. Do not infer installation or current identity from this signature alone.

Saved original7DC950 is a data pointer. IDA currently falsely describes runtime44B1C950 as a function because dumped mutable bytes were analyzed as instructions; native calls are FF15 indirect calls. Never jump directly to the slot bytes.

133470..133C84 is the full original draw dispatcher. IDA falsely splits an interior at133719; **133719 is not a callable entry**. Entry sets deferred-material vector state (runtime447F3C00/04/08), obtains script callback list158B70(18), dispatches original script callbacks, then at1339CD performs entity/class routing. It calls132FF0 (player passes) or133C90 (other model branches). Native player selection/classification and draw decisions should be retained; binding a model interface alone does not close this dispatcher.

## Native material and pass behavior

134810..1348E5 is a bounded no-argument material initializer, called from nativeA287E. Its two original XOR-decoded names are exactly `debug/debugambientcube` and `debug/debugdrawflat`. Each uses MaterialSystem4DE19C slot84 FindMaterial(name,NULL,false,NULL), stores results7DC930/7DC934, then calls returned material slot12 AddRef. These are existing native materials, so preserving this initializer avoids inventing shader definitions or replacement chams assets.

132FF0..133467 is the bounded original player material-pass routine (four stack args despite IDA presenting only one). It reads current player7DC93C, original local resolverA06C0, team/observer helper36ACC0, and native thirdperson predicate14F340. The original category chooses configuration controls with native stride15C. It chooses one of7DC930/934; calls IMaterial slot29 SetMaterialVarFlag with flag8000 (ignore-depth); applies forced material4DE1A0 slot1; applies RenderView4DE1AC slot4 blend and slot6 color; calls original7DC950 for hidden/visible passes; finally restores blend1, color1/1/1 and clears forced override. Original bytecode determines colors, alpha, gates, pass count and category. No external reimplementation is needed or permitted.

133C90..134806 contains additional branches (player/model/weapon-like/native history), native math and helper calls; it is not a safe substitute entry for all models. It includes old scoped operand2DBDB5D3, additional oldfield2D8F1D4F, original ED5F0/EFA40 helpers, thirdperson14F340 and36DF40 dependencies.

## Exact outstanding prerequisites

- Full133470 script dispatch depends on original158B70/162D60/Lua binding routines and mutable script/context/vector ownership. Script callbacks cannot be faked by production fixtures.
- Dispatcher1339DD..133B30 depends on current class IDs7DC94C and57ED7C, owner-handle operand cell2D8F629D, original resource POINTER SLOT4DE1B0, resource alive operand cell2D908E0C, resource-offset resolver37B6E0(hash40A2AF50) and cache7DB920/924. Current native FOV resource/alive evidence is useful but not complete coverage.
- Its player-vs-model gate7DC939 is written1 around native134C00 and cleared at134FD2; this corresponds to an original outer manual-player draw phase, not an arbitrary enable flag.7DC938 (read by152EF0) is set by134980 and cleared by134970, tied to native glow processing. Do not force these flags to obtain a desired branch.
-132FF0 thirdperson14F340 reads originalF7050 and a menu keybind vmethod; current menu readiness does not establish gameplay keybind state.
-36ACC0 uses original local/alive helpers and observer operands2DA9DC0C/2DAF0360, then36B2B0.36B2B0 compares actual entity indices/teams, resource fallback2DB22972, team2D92D648, survivalteam2DB5D56F and mode globals4DE310/314/318. These require current verified fields/context; C++ team comparisons would violate the original-feature rule.
- Full model callback installation target and original draw context must be corroborated in the current Source build. No stale encoded target rebase is valid evidence.
- Raw-byte snapshots of the bounded initializer and132FF0, including correct calling conventions and current method guards, can support meaningful isolated native tests first. A native player capsule can be prepared only after its full original gates/dependencies are repaired; no live replacement classifier or renderer.

## Recommended bounded next step

Independently identify current VEngineModel016 DrawModelExecute and ForcedMaterialOverride, RenderView blend/color, IMaterial29/12; then validate original134810 in an isolated native harness, retaining actual names/call ordering and failure handling. Recover36ACC0/36B2B0 and original dispatcher/player phase before connecting132FF0 to live draw callbacks. This is recovery progress, not a gameplay-ready chams build.

Evidence JSON files `chams-0x44155F40-*`, `chams-0x44155C30-*`, `chams-0x44132FF0-*`, `chams-0x44133470-*`, `chams-0x44133C90-*`, `chams-0x44134810-*`, `chams-0x4436ACC0-*`, `chams-0x4436B2B0-*` contain original read-only IDA output. Raw disassembly also explicitly resolves153000 continuation across inline data. Subsequent retention applied/read back four completed function annotations and one partial model-hook uncertainty note; see receipt below.


## Closed first effect: original particle fire and smoke tint

Read-only current process3984 (discovered by name) provided client.dll path/hash183d42fac8a4ecc24dc02edf5f09122f37fe9e7533ca093a867718b25cfdeb03. Disk PE RTTI provides independent concrete identity: COL pointer atclientRVA D0910C -> COL D8D934 -> type descriptor `.?AVC_INIT_RandomColor@@`. Its vtable startsD09110; slots29/30 contain client+9597A0/+959B30. The original callbacks therefore wrap this random-color particle initializer's single/block entrypoints, with literal min/max color vectors matching in both original callbacks and current client code. Native current second method reads min/max +5C..70 at959B7D..959BA9; first method reads parent collection+88 and current count/attribute layout agrees. This is particle initialization, NOT wallhack model drawing.

Exact minimal payload ABI (opaque stack argument names until full Source parameter semantics verified):

```
int __fastcall original_particle_color_single(void *initializer_ecx,
    void *preserved_edx, void *collection, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4); //155F40 RET14
int __fastcall original_particle_color_block(void *initializer_ecx,
    void *preserved_edx, void *collection, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5); //155C30 RET18
unsigned __cdecl original_fnv1a_signed_cstr(const char *text); //6CF70 RET
```

155C30 is exactly30A bytes through RET18 at155F37..39.155F40 is307 bytes through RET14 at156244..46.6CF70 is39 bytes through6CFA8 RET.6CF70 has no calls or global dependencies: it scans NUL length, FNV811C9DC5/01000193 and **MOVSX signed-byte**.155C30 rel32 helper calls155D7C and155E45; saved original FF15 operand7FB8FC at155EEF.155F40 helper calls15608C/156155; saved original7FB8F8 FF15 at1561FC. The complete callbacks make no other external calls and contain no old absolute Source field cells. UI colors/gates/constants are their only other absolute dependencies. Original helpers must remain native; no replacement feature decision code.

Collection+48 points to particle definition; definition+134 points to NUL name. Collection+88 follows parent to final root (original loop includes all parents); collection+30 is signed active count. For root matching fire set12, hash original child name against excluded smoke-child set7. Only fire-root/noexcludedchild plus byte5390C8 enabled tints. Otherwise match final-root name against smoke2 set and mode537420>0. Both routines repeat the same operator-vector writes count times and restore vector storage after saved original returns. Native effects on collection output are performed by actual current original game initializer; an isolated trampoline fixture can observe vectors before restoration but is NOT a live feature substitute.

Runtime3984 current labels were read as original wstrings:44878F9C XOR60 =>`modulate fire`;44878FB8 XOR23 =>`modulate fire`;448772FC XOR4C =>`smoke`;44877318 XOR91 =>`smoke`. Values lie at5390C8 /537420; RGB at539328 /5376A8 (RVA). Do not label smoke mode as arbitrary ESP/chams.

All21 FNV hash constants matched exactly to names recovered read-only from installed pak01 VPK PCFs. Full map in `original-particle-name-hashes.json`: fire root example8B2AD371=`molotov_groundfire`; fire excludedchild exampleE12092D4=`explosion_molotov_air_smoke`; smokeEF64C394=`explosion_smokegrenade`,3CC26F1F=`explosion_smokegrenade_fallback`. PCF sources are particles/inferno_fx.pcf,particles/weapons/cs_weapon_fx.pcf,particles/explosions_fx.pcf. These provide real asset names for isolated native dependency tests rather than fabricated category mapping.

This closed pair is a defensible first original visual effect once installer guard/profile/tests are implemented; the full native chams chain above remains separately unresolved. No live hook was installed by this investigation.


## Live IDA retention receipt

`tools/retain_original_particle_findings.py` verified primordial.bin SHA/base against active IDA metadata, preserved meaningful existing names/comments, and applied/read back/indexed:

-155F40 `original_particle_random_color_single`, fullfastcall prototype and original gates/dependencies comment.
-155C30 `original_particle_random_color_block`, fullfastcall prototype and original gates/dependencies comment.
-6CF70 `hash_fnv1a_signed_cstr`, unsigned result/cdecl(constchar*) and signed-byte warning.
-134810 `original_chams_cache_builtin_materials`, voidcdecl(void), exact native material names/methods and unresolved full-chams limit.

152EF0 received a PARTIAL uncertainty instruction note without changing its existing name/prototype. No false133719 function annotation was added. `analysis/ida/project-index.json` contains retained functions/data note. Detailed readback receipt `analysis/menu/original-particle-ida-applied.json` has database_disk_save_verified=false: latest disk save is unverified. Comments accurately say native tests/production adapter under preparation and do not claim a passed test or actual feature success.
