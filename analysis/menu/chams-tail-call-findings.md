# Native chams SceneEnd tail call: unresolved identity (2026-10-21)

This is read-only original-byte investigation; no new live hooks, source changes, shared index changes, or IDA edits. Existing game11188 is preserved. Original fire/smoke effects are separately user-confirmed; that does not establish chams readiness.

The11-byte native SceneEnd adapter154690 is CALL DWORD PTR[runtime44B3B854] followed by JMP134B50.134B50 is the actual aligned-stack function entry;134C00 is an interior instruction. The full body ends13506F RET (size520 hex). IDA currently has no reliable contiguous definition; do not call134C00 or invent a133719 entry.

At tail13501D..13502E original instructions are:

```
13501D MOV EAX,DCEF2193
135023 XOR EAX,F129E463  ; decoded 2DC6C5F0
135028 MOV ECX,[4481E200] ; payload context RVA4DE200
13502E CALL EAX
```

No stack arguments are pushed for this call, and its return value is unused. Exact original ECX object type and native callee identity remain UNKNOWN. Oldclientbase2D8F0000 implies callee offset37C5F0, but that arithmetic is NOT current-game target proof. Captured original context pointer32C0E788 is not a usable current pointer. No visible direct writer to4DE200 has been established, and blind ASLR rebasing is invalid. Raw image literal search finds the context operand at13502A only; old target is XOR-encoded, not a direct literal. Searching local reference source and retained reports did not provide an independent callee signature or object identity. No replacement/no-op was introduced.

The call executes while native134B50 still holds real MaterialSystem115 GetRenderContext; afterward it invokes contextslot3 EndRender and slot1 release, then unwinds. That sequencing constrains the callee environment but does not identify its behavior. Names such as glow/shadows/beams/leaf system are unsupported guesses and should not be used as binding evidence.

Known surrounding original prerequisites, independently corroborated by parent current-client captures:134B50 uses MaterialSystem115, context2/3 Begin/EndRender andslot1 release, EngineTrace4DE180slot5 with native trace/filter, originalCInput4DE1A8 camera offsets oldB0/B4/B8 (currentAC/B0/B4), oldview-offset operand2D8FBEC0, local/alive/immunity/thirdperson helper chain, native entity iterationED330(flags4), and local IClientRenderable+4 slot9 DrawModel when original gates permit. Native phase7DC939 is set at134C00 and cleared134FD2; this manual entity draw phase makes inner model dispatcher choose original player passes. It is not an arbitrary enable flag.

## Full model dispatcher script phase is naturally empty

Read-only `chams-native-dispatcher-live-readback.json` captures all19 original script callback vectors7FB9E8+12*i as(0,0,0), including index18 at7FBAC0. Native script registry7FBAE8 is also(0,0,0). No game state was forced zero. Native158B70 is a bounded21-byte cdecl getter: validate index<19, return7FB9E8+12*index. It performs no allocation, guard reconstruction, module lookup or Lua execution. Native133470 calls it with18, compares actual begin/end and jumps1339CD directly when equal. Thus162D60/Lua/native script conversion code is genuinely skipped by the original branch in this actual state.

`probe_chams_empty_scripts.py` reads those actual states and replays intact133470 OFFLINE with explicit emulator-only Source interface fixtures. It returns balanced after72 instructions, executing actual158B70 and never162D60. `chams-empty-script-native-replay.json` records this result. This proves the empty-registry native path, not real model rendering or field correctness. No test fixture is a live replacement.

Ordinary hook152EF0 checks getter132E40 =7DC938 (glow phase). With actual0 it enters133470. Inner133B30 checks7DC939 (manual player phase). Actual0 routes ordinary models through saved original7DC950 rather than native player passes. Neither phase may be forced to obtain chams.

## What remains required

Recover old tail2DC6C5F0/object4DE200 identity using a captured old-client image/actual old function evidence or independent current function/class evidence. Then establish a unique current equivalent with verified ABI/call order. In parallel retain current verified model-render interface guards and operand adapters for the original dispatcher, class/resource/observer/team helpers. Until that tail and surrounding original root dependencies are closed, full134B50 SceneEnd restoration is not ready for a live gameplay test.
