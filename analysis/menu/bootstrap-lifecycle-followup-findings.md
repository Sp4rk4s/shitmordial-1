# Lifecycle and initializer-bound follow-up

Original primordial.bin identity: SHA256e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768. RVA addresses unless stated. Static/read-only candidate analysis; no process/code changes, complete new function reversal, or index annotations.

Latest protected-entry-runtime-state-findings.md establishes an independent missing VMP loader_data dependency at91BC4B. That dependency does not establish the invocation root of menu construction or justify calling a candidate entry.

## No adjacent omitted native initializer table identified

Original bytes at40E5A8 are zero, followed by1810 C++ function pointers40E5AC..4101F0, then zero4101F4. Following DWORDs4101F8,4101FC,410200,410204,410208..410220 are zero. Existing crt_process_attach passes boundaries corresponding40E5A8..4101F4 to _initterm and4101F8..4101FC to _initterm_e. The latter C initializer range is empty. Original TLS callback boundary410204 is empty. Thus no adjacent extra native callback/constructor table was identified that the current CRT walk omits. A separate protected callback mechanism remains possible.

The native profile/RNG helperA1E50 copies the mode byte from absolute4481DEE4 to4481E245; it does not branch on the mode. Its other branches handle string length/capacity and deallocation bounds. No mode-dependent bootstrap is exposed by this helper.

## Generic VMP lifecycle does not supply an extra application init call

Local source C:/Users/k/Desktop/vmprotect-3.9.1-master/core/intel.cc23889..24060 emits SetupImage on process attach followed by original file entry with original DLL arguments. Runtime source loader.cc2794 can call runtime_entry as a DLL lifecycle function, but that initializes VMP runtime machinery; an application-specific callback is not established by this generic source. Existing loader-contract-structural-findings.md already retained this source distinction.

VMP internal IMPORT_INFO triples at runtime/loader.cc2372 bind stored=image_base+Name-Key. A possible direct root-RVA mechanism was checked: exact four-byte RVAs2CAB40,2E04D0,35AF60,362970,3752F0,358E60,14F360,3CA002,154790 are absent everywhere in original bytes. Evidence internal-import-root-rva-leads.json. This negative search does not exclude encoded/cleared metadata or VM-bytecode dispatch. Earlier structural investigation found no complete matching internal triples; no import recovery was rerun.

No supported additional startup address or invocation contract emerged. The missing caller remains unresolved; restoring VMP guard state alone would not cause the current configured CRT/DllMain path to invoke those roots.
