# Render callback/installer investigation (2026-10-20)

Static original-dump inspection. SHA256e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
RVA values; runtime44340000, IDA44000000. No live execution, production change or
new complete function reversal. IDA decompilation batch timed out; raw dumpbin
instructions were used, never synthetic carrier execution.

## Corrected D3D candidate exclusion

155F40 initially looked like Present because its tail RET14h cleans five stack
DWORDs and forwards through44B3B8F8. Actual entry bytes contradict a standard
D3D device-on-stack interpretation:155F49 stores incoming EDX;155F4C copies ECX
to EDI;155F64 reads object+64h,155F67 reads+5Ch,155F6F reads+70h. The first stack
argument is traversed through+48h/+88h/+134h and a nested name is hashed before
original forwarding at1561FC. This is a fastcall object/entity/model-shaped
callback, not established as D3D Present. No precise Source API label was assigned.
The similarly shaped155C30 returns18h and forwards44B3B8FC; it remains a Source
object callback candidate. Stack-cleanup size alone cannot identify rendering API.
Evidence render-present-candidate-disassembly.txt.

155B10 forwards44B3B8D8 then calls virtual methods+48h/+4Ch through Source engine
interface4481E174 using a3-float local vector.155BD0 preserves ECX entity-shaped
input and copies a3-DWORD vector result from36A7C0 to its stack argument. Neither
supplies a verified D3D EndScene/Present target.

## Installer dependency boundary

Every literal aligned DWORD address in44B3B600..44B3BAF0 was searched across native
text1000..40D000. Among selected original callback slots:

|Runtime slot|Sole native literal reference|
|---|---|
|44B3B884|1550F2, FF15 operand in panel-render candidate154790's continuation|
|44B3B8F8|1561FE, FF15 operand inside155F40|
|44B3B8FC|155EF1, FF15 operand inside155C30|
|44B3B8D8|155B1B, FF15 operand inside155B10|
|44B3B8E4|155BE0, FF15 operand inside155BD0|

No native literal writer or PUSH output-slot descriptor references these selected
original pointers. Their captured addresses alone do not identify an installer.
Computed/protected dispatch, encoded descriptors and external setup remain
outside this negative scan. No guessed live function call is justified.
Evidence render-original-slot-native-references.json.

The parallel Source-panel path investigation owns callback154790, whose interface
4481E16C and SourceSurface4481E15C dependencies are a better current lead to menu
drawing than the unverified D3D cleanup-size candidates above. No hook installation
path has yet been established by this static scope.
