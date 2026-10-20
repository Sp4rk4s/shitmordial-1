# Loader contract: structural metadata inspection (2026-10-20)

Read-only investigation of the original dump, SHA256
`e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768`.
Runtime base44340000; IDA analysis base44000000. No target execution, debugger
operations, or live database changes were performed. No new payload function was
fully reversed in this investigation.

## Source-supported format boundaries

Local source root: `C:/Users/k/Desktop/vmprotect-3.9.1-master`.

- `core/pefile.cc:1436`, PEExportList::WriteToData, emits a conventional
  IMAGE_EXPORT_DIRECTORY with RVA function/name arrays, WORD ordinal array,
  plaintext export names and DLL name. Export names are not written through the
  VMP import-name cipher here.
- `core/intel.cc:22779` copies this export list into the loader when cpPack is
  enabled. This establishes a supported search format, not proof this exact
  protected dump used those settings or version.
- `runtime/loader.h`, SETUP_IMAGE_DATA has getters for section/import/internal
  import/TLS/runtime entry metadata. It contains no application-specific
  exported initialization contract.
- `runtime/loader.cc:2371` applies internal IMPORT_INFO triples using
  stored = image_base + Name - Key at destination Address. `core/intel.cc:23373`
  constructs those triples from VMP runtime export function types.
- `runtime/loader.cc:2794` calls data.runtime_entry as a three-argument DLL
  lifecycle function. This is VMP runtime initialization; its presence does not
  identify the payload game/menu bootstrap.
- `core/intel.cc:23889..24060` documents and generates a protected DLL entry
  wrapper: SetupImage on process attach, then original file entry with the
  original three arguments, and FreeImage on detach. No extra application
  startup call is established by that generic source.

## Bounded structural searches

A byte-granular scan covered every possible 40-byte IMAGE_EXPORT_DIRECTORY
position. Candidate checks: 1..65536 functions; names<=functions; ordinal
base<=65536; in-image RVA arrays; every nonzero function RVA in image; DLL name
when present printable/NUL-terminated; every export name printable/NUL-terminated;
all name ordinals within function array. Characteristics was unrestricted on the
final scan. No fully valid candidate survived. This does not exclude a removed,
encoded, partially captured or nonconventional export contract.

A separate byte-granular scan covered every possible 12-byte internal IMPORT_INFO
triple, required Name/Address RVAs in captured image and compared the captured
DWORD at Address against uint32(44340000 + Name - Key). No exact triple survived.
This does not prove internal imports are absent: captured metadata or slots may
have been erased, transformed, or use a different stage/base.

Literal little-endian references to known section-table RVA1293864 and import
metadata RVA10AD0CF were absent. VMP can obfuscate its metadata accessors, so this
absence does not establish unused metadata.

Preserved loader inputs were inspected without changes: username `Rizen`, ID841 (349 hex), mode byte0 at RVA4DDEE4. Absolute
runtime-address bytes4481DCE0 occur only at RVAA1E7D/A1EAF;4481DEE4 onlyA1E73.
These references lie in the already-inspected DllMain profile/RNG helper. No
additional initialization input or flag is established by these searches.

## Remaining gap

The searches supply no supported new entry address or export-array contract.
Keep tracing protected/indirect consumers of interface lookup358E60 and worker
setup362970; an evidence-backed caller is still required before any invocation.
