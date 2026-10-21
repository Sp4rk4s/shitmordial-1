# Original ESP recovery investigation (2026-10-21)

Read-only investigation of repository `primordial.bin`; SHA256 e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768. RVAs below use raw-file offsets; IDA base44000000, captured runtime base44340000. No production changes, new live calls, hooks, or own feature implementation were made. No claim that ESP is ready for gameplay testing.

## Exact player ESP chain

The native outer panel callback154790 calls F06C0 at154BBB and13ACC0 at154BC5. These have different roles:

- **F06C0 is player screen geometry preparation**, not a complete ESP renderer. It checks original Engine slots26 and27, resolves the local player, loops64 native records with stride95E0, computes feet/head projections through362810, derives/clips the original bounding rectangle, and marks invalid/offscreen geometry. Record array begins runtime448C4120 (RVA584120), ends44B1B920. Record+4 is the entity pointer. The loop working pointer is record+40; box writes at working+112/+116/+120/+124 therefore equal record+152/+156/+160/+164. Invalid geometry is working+68 = record+108. Original code owns these calculations; an adapter must not synthesize boxes.
- **13ACC0 is the actual player ESP dispatch root.** It gates Engine26 (IsInGame), then requires the real player-resource POINTER SLOT4481E1B0 to contain a nonzero pointer. It constructs native callback object with vtable4475E038 and calls ED330(callback,0x22). Its callback slot2 is138130, which forwards the selected native record to137310. The callback's other trivial methods are138110/138170, destructor B3100. Beware subtracting bases correctly: runtime4442D330 is RVA ED330, not12D330.
- **ED330 is the original entity/cache iterator.** Flags0x22 include dormant entities while requiring original alive filtering; it iterates using Source globals maxClients(+24), obtains EntityList slot3 entities into record+4, and invokes the callback object's virtual+8. It uses existing native cache records, not a new list format. It also supports nonplayer flags, but those paths are not fully established here.
- **137310 is substantial original player visual drawing.** It gates native per-category UI state using record+148, chooses dormant/live opacity record+100/+104, copies original cached rectangle+152/+156/+160/+164 into draw-context globals44B1CB78/7C/80/84, and handles callback bucket13 via158B70/162D60 before original visual helper calls. Its decompiler ABI has stack/frame recovery artifacts; inspect native wrapper138130 before assigning a final prototype.

Original helper calls within137310:138BA0 (offscreen/directional drawing candidate),13AE00 (model/bone or hitbox overlay candidate; not fully classified),13A9F0 (name text using369A50 then existing native font-measure/draw),13A2F0 and13A660 (resource-index based bars; exact health/armor assignment not proven),139110 (unrecovered),1388C0 (ammo/weapon-capacity bar: weapon-data maxclip+20 and original clip operand),13AFB0 (unrecovered flags/text branch),138590 (extra feature branch). These names remain research descriptions, not completed reversals. Exact enabled gates and helpers are retained in original-esp-extra-137310.json. Do not substitute helper implementations.

## Cache production precedes drawing

F06C0+13ACC0 are insufficient by themselves. Major native cache producer **F0B30** (IDA function extent F0B30..F22AE) is called by original FrameStage-shaped callback14F740 at14F916. It populates per-player category+148, origin+36/+40/+44, opacity and many weapon/animation/visibility fields used by draw helpers. This function has a151416-byte scratch stack, per-record locks, animation/history containers, Source mutation, stale direct calls and operand locations. Full code cannot safely be called after only repairing projection:

- stale direct/current-code targets2DD06CA0,15B11260,2DAD3870 and many additional native helpers;
- old operand cells2DCA59EB,2D8F1D4F (velocity),2DA8BB5F,2DCBF6E9,2D942073,2D8F6F39,2D90655C,2D8F6DBC;
- fixed entity field+103C0 (decompiler v12[16624]) participates in cache identity/reset;
- real Globals4481E164, EntityList4481E170, Engine4481E174, resource-slot4481E1B0, current context/local state, model/trace dependencies and native synchronization/container state.

A second original cache updater **F03E0** (F03E0..F06B1) is called from original CreateMove-shaped callback3798C0 at379A71; it handles current entity assignment and dormant position/opacity state. F0AD0, called from377A00 at377B54, constructs another callback and runs ED330(flags0); do not mistake it for the paint draw root.

Retained decompilations are potentially incomplete due protected flow and stale IDB boundaries. In particular130B50 is only a native two-checkbox gate/tail jump into12D440; IDA merges the target into a848-line decompilation.130460 lacks a valid IDA function definition, but raw bytes prove it iterates entity indices65..EntityList highest index (slot6), builds/sorts world-entity entries, and draws them via native projection/text helpers; this is dropped weapon/projectile/world ESP candidate, separate from player ESP.

## Smallest supported restoration path

1. Recover current Source DebugOverlay004 slot13 for original362810 projection, with real Source screen dimensions/font bindings already used by the working menu. Verify unchanged native projection in isolated tests and correct present-game interface identity. This is a real shared prerequisite, not ESP success.
2. Recover an original cache-production path sufficient for live active-player records. F0B30 must be dependency-audited and its original container initialization/field operands/callees bound, or a **verified exact original sub-block** may be isolated if its entry/live-ins/output closure are proven. Filling cache fields using new agent-written feature decisions would violate AGENTS.md.
3. Preserve native F06C0 bounding-box math and13ACC0 -> ED330 ->138130 ->137310 drawing. Restore only proven interface/operand/ABI adapters; run under the existing valid Surface paint context, retain original callback/material/scripting limits.
4. Recover remaining visual helpers and world-object root separately. Do not enable full original154790 or entire14F740 simply because these roots are identified. They contain unrelated unsupported startup/feature calls.

No shared index or live IDA names/comments were changed by this delegated read-only investigation, as assigned. Parent should retain completed functions only after ABI/control/dependency verification; these roots are currently partial.

## Bounded independent visual candidate: native spread-circle B5680

B5680..B59E8 is a plain native no-stack-argument function with balanced EBP frame and RET. Unlike player ESP, it does not consume95E0 player caches. Native gate4487AD28 enables its primary spread/inaccuracy circle; gate44821540 enables a second category-scaled circle. Dependencies:

- original already-recovered A06C0 and369370 obtain current local/activeweapon;
- weapon virtual slots **453** and **483** return x87 float values. Raw XOR constants decode exactly453 and483. Native routine returns if453 result equals native zero;483 provides screen-scaled radius. Their **current-game semantic slots still need independent verification**, not blind use of oldindices;
- original35FE40 supplies dimensions; optional camera4481E1E0+176 scales radius by nativeconstant; optional means native itself checks null;
- 200 original static sample records,24bytes each, at runtime448BEFE8..448C02A8. Consumer working ESI starts448BEFF4 (record+12), increments24, stops448C02B4. Original binary contains populated floatdata, not a heap pointer. IDA incorrectly decompiles it as dword_448BEE5C+408; raw machine code uses literal448BEFF4. Retaining original bytes is allowed; generating replacement random samples would violate the feature rule;
- native math CRTfloor3CA7B5 and existing original filled-rectangle helper361090. Native routine owns every radius/sample/position/alpha calculation;
- primary RGB4487AF88, secondaryRGB448217A0; alpha is native sample-derived;
- second branch additionally calls103F60 to select UI category and reads float448269F0+category*616. It remains unresolved until original categoryhelper dependencies are proven.

This is the smallest identified independently bounded visual feature candidate. It still needs currentweapon-method proof and originalsampletable startup preservation before a production adapter. Do not interpret this investigation as a completed feature.

Other compact root10BE80 tails into10C100, which draws manual anti-aim direction arrows and depends native AA choice state, native scopedoperand2DBDB5D3 and triangular renderer362350; it is not a scope crosshair. F6730 updates a clantag through captured engine function, so enabling it would not recover drawing. B1250 contains grenade-prediction/trace-shaped dependencies;13EAD0 draws existing grenade trajectory48byte records, and cannot invent their producer.
