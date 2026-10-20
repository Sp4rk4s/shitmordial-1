# Was the missing bootstrap erased by helper717F0?

Read-only static comparison, original dump SHA256
e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768. RVAs below. No target execution or IDA/index/source edits.

## Exact effects and footprint

Verified marked_region_erase_after_use717F0 preserves both18-byte markers,
randomizes only bytes between them, and for any region length>5 writes exactly
`E9 22 66 34 00` into its first5 bytes. It does not erase/replace the markers.
The constant displacement346622h is literal in the retained assembly, not the
region length. Empty regions perform no writes; lengths1..5 omit the E9 footprint.

Full byte-granular scan finds ZERO occurrences of `E9 22 66 34 00` in the entire
original dump. Thus no captured region presently bears the required footprint
of this helper's completed erasure for length>5. Later overwrites or a different
historical helper cannot be ruled out, but neither is evidenced here.

## Exact marker comparison

Exact begin marker EB10 WLCp...WL55 occurs only once, at its reference literal
45FA48. Exact end marker EB10 WLCp...WL22 occurs at2CBA83,32A9DB,362A65,3758BF and
its reference literal45FA5C. Four near-begin markers preserve the final16 bytes
exactly, but differ in the first2 bytes:

|Begin candidate|First2 bytes|Body ends at|
|---|---|---|
|2CAB5E|C3 77|2CBA83|
|2E0509|C7 00|32A9DB|
|36298E|C3 01|362A65|
|375311|00 DA|3758BF|

Helper717F0 cannot produce these differences: its writes begin AFTER an exact
begin marker. No evidence supports labeling the native bodies as erased; retained
config, menu and localization continuations contain coherent ordinary code.
The cause of the first2-byte differences remains unresolved (protected/mixed
entry structure is a possibility, not a completed reconstruction).

Even if the menu candidate's begin marker matched, its native body length to
32A9DB exceeds the helper'sFFFF-byte end search window. The helper would not erase
that entire menu body through this matched pair. Other matched pairs or stages
are not established.

## DllMain/profile helper boundary

Native DllMain14F360 retains coherent branch logic with one process-attach call
toA1E50 and normal returns. The retained A1E50..A1FB4 profile/RNG helper has a
coherent epilogue, no exact marker pair and no fixed erasure jump footprint.
Its absence of a bootstrap call cannot be attributed to the verified717F0
transformation: that transformation would randomize a matched region>5 bytes and
leave the fixedE9 footprint, rather than surgically remove one call and preserve
surrounding flow. No claim is made that this is pristine original source code.

## Conclusion and limits

The original dump supplies NO positive footprint of completed717F0 self-erasure.
This hypothesis currently has less direct support than the unresolved protected/
indirect invocation boundary. A missing external caller is itself still an
unverified explanation. Do not infer erased bootstrap merely from entropy,
unrecognized protected bytes, changed marker prefixes or empty function xrefs.

Evidence: self-erasure-footprint-scan.json,
self-erasure-marker-byte-comparison.json,
self-marker-tail-helper-disassembly.json,
self-marker-tail-static-model.json.
