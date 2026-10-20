# Passive Source render interfaces, PID21728

Read-only, non-atomic process observations on2026-10-20. No remote writes,
function invocation, injection, suspension or debugger changes. PID21728 was
rediscovered as the current csgo.exe before inspection.

Reusable capture: `py -3 tools/capture_source_interfaces.py 21728`.
The tool opens PROCESS_QUERY_INFORMATION|PROCESS_VM_READ only. It verifies the
observed factory wrapper/registry/singleton instruction forms before interpreting
addresses. These are version-specific observations, not a generic interface ABI.

## Verified factory chain

Each module's named CreateInterface export is55 8B EC 5D E9 rel32. Its internal
body starts55 8B EC56 8B35 absolute_head_slot, traverses12-byte records
{creator,name,next}, compares names and dispatches the creator. Relevant creators
are exactlyMOV EAX,imm32;RET, so their singleton result can be recovered without
executing them. Evidence source-factory-prefix-21728.json,
source-interface-registry-21728.json and source-interface-state-21728.json.

|Module/interface|Creator|Singleton|Vtable|
|---|---|---|---|
|vgui2/VGUI_Panel009|69939850|699808C4|6996F2D8|
|vguimatsurface/VGUI_Surface031|699ABC30|69A947D0|69A51C14|
|engine/VEngineVGui001|6C340330|6F97D178|6C561EB0|

vgui2 also registers a distinct VGUI_Surface031 at6998C310; that object must not
be confused with the vguimatsurface implementation.

## Current panel paint target

VGUI_Panel009 slot41 is69939520. Its complete31-byte implementation is:
`55 8B EC 8B01 FF7508 FF9004010000 FF7510 8BC8 FF750C 8B10 FF520C 5D C20C00`.
It obtains the panel client through IPanel virtual+104h, then passes the two
boolean stack arguments to client virtual+0Ch and returns with RET0Ch.
This matches PaintTraverse(panel,forceRepaint,allowForce) in the declared method
order, surrounded by GetName36,Think39,ApplySchemeSettings40,Repaint42.

The slot value exactly equals its rebased on-disk pointer10019520. All31 code
bytes match the installed vgui2.dll file exactly. Thus this specific current
singleton's slot/entry shows no payload redirection or inline modification.
This does not audit every panel/client vtable or downstream draw hook.
Evidence source-render-method-disk-comparison-21728.json.

Primary method-order reference:
https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/vgui/IPanel.h
The installed native method body and current factory/version were independently
checked; the public SDK alone is not proof of installed binary equivalence.

## GetPanel(PANEL_TOOLS=3), recovered without a call

VEngineVGui001 slot1=6C3412A0. Its21-byte body and slot exactly match engine.dll
on disk after base adjustment. It invokes6C341210; early-return flag6C851D88 is0.
Helper switch indexes(arg-1), entry2 selects6C341240 (`MOV EAX,[ECX+3Ch]`).
[6F97D178+3Ch]=046A16C0. That object's first virtual method6C15B360 is exactly
`8B41 4C C3`, returning[object+4Ch]. Therefore the captured GetPanel(3) result is
**02FBCCB8**. No target method was executed to derive it.
Evidence engine-vgui-panel-switch-21728.json,engine-vgui-tools-panel-21728.json.

Primary enum/interface reference:
https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/ienginevgui.h

## Payload dependency state

The same capture reads payload SourceSurface4481E15C=0,EngineVGui4481E16C=0,
original panel callback44B3B884=0,cached panel44B3B888=0. The real Source factory
objects exist while these payload dependencies remain unbound. This supports
an initialization/installation gap; it does not identify its missing caller.
The separately inspected payload callback154790 and renderer32BA40 must not be
called with guessed state to bypass that gap.
