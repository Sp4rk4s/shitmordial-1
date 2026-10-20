> Correction: follow-up render-static-findings.md establishes32BA40 as the actual frame window renderer, called from native paint callback at1550B4. Its window iteration performs update3504B0 and draw351AB0. Earlier tentative config-only categorization below is superseded for32BA40/32BC92.

# Window collection consumers, bounded static inspection

Original primordial.bin hash remains e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768. RVA addresses below use unchanged raw bytes; runtime base44340000 and IDA analysis base44000000. No complete function reversal or ABI assignment is claimed; candidates remain partial. No debugger/source/index changes.

Global runtime44B44E5C/60/64 is a vector-shaped triple of window pointers. Consumer2DEB10 loops the pointers and calls350050 with44B44E78. Pseudocode350050 handles strings/JSON values and settings including window position; this suggests serialization, not frame drawing. Runtime pointer4461EB10 occurs at raw44D39C in a repeated table of native callbacks/vtable-like groups.

Consumer2DF6D0 loads vector begin and returns its first pointed-to object if nonempty, otherwise0. Twenty-five E8/E9 byte-scanned callers include E47B2,164AF1,16588C,257BCD,257F29,2584EB,258C12,258FFB,259293,259612,25993C,259D59,25BE28,25D120,25D12C,25D746,25D75E,25D771,25D7A6,25D7BE,25D7D1,25F7F0,25F7FC,267931,267C11. Candidate caller functions257870,25D740,267870 exhibit script/binding-oriented argument processing or getters. No plaintext pointer to the getter was found.

32C130 loops windows and invokes3524D0 with width/height/force arguments;3524D0 rescales/clamps window coordinates and bounds against current screen size from35FE40. Sole direct caller of32C130 is36259D in362570. Native362570 loads prior dimension globals44B487AC/B0, stores new dimensions, and if changed calls32C130(oldwidth,oldheight,1). The decompiler omits the prior-value register distinction, so assembly is required here.

Sole direct byte-scanned caller of362570 is152EB6 in function152E30. Native152E30 preserves ECX/EDX and two explicit stack args, forwards through original pointer44B3B7B8 and ret8. With nonnull first stack struct pointer, it looks up cached interface hash528C7293 using354170, invokes its vtable+40h method with boolean global4487A2E8, calls362570(struct[0],struct[1]), and stores8 at struct+24h before forwarding. This resembles a material-system OverrideConfig hook (screen dimensions and AA-sample field), but exact interface/struct/API identity is unverified. No E8/E9 callers or runtime absolute pointers to152E30 were found. This is a useful runtime reachability breakpoint candidate, not proof it is installed.

The listed consumers32A1C4/32A38D/32A556/32A593 remain within the large construction body. Consumers32B4DC/32B4FF/32B7DB/32BC92/32C627 show configuration serialization/deserialization/rescaling loops.40CC30 is vector storage cleanup, not drawing. No render callback or initialization root was established from this collection's plaintext references.

Evidence: menu-consumer-boundaries.json,menu-consumer-candidates.json,menu-getter-callers.json,menu-hook-candidates.json,menu-screen-caller.json,menu-config-slices.txt. These contain raw candidate output including decompilation failures; do not treat failed output as function semantics.
