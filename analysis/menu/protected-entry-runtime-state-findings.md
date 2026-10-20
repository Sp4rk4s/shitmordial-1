# Protected entry prefixes require retained VMP runtime state

2026-10-20. Original dump SHA256
e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
Prepared PID21728 image SHA256
cb34002af042956297e3101e2027be8bc319084c1bf889404277676e1d9aeaa0.
All addresses below are RVAs unless explicitly called runtime VAs; runtime base
44340000, IDA analysis base44000000.

## Concrete evidence

`protected_entry_probe.py` emulates only copied bytes in Unicorn x86. It opens
PID21728 read-only, copies actual TEB699000 into the emulator's FS-base-zero
page, and lazily copies external readable target pages. It never executes code
in the game, changes its memory, suspends it or changes debugger state. External
code fetch stops; unavailable pages are not synthesized. General registers,
arguments and stack are synthetic, and Unicorn supplies its own CPU model.
These are bounded prefix observations, not a verified function ABI or evidence
that actual game startup entered these roots.

Run used:

```
py -3 analysis/menu/protected_entry_probe.py --image analysis/menu/runtime-21728-prepared-image.bin --pid 21728 --teb 0x699000 --output analysis/menu/protected-entry-offline-21728.json
```

All four roots execute CPUID leaf1, read actual FS:[30] -> PEB696000, read
PEB+AC (OSBuildNumber), load the same image DWORD at runtime44C5BC4B
(RVA91BC4B), then attempt to dereference that DWORD+B8 before their native
self-marker continuation:

| Root | CPUID RVA | Image DWORD load instruction RVA | External dereference instruction RVA | Offline instructions |
|---|---|---|---|---|
|2CAB40 config candidate|90FC41|943A11|993518|36951|
|2E04D0 menu candidate|10414AE|9A2965|89C9FE|39653|
|362970 worker candidate|8E8C17|988337|FDE535|30651|
|3752F0 localization candidate|84EB59|103631E|94E7BE|26123|

Image DWORD91BC4B contains1C85CEC8 in both original and prepared images.
AddingB8 yields1C85CF80. Reading target page1C85C000 fails Win32 error299;
the emulation stops at the listed dereference. This value was **not** inferred
from a random register or missing PEB. Its direct image load and intermediate
VM stack writes are retained in the JSON. None of the339 recovered VMP import
destinations lies within100 bytes of91BC4B.

For the worker probe, the decisive sequence is:

- Interpreter instruction988337 reads [44C5BC4B] =1C85CEC8.
- Instruction988339 writes1C85CEC8 onto the emulated VM stack.
- Interpreter state transfers retain this value; instruction10074E6 readsB8
  and10074E8 reads1C85CEC8 from the VM stack.
- Instruction97CE69 writes the sum1C85CF80 back to the VM stack.
- FDE533 loads ECX with1C85CF80; FDE535 executes MOV EDX,[ECX], which faults
  in the offline model because the actual current process has no readable page.

## Source corroboration and limits

Local VMProtect3.9.1 `core/intel.cc:15824..15940` emits a CPU-hash guard for
virtualized function entry whenever a runtime is present. Its semantics are:

1. Save flags/EAX/ECX/EBX/EDX.
2. CPUID leaf1, apply Athlon quirk, maskEBX with00FFFFFF and add toEAX.
3. Load PEB, add OSBuildNumber<<7 toEAX.
4. Load the image's `loader_data` pointer and XOR it intoEAX.
5. Read saltedCPU_COUNT at pointer+randomized field offset.
6. Scan saltedCPU_HASH entries at another randomized field offset.
7. A match restores registers/flags and continues original code. A mismatch
   restores state, setsEAX=DEADC0DE and transfers tozero using PUSH0/RET.

The observed sequence matches this distinctive generated prefix through the
first loader-data field dereference. Therefore91BC4B is a **strongly supported
VMP loader_data/GlobalData pointer candidate**, and+B8 is a CPU_COUNT-field
candidate. Exact randomized field indices/salts, guard completion and any
downstream runtime consumers have not been reconstructed. Do not declare those
fields verified solely from this source version.

`runtime/common.h:253..287` defines salted GlobalData fields, including CPU
count/hash, session key, debugger/patch status and OS build number.
`runtime/loader.cc:906..960` allocates/initializes GlobalData and establishes
the OS-dependentCPU salt. This explains why a retained dump can contain an
image-local pointer to state that existed outside the captured image.

This is an additional supported dependency gap, not proof it alone causes the
missing menu: actual startup has not been observed reaching these roots. Calling
the roots or changing91BC4B based on guessed data is not justified. A supported
repair requires recovering the runtime-state contract/fields and the actual
bootstrap caller, then testing game behavior.

No payload or debugger state, source loader, IDA name/comment or IDA index was
changed. No complete native or interpreter function was reversed here; this
report is data/protected-prefix candidate evidence for the root investigator
to retain with the existing IDA findings.
