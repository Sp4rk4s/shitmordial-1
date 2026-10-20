# Renderer bounds repair and recovered fonts — 2026-10-20

Baseline input remains unchanged: primordial.bin SHA256
e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
IDA44000000, runtime44340000. Addresses below are RVAs unless prefixed otherwise.
**Latest actual user result remains no menu in PID11240. No new patcher or menu
initializer has been injected during this follow-up.**

## IDA metadata correction is applied and saved

The user successfully ran tools/ida_fix_dump_analysis.py. Receipt
analysis/ida/mutable-dump-segment-applied.json verifies the same segment bounds,
name and type; classCODE/permissions0 became classPRMDUMP/permissionsWRITE.
This removes the class-based immutable-data assumption without imposing guessed
original PE section boundaries. The script now uses the installed modern
get_segment_class/set_segment_class APIs to avoid its deprecation warnings.

Fresh decompilation of user DllMain still calls the profile/RNG attach helper
only. The helper itself is not defined as a normal IDA function and its requested
decompilation failed; this is not evidence of extra or absent behavior there.
Evidence mutable-recheck-0x4414F360.json and mutable-recheck-0x440A1E50.json.

## Concrete empty-selector defect and supported repair

Function343560 takes ECX=selector, an uninitialized24-byte output wstring and
signed index on the stack, returns output inEAX and RET8. Selector+150/+154
contain a vector of28-byte encrypted-string records. After rejecting negative
indices it computes count=(end-begin)/28, then compares index against count-1
using unsignedJA at343592. For count0 this becomesFFFFFFFF, so index0 is accepted
and null is passed to333C60. That helper copy-constructs a wstring before XOR
decoding its words with the wrapper key byte+18. The copy faults at source+14.

The bounded offline menu frame reproduces this exact ordinary null access:
render-dependency-probe-11240-absent-crt-source-native-draw-config.json.
The copied TEB at address0 is allowed ONLY for actual FS-prefixed instructions;
otherwise a null-page access stops the replay. Earlier continuation beyond that
point without this check must not be treated as valid execution evidence.

startup.cpp validates the exact42-byte decision sequence transactionally before
any write, then changes only opcode343592 from77(JA) to7F(JG). Negative indices
still take the existing fallback. For every physically feasible valid32-bit
vector count, this preserves nonempty bounds and correctly rejects count0.
It does not disable checks or redirect to an invented startup routine.

tests/startup_tests.cpp executes just those original/repaired42 arithmetic and
branch bytes in an isolated native x86 fixture with synthetic success/failure
returns. No payload string routine or entry is called. It reproduces original
empty/index0 acceptance, tests repaired negative/valid/end/huge indices across
six counts, and checks unchanged nonempty decisions. Instruction/profile mismatch
must fail before any image mutation. All unrelated image bytes remain checked.

Both complete native functions are named/prototyped/commented and read back in
live IDA, with the branch annotated. See selector-ida-applied.json and the updated
analysis/ida/project-index.json. New function names:
selector_copy_option_label at44343560 and encrypted_wstring_copy_plaintext
at44333C60. Saving these newest annotations to disk has not yet been verified;
the earlier metadata-correction save is separately verified. Replay is included
in tools/ida_retain_render.py via selector-annotations-pending.json.

## Two more precise captured ownership fields

The configuration initializer2CAB40 releases and replaces the captured wstring
at4B3D14 (old pointer41C66620, length18, capacity23). It assigns the native
literal primordial/configs, creates primordial and primordial/configs, then
enumerates the directory. With explicit virtual empty-directory Win32 models,
it reaches RET2CBAB2 and balanced caller sentinel after683,916 instructions.
No input-stack arguments or incoming-register buffer accesses were observed.
Its observed synthetic loader-context reads are the already recovered+B8/+20.
No real filesystem operation or target function was performed.
Evidence config-constructor-native-crt-11240-virtual-dirs.json.

Periodic renderer work also releases captured wstring4B3C44 through nativeA11A0.
Its old pointer417EADA0, length19, capacity23 and all remaining union bytes are
recorded exactly. Continued native work constructs/queries primordial/skyboxes.
The helper using selector4487A430 is not confidently classified as configuration
or scripts; do not assign that feature name just from its proximity to refresh.

Both newly verified fields are now exact-profile-guarded and emptied without
freeing old pointers, alongside the five earlier wstrings and20-byte vector.
This is eight ownership objects total, including the vector. The reset preserves
unrelated scalars and settings. Native ownership/mismatch tests cover every field.

Latest source build: build/menu-render-guard/. All13 x86 and6 Steam tests pass.
This build is not loaded in PID11240. Bounds/ownership repair alone still does
not establish interface, font or paint-hook initialization.

## Original embedded icon font assets recovered

A bounded SFNT directory scan found two intact fonts. Their complete padded
32-bit checksums both equal the requiredB1B0AFBA. Table directories and name
records establish family names and exact extents; no font was loaded into the
target or analysis process.

|Family|RVA|Bytes|Derived artifact|
|---|---|---|---|
|primordial_menu_icons|4B3D58|13568|fonts/primordial_menu_icons.ttf|
|primordial_csgo_icons|4B7258|147108|fonts/primordial_csgo_icons.ttf|

Hashes, full table/name evidence and checksums are in embedded-font-scan.json.
The active339 import profile already includes GDI32.AddFontMemResourceEx, a
relevant registration lead. There are no literal DWORD references to either
complete runtime font address in the dump, nor to the second exact size. That
bounded negative scan does not establish that registration code is absent.
See embedded-font-literal-references.json.

The primary draw requests six font records:580F34,580F40,7FB610,805430,805480,
8054C8. They are zero in the constructed replay. Creating a window tree does
not initialize these fonts. The older cached-font constructor also has a private
FontManager language-field incompatibility; do not bind that object by guess.

## Isolated original primary-window draw completes offline

render_dependency_probe.py reuses the constructed Unicorn snapshot. It records
explicit surface drawing models rather than calling the target or producing
pixels. Source engine IsInGame is derived read-only from its verified current
signon predicate; timing and client rectangle are copied facts. Text metrics and
surface drawing remain explicitly synthetic. Native copied CRT code stops on
missing pages or OS boundaries; no unmodeled OS result is supplied.

Calling primary-window draw351AB0 with ECX447F33B0 and emulated opacity1 reaches
RET3524C3 and caller sentinel12345678 with ESP10010004 after414,720 instructions.
It records62 text draws, including the original title/tab labels. It performs
no loader-context reads in this bounded path, and skips the full frame manager's
periodic filesystem/feature callbacks. Evidence:
render-dependency-probe-11240-absent-crt-source-native-draw-fixed-window.json.
This proves a bounded modeled control-flow path, not actual game rendering or
an authorization to call initializers live.

Full frame continuation is still bounded at further operating-system filesystem
calls. Do not describe it as a completed actual frame or full game bootstrap.
The current project guide forbids forced subsystem calls and guessedGlobalData.
A scope clarification was requested before any separate menu-only live test;
no response or authorization should be inferred from elapsed time.

## Remaining original-bootstrap work

Byte scans retain24 aligned and307 total native-range E9-to-protected candidates;
these are not a control-flow graph or verified function-entry inventory. Several
were explored only offline. Candidate360080 reads missing EngineClient4481E174;
352B80 also reaches an absent engine-interface dependency. Candidate35AEE0 reaches
a captured external address after an item/schema-shaped native continuation.
Candidate36E110 reaches VirtualProtect with missing global inputs. CandidateB5320
resolves Source vstdlib.RandomSeed; continuation is incomplete. E2030 builds
primordial/nades,162FA0 builds primordial/scripts under explicit directory models.
Neither has been identified as the top-level bootstrap or font registration.
Candidate1564B0 remains incomplete at its2M instruction budget. No new live calls,
interfaces, callback arrays or alleged exports were invented from these leads.

Debugger paint observations were read-only apart from owned test breakpoints.
Temporary Source paint breakpoints were removed and the game resumed. The
conditional-breakpoint API refused because script permission is disabled; that
setting was not changed. No claim of a match to the specific engine panel follows
from the bounded paint samples.
