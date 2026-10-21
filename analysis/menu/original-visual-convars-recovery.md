# Original visual ConVar routines (2026-10-21)

Read-only native-byte/live-registry investigation. No production code, live feature call, index or IDA modification. Current game11188 preserved. Raw SHAe16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.

## Public current registry evidence

VEngineCvar007 factory in vstdlib.dll at734D33A0 is MOV EAX,7350D310;RET. Singleton7350D310 vtable734FEC08. Independent UnregisterConCommand implementation at734D41BE iterates this+30 linked command head and command+4 next, establishing read-only registry layout.5179 nodes traversed once with cycle bound; command+0C names hashed with original ASCII-compatible FNV1a, producing one match for each seven requested hashes. No target functions called. Complete object and current methods captured in `original-visual-convars-current-11188.json`.

| Original hash | Actual registered name | Current object | Initial value |
|---|---|---|---|
|D2AB09A1|violence_hblood|6F019620|1|
|2AFB490A|r_drawdecals|6F011C00|1|
|642F682F|cl_csm_max_shadow_dist|5E384D28|-1.0|
|7C0AE72B|cl_csm_rot_override|5E385000|0|
|E7B7BF01|cl_csm_rot_x|5E385068|50.0|
|E6B7BD6E|cl_csm_rot_y|5E3850D0|43.0|
|E5B7BBDB|cl_csm_rot_z|5E385138|0.0|

Current raw ConVar parent+1C equals actual own object for all seven captured. Current slot12 returns float via x87 and handles parent delegation; slot15 consumes one float;slot16 consumes one DWORD/int and returns RET4. Engine primary ConVar vtable6EF4C058 has12=engine6EAB3290,15=6ED6BD50,16=6ED6BE50; client primary5A2A2190 has12=59733F10,15=59F2E0E0,16=59F2E1E0. Those are current addresses only; stable adapters require module/file identity+relative methodguards. Captured integer/float storage decoded using object's XOR address independently verifies the table initial values. Do not copy raw encrypted values between objects.

## Closed native blood/decal removal roots

14B8C0..14B93F and14B940..14B9BF are each7F bytes, plainRET/no stack args. Defensible prototype `void __cdecl routine(void)` (returnvalueunused by normal frame pipeline). Each lazily caches exact hash lookup through original354170; then realECX=ConVar, EDX zero, CMP byteUI,0; SETZ DL; PUSH EDX; CALL[ConVarvtable+40]. This is original boolean inversion as native integerSetValue slot16; the decompiler's vtable+16 wording uses DWORD indexing and must not be mistaken for byteoffset10. No external Source field operands, floats, renderer, entity context or invented feature decisions appear.

14B8C0 hashD2AB09A1 resolves violence_hblood; actual original control537CF0 label`blood spray`, value537E28=control+138. Originalcache pointer7FB598,initializedflag7FB59C(bit1). It sets violence_hblood to1 when unchecked and0 when checked, every invocation. It affects that actual native blood cvar only; do not claim allblood/particleeffects disabled.

14B940 hash2AFB490A resolves r_drawdecals; actual original control537BB0 label`decals`, value537CE8=control+138. Originalcache pointer7FB590,flag7FB594(bit1). Same native inversion to r_drawdecals every invocation. The original output gates must remain intact.

Function14B8C0 hash049969ebc8cab9959d20e6b4867c1689bc3277c608a3fc38eebb337f60ccc50d.14B940 hash4b7433ad7195b7ed78150beb2d664085f2348cace721dddeb12427640273131d. Fullraw disassembly/decompilation evidence in convar-0x4414B8C0-* and convar-0x4414B940-*.

A production adapter may supply the verified actual ConVar and scope/cache init state or seed exact original cache; it must execute these original routines/verifiednativeblocks for inversion/settercalls. It cannot write its own checkbox-to-ConVar behavior. No gameplay test was performed during this investigation.

## Closed original sunset-mode behavior

14BBA0..14BE05 spans265 bytes/plainRET, no consumed incomingregister or stackparameter. IDA incorrectly guesses floatfastcall from local float temporaries; actualroot is voidcdecl(void), AL incidental currentUIbyte. SHA5f9e13d1794eb3a93c4c149bb2dcbc22d5255f30539636fca61a08ad8939d439.

This is `sunset mode`, not a generic shadow-disable routine. Original UIcontrol53A2F0 label`sunset mode`, value53A428. Actual function caches the five exact ConVars above through354170 using initbits1/2/4/8/10 in7FB5A4; pointers7FB5A0 (maxdistance),7FB5A8 (override),7FB5AC(x),7FB5B0(y),7FB5B4(z). Last-enabled byte7FB550 and savedinteger7FB554 are separate.

If actualUI equals last-enabled, original returns without writing. On transition from off to on it calls currentmaxshadowdist slot12 GetFloat, stores float inlocal, uses CVTTSS2SI and saves truncatedinteger7FB554. It then calls originalConVar15 SetValue(float800.0) on maxshadowdist; ConVar16 SetValue(int1) on rot_override; ConVar15 SetValue(float0.0) on each x/y/z. On transition off it converts savedinteger tofloat, restores maxshadowdist throughslot15 and calls rot_override16(int0); it does NOT restore old rotationXYZ values. Finally it updates original last-enabledbyte fromactualUI. These formulas/rounding/restore quirks are native behavior and must not be corrected with approximate/new logic.

The initial currentmaxdistance-1.0/currentrot50,43,0 values are observations, not substitute defaults. Capturing/restoring an adapter's unrelated safety state must not silently alter originalnativefeature decisions. Scope/installation must ensure actualcachepointers and realmethods correct beforecalling because originalnull branches can lead to invalid calls. No runtimefeatureenabledbythisresearch.
