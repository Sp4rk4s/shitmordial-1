# Original third-person and free-camera recovery (2026-10-21)

Read-only native primordial.bin and current process11188 inspection. No game calls/writes, production changes, shared index edits or IDA annotations. Native RVAs use file offsets, IDA base44000000/runtime44340000. Existing player/menu/particle game preserved.

## Key distinction: F7050 is free-camera state

F7050 is exactly MOV AL,[runtime44B1BBA0];RET. This state is written by native F7780, called from CreateMove-shaped3798C0 at379EE1. Its actual original label is `freecam key`, UI object runtime4488A800/RVA54A800; current11188 encrypted UTF16 string at34BE3780 decodes with key13.

F7780 activates from native keybind virtual8, captures current CViewSetup origin+B8/BC/C0 or original36A7C0 eye-position helper, copies local velocity via old field operand2D8F1D4F, preserves original command viewangles+0C/+10/+14, and integrates freecam velocity via F7060. It zeroes command buttons+30, impulse+38 and movement+24/+28/+2C and calls F4FE0. Deactivation restores viewangles and calls Engine19 SetViewAngles. Therefore forcing7DBBA0 is not a safe thirdperson fix: it is a separate input-affecting feature.

F79B0 (71bytes) is called after the original OverrideView inside14FF30 at15013E. When freecam active and local alive it copies original freecam position7DBB94/98/9C into actual view origin+B8/BC/C0. F7060 itself reads hashed movement ConVars, command buttons/movement, Source Globals tickinterval+20 and local movement-scale operand2DACD7B6; it owns full movement integration/drag. It must not be replaced by an agent-written fly-camera algorithm.

14F340's predicate is **freecam-active OR thirdperson keybind-active**, not a direct read of current CInput camera flag. The original keybind is runtime448750E8/RVA5350E8; method8 dispatch is native. This predicate is used by chams/glow/manual player draws too.

## Actual original thirdperson callback

The native thirdperson feature root is **378FC0..379821** (entry bytes538BDC83EC0883E4F083C404...). IDA currently has no valid function definition there. It preserves incoming ECX receiver and EDX, takes no stack arguments, and returns plainRET. Its original target pointer is stored at runtime44B65FDC/RVA825FDC. Three native calls through that pointer occur3793C1/3793E9/379803. This is a CAM_Think-shaped hook, separate from OverrideView14FF30.

Original control flow:

1. Require original A06C0 local entity,36ABF0 alive and Engine26 IsInGame.
2. Resolve active weapon369370; read original scoped operand2DBDB5D3/item2DA842AE and exclude item39/8 while scoped.
3. Permit F7050 freecam or original keybind5350E8 nativevirtual8. Disabled paths clear old CInput+AD, reset persistent collision fraction4DB478 to1, and call original CAM_Think.
4. Enabled path sets old CInput+AD, temporarily changes a captured external pointer at old32BEFD98, sets native camera ConVars, temporarily adjusts local view-offset Z via original110FD0 and oldoperand2D8FBEC0 around the original CAM_Think call, then restores the pointer/offset.
5. Uses current native CInput camera angles/distance, local eye origin, original scalar sin/cos imports and a hull TraceRay on real EngineTrace interface4DE180 virtual5 with mask0200400B. Original code builds ray/filter using native vtable4475D4F0 and stack objects. Fraction smoothing uses4DB478 and Globals frametime+14, then reduces camera distance. Preserve all original collision decisions and math.

Exact current Source field compatibility difference is4 bytes: old CInput thirdpersonAD -> currentA9; old anglesB0/B4 -> currentAC/B0; old distanceB8 -> currentB4. This follows independently verified current CInput methods below, not assumed broad structure translation. Fix only these exact consumed operands after whole callback dependencies/ABI are proven.

Native UI labels read from11188 control objects:

| Original control RVA | Actual decoded label | Storage/value |
| --- | --- | --- |
|5350E8|enable key|native keybind virtual8|
|535260|distance|float535390|
|5354C8|transparency when scoped|float5355F8|
|54A800|freecam key|native keybind virtual8|

The runtime keybind text object uses encoded UTF16 pointer+0C, codeunit length+1C, XOR byte+24. Observed names above are exact decoded native text, not new labels.

## Independent current CInput evidence

Current client base59590000, CInput objectclient+52627B0 =5E7F27B0; factory/VClient018 slot16 getter evidence independently supplied by parent. Current vtable5A1A4CEC. Captured methods in thirdperson-current-input-methods.json:

-31 client+2F54E0: CAM_Think candidate; reads camera thirdpersonA9 and actual original named cam_idealpitch object5E7CC2A0, cam_idealyaw5E7CC238,cam_idealdist5E7CC308. These native ConVar object references independently link camera role.
-32 client+2F8940 readsA9 (IsThirdPerson-style method with split-screen stackargument).
-34 client+2F8990 copiesAC/B0/B4 into supplied vec3 (camera offset getter).
-36 client+2F85E0 clearsA9;38 client+2F52B0 setsA9; both update camera mode state.

Current method31's exact full function boundary/ABI and actual hook callsite remain incomplete; do not install solely because its vtable slot matches a reference name. Native current method disassembly retained thirdperson-current-camera-think.txt.

## Original named camera ConVars

Read-only public VEngineCvar007 list traversal of actual11188 objects independently matched original FNV constants. Full evidence thirdperson-convar-hashes.json:

-48117B24 cam_idealpitch, original sets int0.
-95BDD279 cam_idealyaw, original sets int0.
-C554050A cam_idealdist, original sets float(distance535390 + nativeconstant44754BEC).
-61D1278E cam_ideallag, original sets int0.
-EC70F5E8 cam_snapto, original sets int1.
-EB1AB238 c_maxdistance, original sets int230.
-EEFCD373 cam_collision, original sets int0 (original callback does own hull collision).

Original lookup354170 hash5BC570FD has no match among actual5179 public registered command names. It is used to replace captured pointer32BEFD98; role unresolved. Do not substitute sv_cheats or a fake object: their FNV hashes do not match. Current address/semantics of that pointer is a concrete blocker.

##134B50 is manual model drawing, not camera enable logic

134B50 obtains MaterialSystem render context slot115, acquires it, sets render-state color, then enters native model phase7DC939=1 at134C00.134C00 is interior with EBP/stack/register live-ins, not a callable entry. It uses old CInput B0/B4/B8 camera offsets to construct camera ray, EngineTrace5 and an ED330 entity iterator callback, sets original model fade7DC940=1, optionally draws local player Renderable9 using14F340 gates, then clears phase at134FD2 and releases state/context. Entire function also calls captured private Source target2DC6C5F0 via4DE200; it is not safe to enter from only camera offset repair.

## Remaining exact blockers / viable next recovery

- Callback378FC0 and134B50 need bounded entry/control-flow/SEH destructor coverage. The former has protected exceptional allocator path near3797C6; latter has similar protected release-failure helpers.
- Current CameraThink31 full ABI/entry identity and saved-original publication must be corroborated before hooks.
- Original UI keybind evaluation requires native gameplay input dispatcher/state, not just menu click responsiveness.
- Original local/observer/alive/weapon helpers can reuse verified original capsules. Read current scoped9974/item2FBA and viewoffset fields from real RecvTables. No guessed oldfield dereferences.
- Repair real CInput camera fields and bind real EngineTrace4DE180, Globals4DE164, current input4DE1A8 and camera view4DE1E0. Actual tracefilter4475D4F0 virtualmethod dependency must be audited.
- Recover original110FD0 camera-height logic and its input state; do not replace its result with arbitrary camera height.
- Resolve old external pointer32BEFD98/hash5BC570FD; no stale rebase or invented replacement.
- Freecam F7780/F7060 depends full original current-command lifecycle and movement ConVars; scope it separately from thirdperson.

No thirdperson/freecam feature is game-ready from these findings. Native logic is located and current camera-layout changes are verified; enabling only the keybind or camera flag would bypass unresolved original behavior.
