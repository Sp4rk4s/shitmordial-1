# Self-marked subsystem candidates (2026-10-20)

Subsystem roots remain static candidates. Dump SHA256e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
RVA addresses; IDA base44000000, runtime44340000. No complete subsystem-root reversal, prototype, startup-call contract, or execution claim is made. The two marker helpers were subsequently fully analyzed and retained in live IDA/index as described below.

A byte scan over RVA1000..40D000 collected E8 calls to existing helper717E0/717F0
immediately preceded by PUSH of an in-image native code address. There are five
unique self-address arguments. Evidence native-self-marker-candidates.json.

| Root RVA | Begin marker call | Tail marker call | Observed native content / candidate role |
|---|---|---|---|
|2CAB40|2CAB75 ->717E0|2CBA9A ->717F0|UTF16 XOR30 string `primordial/configs`, path/string operations; config initialization candidate.|
|2E04D0|2E0520 ->717E0|32A9F2 ->717F0|Screen vector queries35FE40, centered coordinates, (730,575) size, string/UI constructors, registers object via32C1D0 at global44B44E5C; menu construction candidate.|
|35AF60|35AF85 ->717E0|35ED4B ->717E0|String/hash/item map building and client section scanning; item/schema initialization candidate. Tail calls SAME marker helper, unlike the other four.|
|362970|3629A5 ->717E0|362A7C ->717F0|Previously retained worker setup candidate.|
|3752F0|375328 ->717E0|3758D6 ->717F0|XOR09 string `resource\\csgo_english.txt`, virtual open/size/read/close shaped calls through4481E1FC; localization file initialization candidate.|

The root first bytes at2CAB40 and3752F0 are JMPs into protected regions.
Straight-line disassembly from those starts becomes misaligned in mixed
protected/native bytes. Saved slices begin at verified PUSH self instruction
boundaries near the native continuations. The protected prologues/return path
remain unresolved; never invoke these addresses with guessed signatures.

All five roots have ZERO direct E8/E9 byte-scanned inbound branches anywhere in
the dump. Every plaintext full runtime-VA occurrence is its own marker pair.
Evidence native-self-marked-references.json. This supports a common protected or
indirect invocation mechanism but does not establish an external loader contract.

Relevant retained evidence:
- self-marker-config-disassembly.txt
- self-marker-ui-head-disassembly.txt
- self-marker-file-disassembly.txt
- native-self-marked-inspection.json (IDA decompilation failures, not evidence of code absence)
- bootstrap-native-candidate-inspection.json (35AF60 candidate pseudocode)

Additional native candidate exclusions:3582C0 iterates114 input-state entries and
is called from154790 at154A6C; it appears to handle per-frame input transitions.
35A130 is a widely called cache/lookup routine rather than a unique root.
6D210 inspects PE sections and hashes section names; the constant88A82EC2 in its
callers is a section lookup key, not established to be a Source export name.
No plausible plaintext hook callback table containing154790,3582C0,358E60 or these
five roots was recovered. In captured4B2000..826654 aligned data, no saved native
function starts in ranges140000..170000 or350000..363000 were found. This is a
bounded negative search; heap registries are not captured by it.

## Marker helpers subsequently verified

Analysis440717E0 is `marked_region_begin_noop`, bytesC2 00 00, cdecl one-code-pointer
call-site contract, no effects. Analysis440717F0 is
`marked_region_erase_after_use`, cdecl one-code-pointer argument, no semantic
return. It follows one E9, or an unusual unsigned EB+5 path; searches64K for the
exact18-byte begin marker from4479FA48; then searches to begin+FFFF for the end
marker4479FA5C. On match it overwrites intervening bytes with CRT rand()%255 via
active import40E598. For regions longer than5 bytes it writes E9 and literal
rel32 value346622h at their start. No bootstrap/registration or VirtualProtect
operation exists in this helper. It assumes readable/writable ranges.

`self-marker-tail-static-model.json` shows all four protected rootE9 target
windows contain no begin marker in unchanged dump bytes; this predicts no mutation
for those unchanged inputs, not observed execution.35AF60 uses the no-op begin
helper at both ends and has no exact WL marker pair.

The local primary Themida3.2.4 SDK at
C:/Users/k/Desktop/re/Themida_x32_x64_v3.2.4/ThemidaSDK/Include/C/ThemidaSDK.h
uses EB10 / WL-space-space / type / ... / WL-space-space macro signatures.
Its CLEAR_START/END types06/07 and VM_START0C differ from the payload marker.
A scan of that SDK finds no exact WLCp marker. Similar WL letters do not identify
the exact protector/version or establish an extra entry contract.

Both helper names/prototypes/comments were applied to the matching live IDA DB,
names/comments read back, and analysis/ida/project-index.json updated. Evidence
self-marker-ida-applied.json. IDA database disk save remains unverified.
