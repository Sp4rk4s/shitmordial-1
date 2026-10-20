# Reconstructed CPU guard contract (offline only)

2026-10-20. This extends `protected-entry-runtime-state-findings.md`; its earlier
unknown field offsets/salts are now recovered for the CPU guard. It does not
establish the rest of GlobalData or the actual game-bootstrap caller.

Evidence: `protected-guard-models-21728.json`, generated reproducibly by:

```
py -3 analysis/menu/protected_guard_models.py --image analysis/menu/runtime-21728-prepared-image.bin --pid 21728 --teb 0x699000 --output analysis/menu/protected-guard-models-21728.json
```

The script emulates copied image bytes, uses actual target TEB/PEB read-only,
and deliberately adds a **synthetic page only inside Unicorn** at the captured
loader pointer. No game memory, threads or debugger state is changed. No target
function is called. General registers/arguments/stack are synthetic. This is a
controlled binary-analysis experiment, not a production repair or menu test.

## Recovered fields and predicate

Runtime pointer slot44C5BC4B (RVA91BC4B; IDA4491BC4B) contains1C85CEC8.
The protected prefixes implement the following shared contract:

```
P = DWORD[image + 0x91BC4B]
count = DWORD[P + 0xB8] XOR 0x59DB1541

(a, b, c, d) = CPUID(1)
if ((a & 0xFF0) == 0xFE0):
    a ^= 0x20
expected = uint32(a + (b & 0x00FFFFFF) + (PEB.OSBuildNumber << 7)) XOR P

For positive count, inspect hash entries starting at P+0x20, stride4:
    decoded_hash = DWORD[P + 0x20 + 4*i] XOR 0x40DF21BC
    if decoded_hash == expected:
        restore saved registers/flags and continue native function
    decrement remaining count and continue if nonzero

If no entry matches:
    restore saved registers/flags
    EAX = 0xDEADC0DE
    transfer control to address0 (PUSH0/RET source sequence)
```

The zero/negative decoded-count case was not exhaustively emulated; the emitted
source uses a do-while-shaped loop, so do not assume zero is a valid empty table.

These constants were derived from VM stack dataflow, not guessed from source:
with encoded-countzero, the interpreter computes59DB1541; changing it to
59DB1540 yields one iteration. A zero hash entry decodes to40DF21BC; changing
the encoded hash to expected XOR40DF21BC changes the guard to the success path.
The local VMP source `core/intel.cc:15824..15940` independently matches this
CPU-check generation, and `runtime/common.h:253..287` explains the salted
GlobalData fields. The physical pointer affects the expected hash; copying a
table to a different allocation without recomputing its hash would be wrong.

For this emulator CPU, CPUID1 returnedEAX306C4, EBX800. Actual target PEB build
is4A65. Expectedhash1CAD8F8C; encoded entry5C72AE30. These are **emulator model
values** and must never be copied into the live process as a purported repair.

## Controlled outcomes

Six worker-prefix cases establish equality and table-count bounds:

| Synthetic table | Decoded count | Outcome |
|---|---|---|
|First hash wrong|1|EAXDEADC0DE, EIP0|
|First hash matches|1|Native worker marker3629A0|
|Only second hash matches|1|EAXDEADC0DE, EIP0; second entry not read|
|Only second hash matches|2|Native worker marker3629A0; reads+20 then+24|
|Both hashes wrong|2|EAXDEADC0DE, EIP0|
|First encoded hash differs by one bit|1|EAXDEADC0DE, EIP0|

Additional controlled CPUID output models verify the arithmetic and Athlon quirk:

- Changing EBX800 toFF000800 preserves success with the same table (high byte
  masked).
- Changing EAX/EBX to906EA/7F123456 passes with the recomputed source-formula
  table.
- EAXFE0/EBX55000700 passes when the formula applies XOR20 toEAX; the otherwise
  identical table calculated without the quirk failsDEADC0DE/EIP0.

All four roots pass with the recovered shared fields and reach their exact
native self-marker PUSH boundaries:

| Root | Native boundary | Offline instructions |
|---|---|---|
|2CAB40 config candidate|2CAB70|80156|
|2E04D0 menu candidate|2E051B|97540|
|362970 worker candidate|3629A0|64674|
|3752F0 localization candidate|375323|62493|

The emulated stack includes sufficient downward space for the menu's large
stack probe. It does not read target pages to supply synthetic stack storage.

## Bounded continuation results

Continuing the same offline model until its first external code boundary:

- Config and menu prefixes reach current bound malloc75830130. Their identities
  were checked against the prepared339-record import destinations and keys.
- Worker reaches current MSVCP140!_Thrd_hardware_concurrency5631D240 by the same
  import comparison.
- Localization enters its native continuation and then encounters an interface
  or synthetic-call-state gap, ultimately transferring to1. This is not evidence
  of a target runtime crash or a supported localization-call ABI.

Only GlobalData+B8 and+20 are read before those boundaries. Further allocation,
threading, nested protected calls or runtime-integrity consumers were not
emulated. No complete native function was reversed here; this is a reconstructed
protected prefix/data contract. Live IDA annotations remain the main agent's
responsibility, as requested.

Earlier trial JSONs `protected-worker-synthetic-guard-first.json` and
`protected-worker-synthetic-guard-zero.json` are exploratory traces; the `first`
trial used a rejected salt guess. Prefer the complete model matrix above for
the verified contract.
