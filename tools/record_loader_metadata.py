"""Record the verified loader metadata in the matching, already-open IDA DB."""
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
SHA256 = "e16dd0ecf4f763be49139d5edff21f2e6106cff1040e2f6a684fabcb7eeb9768"


def rpc(method, params):
    if method not in {"get_metadata", "set_comment", "disassemble_function",
                      "rename_function", "set_function_prototype"}:
        raise ValueError("Unsupported annotation operation")
    request = urllib.request.Request(
        "http://127.0.0.1:13337/mcp",
        data=json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}).encode(),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(request, timeout=20) as response:
        result = json.load(response)
    if "error" in result:
        raise RuntimeError(result["error"])
    return result["result"]


def main():
    metadata = rpc("get_metadata", {})
    if metadata["sha256"] != SHA256 or int(metadata["base"], 16) != 0x44000000:
        raise RuntimeError("Refusing annotations: wrong IDA target or analysis base")
    index_path = ROOT / "analysis/ida/project-index.json"
    index = json.loads(index_path.read_text())
    annotations = [
        ("0x44461100", "Original TLS directory RVA461100, independently confirmed by GCTL .rdata$T. Template VA447A46D4..447A46DC is eight zero bytes. Index VA44B66264/RVA826264. Empty callbacks VA44750204/RVA410204. ZeroFill0, characteristics00300000. Patcher binds a Windows-managed static TLS index via companion primordial_tls.dll; current/existing/future thread isolation verified. See analysis/menu/loader-repair-findings.md."),
        ("0x44826264", "Original payload static TLS index: runtime44B66264, RVA826264. Captured0 must not be reused. Lua state setup analysis44167332 loads FS:[2C], selects this index, stores Lua state at block+4. The patcher clears BSS then binds the loader-assigned index of primordial_tls.dll before constructors. Static TLS is distinct from TlsAlloc slots."),
        ("0x444DDBE0", "Linker-proven .bss RVA4DDBE0 size348A74, end826654. Captured counts and guards cannot survive fresh initialization. Live PID9928 had a fresh interface cache head but captured count29 at runtime44B486E8; worker count44B49564 stayed11. Constructors can omit zero stores assuming ordinary PE zero initialization. Startup clears this exact range, preserving loader input [4DDCE0,4DDEE5), cookie pair outside BSS and alphabet repair. See analysis/menu/loader-repair-findings.md."),
        ("0x44464358", "GCTL linker contribution metadata corroborates .bss RVA4DDBE0 size348A74; .rdata$T RVA461100 size18; .tls RVA4646D4 size4, .tls$ RVA4646D8 size4, .tls$ZZZ RVA4646DC size4. .CRT$XCU RVA40E5B0 size1C44; .CRT$XLA/XLZ at410200/410204, no callbacks. Original .data4B2000 size29730, .data$r4DB730 size1B18, .data$rs4DD248 size998."),
        ("0x45293864", "VMP SECTION_INFO[3] at RVA1293864: {827000,8566FE,60000020}, {40E000,A3646,40000040}, {1000,40C6E0,60000020}. Format and omission of writable original sections match local VMP core/intel.cc23218..23249. Not a complete original section map."),
        ("0x44167332", "Verified Lua setup static TLS store (runtime444A7332): index from absolute44B66264; TLS vector from FS:[2C]; selects vector[index]; writes ESI at block+4. Original TLS directory RVA461100 recovered/corroborated. Containing function sub_441671C0 remains only partially analyzed; ABI not established. Do not call it as an arbitrary bootstrap."),
    ]
    for address, comment in annotations:
        previous = next((x["comment"] for x in index["data_comments"]
                         if int(x["address"], 16) == int(address, 16)), "")
        if previous and comment not in previous:
            comment = previous + "\n\n" + comment
        rpc("set_comment", {"address": address, "comment": comment})
        index["data_comments"] = [x for x in index["data_comments"]
                                  if int(x["address"], 16) != int(address, 16)]
        index["data_comments"].append({"address": address, "comment": comment})
    function = {
        "address": "0x4421EA40", "name": "lua_register_memory_api",
        "aliases": ["sub_4421EA40"],
        "prototype": "int __cdecl lua_register_memory_api(void *context);",
        "comment": "Registers the payload Lua memory API using Lua state at context+20. Installs callbacks including pattern_scan (runtime4455E560, IDA4421E560), text search and vfunc access. Scanner pointer appears at RVA21EB2E as callback registration, not a native startup call. One caller-cleaned context stack argument, plain ret; EAX reflects final Lua cleanup/API result, not a verified boolean. Further library names are inferred. Evidence analysis/menu/scanner-registry.json. This routine does not establish that primary menu bootstrap ran.",
    }
    before = rpc("disassemble_function", {"start_address": function["address"]})
    if int(before["start_ea"], 16) != int(function["address"], 16):
        raise RuntimeError("Refusing annotation of a containing function")
    if before["name"] in function["aliases"]:
        rpc("rename_function", {"function_address": function["address"], "new_name": function["name"]})
    elif before["name"] != function["name"]:
        function["name"] = before["name"]  # preserve an analyst's existing name
        function.pop("prototype")
    if function.get("prototype"):
        rpc("set_function_prototype", {"function_address": function["address"], "prototype": function["prototype"]})
    old_comments = before["lines"][0].get("comments", [])
    for old in old_comments:
        if old not in function["comment"]:
            function["comment"] = old + "\n\n" + function["comment"]
    rpc("set_comment", {"address": function["address"], "comment": function["comment"]})
    after = rpc("disassemble_function", {"start_address": function["address"]})
    if after["name"] != function["name"] or function["comment"] not in after["lines"][0].get("comments", []):
        raise RuntimeError("Function annotation readback failed")
    index["functions"] = [x for x in index["functions"]
                          if int(x["address"], 16) != int(function["address"], 16)] + [function]
    index_path.write_text(json.dumps(index, indent=2) + "\n")
    print("Loader metadata annotated in live IDA and indexed; disk database save unverified.")


if __name__ == "__main__":
    main()
