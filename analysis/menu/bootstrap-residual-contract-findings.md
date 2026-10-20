# Residual contract and native module-field search (2026-10-20)

Static bounded searches of original dump SHA256
 e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768.
No target execution or payload edits; no additional function fully reversed.

Residual table search, distinct from prior export-directory validation:
Every literal DWORD occurrence was searched for the exact RVAs, analysis VAs and
runtime VAs of2E04D0,2CAB40,3752F0,362970,35AF60 and358E60. No RVA or analysis VA
appears anywhere in the dump. The first five runtime VAs appear only in their own
self-marker argument pairs;358E60 has no runtime VA occurrence. Therefore no
plaintext residual function-RVA array can reference these exact roots. This does
not exclude encoded/relative tables, erased metadata or an unrecognized thunk.
Evidence bootstrap-residual-rva-references.json.

A second search examined all DWORD-aligned fields within the12 module objects
(RVA4DDF28..4DE108), looking for literal runtime addresses from native code range
1000..40D000. References were grouped by the existing IDA function inventory.
No native function grouped references to multiple distinct game modules. The
matches are12 constructors, destructors, section-cache/scan consumers for client,
server or engine, plus vstdlib/tier0 resolver wrappers already retained. No
native common loop waiting on or binding all modules was established. This
search includes object+4 handle fields, unlike the earlier object-start search.
Evidence module-object-field-native-references.json. False instruction alignment
remains possible; grouping alone does not prove instruction/effect identity.

The shared five isolated roots remain subsystem candidates. Their complete
absence from plaintext inbound E8/E9 and exact-pointer/RVA tables makes protected
or indirect dispatch a concrete outstanding boundary. Static scans do not
identify an original external initialization ABI or justify calling any root.
