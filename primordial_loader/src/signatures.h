#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace sig {

// A signature entry matching hazedumper's config.json schema.
// "relative" means the resolved value is an offset from the module base.
// "offsets" is a dereference chain: read [match + offsets[0]], then [result + offsets[1]], etc.
// "extra" is added to the final result.
struct Entry {
    const char* name;
    const char* module;
    const char* pattern;
    bool        relative;       // subtract module base from pointer reads
    int         extra;          // added after dereference chain
    std::vector<int> offsets;   // dereference chain from match address
    uint32_t    resolved;       // filled at runtime
};

// All known signatures (hazedumper config, last CS:GO build 2023-09-05).
const std::vector<Entry>& all();

// All known netvar offsets (static, no scanning needed).
struct Netvar {
    const char* name;
    uint32_t    offset;
};
const std::vector<Netvar>& netvars();

// Scan local process modules for all signatures. Returns count of failures.
int resolve_all();

// Log all signature statuses.
void dump_status();

// Log all netvar offsets.
void dump_netvars();

// Get a resolved signature by name, or 0 if not found/failed.
uint32_t get(const char* name);

// Get a netvar offset by name, or 0 if not found.
uint32_t get_netvar(const char* name);

}
