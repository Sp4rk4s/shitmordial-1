#include "signatures.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <sstream>

namespace sig {

// ---------------------------------------------------------------------------
// Hazedumper signatures — last CS:GO build (2023-09-05 21:12:09 UTC)
// Source: github.com/frk1/hazedumper/blob/master/config.json
// ---------------------------------------------------------------------------

static std::vector<Entry> s_sigs = {
    // -- engine.dll --
    { "dwClientState",                    "engine.dll",       "A1 ? ? ? ? 33 D2 6A 00 6A 00 33 C9 89 B0",                       true,  0, {1} },
    { "dwClientState_GetLocalPlayer",     "engine.dll",       "8B 80 ? ? ? ? 40 C3",                                            false, 0, {2} },
    { "dwClientState_IsHLTV",             "engine.dll",       "80 BF ? ? ? ? ? 0F 84 ? ? ? ? 32 DB",                            false, 0, {2} },
    { "dwClientState_Map",                "engine.dll",       "05 ? ? ? ? C3 CC CC CC CC CC CC CC A1",                           false, 0, {1} },
    { "dwClientState_MapDirectory",       "engine.dll",       "B8 ? ? ? ? C3 05 ? ? ? ? C3",                                    false, 0, {7} },
    { "dwClientState_MaxPlayer",          "engine.dll",       "A1 ? ? ? ? 8B 80 ? ? ? ? C3 CC CC CC CC 55 8B EC 8A 45 08",      false, 0, {7} },
    { "dwClientState_PlayerInfo",         "engine.dll",       "8B 89 ? ? ? ? 85 C9 0F 84 ? ? ? ? 8B 01",                        false, 0, {2} },
    { "dwClientState_State",              "engine.dll",       "83 B8 ? ? ? ? ? 0F 94 C0 C3",                                    false, 0, {2} },
    { "dwClientState_ViewAngles",         "engine.dll",       "F3 0F 11 86 ? ? ? ? F3 0F 10 44 24 ? F3 0F 11 86",               false, 0, {4} },
    { "clientstate_delta_ticks",          "engine.dll",       "C7 87 ? ? ? ? ? ? ? ? FF 15 ? ? ? ? 83 C4 08",                   false, 0, {2} },
    { "clientstate_last_outgoing_command","engine.dll",       "8B 8F ? ? ? ? 8B 87 ? ? ? ? 41",                                 false, 0, {2} },
    { "clientstate_choked_commands",      "engine.dll",       "8B 87 ? ? ? ? 41",                                               false, 0, {2} },
    { "clientstate_net_channel",          "engine.dll",       "8B 8F ? ? ? ? 8B 01 8B 40 18",                                   false, 0, {2} },
    { "dwGlobalVars",                     "engine.dll",       "68 ? ? ? ? 68 ? ? ? ? FF 50 08 85 C0",                           true,  0, {1} },
    { "dwGameDir",                        "engine.dll",       "68 ? ? ? ? 8D 85 ? ? ? ? 50 68 ? ? ? ? 68",                     true,  0, {1} },
    { "dwSetClanTag",                     "engine.dll",       "53 56 57 8B DA 8B F9 FF 15",                                     true,  0, {} },
    { "dwbSendPackets",                   "engine.dll",       "B3 01 8B 01 8B 40 10 FF D0 84 C0 74 0F 80 BF ? ? ? ? ? 0F 84",  true,  1, {} },
    { "model_ambient_min",                "engine.dll",       "F3 0F 10 0D ? ? ? ? F3 0F 11 4C 24 ? 8B 44 24 20 35 ? ? ? ? 89 44 24 0C", true, 0, {4} },

    // -- client.dll --
    { "dwEntityList",                     "client.dll",       "BB ? ? ? ? 83 FF 01 0F 8C ? ? ? ? 3B F8",                        true,  0, {1} },
    { "dwForceAttack",                    "client.dll",       "89 0D ? ? ? ? 8B 0D ? ? ? ? 8B F2 8B C1 83 CE 04",              true,  0, {2} },
    { "dwForceAttack2",                   "client.dll",       "89 0D ? ? ? ? 8B 0D ? ? ? ? 8B F2 8B C1 83 CE 04",              true, 12, {2} },
    { "dwForceBackward",                  "client.dll",       "55 8B EC 51 53 8A 5D 08",                                        true,  0, {287} },
    { "dwForceForward",                   "client.dll",       "55 8B EC 51 53 8A 5D 08",                                        true,  0, {245} },
    { "dwForceJump",                      "client.dll",       "8B 0D ? ? ? ? 8B D6 8B C1 83 CA 02",                             true,  0, {2} },
    { "dwForceLeft",                      "client.dll",       "55 8B EC 51 53 8A 5D 08",                                        true,  0, {465} },
    { "dwForceRight",                     "client.dll",       "55 8B EC 51 53 8A 5D 08",                                        true,  0, {512} },
    { "dwGameRulesProxy",                 "client.dll",       "A1 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 B8 ? ? ? ? ? 74 7A",          true,  0, {1} },
    { "dwGetAllClasses",                  "client.dll",       "A1 ? ? ? ? C3 CC CC CC CC CC CC CC CC CC CC A1 ? ? ? ? B9",       true,  0, {1, 0} },
    { "dwGlowObjectManager",             "client.dll",       "A1 ? ? ? ? A8 01 75 4B",                                          true,  4, {1} },
    { "dwInput",                          "client.dll",       "B9 ? ? ? ? F3 0F 11 04 24 FF 50 10",                             true,  0, {1} },
    { "dwInterfaceLinkList",              "client.dll",       "8B 35 ? ? ? ? 57 85 F6 74 ? 8B 7D 08 8B 4E 04 8B C7 8A 11 3A 10", true, 0, {} },
    { "dwLocalPlayer",                    "client.dll",       "8D 34 85 ? ? ? ? 89 15 ? ? ? ? 8B 41 08 8B 48 04 83 F9 FF",     true,  4, {3} },
    { "dwMouseEnable",                    "client.dll",       "B9 ? ? ? ? FF 50 34 85 C0 75 10",                                true, 48, {1} },
    { "dwMouseEnablePtr",                 "client.dll",       "B9 ? ? ? ? FF 50 34 85 C0 75 10",                                true,  0, {1} },
    { "dwPlayerResource",                 "client.dll",       "8B 3D ? ? ? ? 85 FF 0F 84 ? ? ? ? 81 C7",                       true,  0, {2} },
    { "dwRadarBase",                      "client.dll",       "A1 ? ? ? ? 8B 0C B0 8B 01 FF 50 ? 46 3B 35 ? ? ? ? 7C EA 8B 0D", true, 0, {1} },
    { "dwSensitivity",                    "client.dll",       "81 F9 ? ? ? ? 75 7E F3 0F 10 05",                                true,  0, {2} },
    { "dwSensitivityPtr",                 "client.dll",       "81 F9 ? ? ? ? 75 7E F3 0F 10 05",                                true,  0, {2} },
    { "dwViewMatrix",                     "client.dll",       "0F 10 05 ? ? ? ? 8D 85 ? ? ? ? B9",                             true,176, {3} },
    { "dwWeaponTable",                    "client.dll",       "B9 ? ? ? ? 6A 00 FF 50 08 C3",                                  true,  0, {1} },
    { "dwWeaponTableIndex",               "client.dll",       "39 86 ? ? ? ? 74 06 89 86 ? ? ? ? 8B 86",                       false, 0, {2} },
    { "dwYawPtr",                         "client.dll",       "8B 44 24 1C 35 ? ? ? ? 89 44 24 14 EB 0D",                      true,  0, {5} },
    { "dwZoomSensitivityRatioPtr",        "client.dll",       "35 ? ? ? ? 89 45 FC EB 0A 8B 01 8B 40 30 FF D0 D9 5D FC A1",    true,  0, {1} },

    // -- client.dll (non-relative) --
    { "m_pStudioHdr",                     "client.dll",       "8B B6 ? ? ? ? 85 F6 74 05 83 3E 00 75 02 33 F6 F3 0F 10 44 24", false, 0, {2} },
    { "m_yawClassPtr",                    "client.dll",       "8B 44 24 1C 35 ? ? ? ? 89 44 24 14 EB 0D",                      true,  0, {5} },
    { "m_pitchClassPtr",                  "client.dll",       "B9 ? ? ? ? 89 44 24 40",                                        true,  0, {1} },
    { "m_bDormant",                       "client.dll",       "8A 81 ? ? ? ? C3 32 C0",                                        false, 8, {2} },
    { "m_bIsLocalPlayer",                 "client.dll",       "80 BF ? ? ? ? ? F2 0F 10 1D",                                   false, 0, {2} },
    { "m_flSpawnTime",                    "client.dll",       "89 86 ? ? ? ? E8 ? ? ? ? 80 BE ? ? ? ? ?",                      false, 0, {2} },
    { "anim_overlays",                    "client.dll",       "8B 89 ? ? ? ? 8D 0C D1",                                        false, 0, {2} },
    { "set_abs_angles",                   "client.dll",       "55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 E8",                  true,  0, {} },
    { "set_abs_origin",                   "client.dll",       "55 8B EC 83 E4 F8 51 53 56 57 8B F1 E8",                        true,  0, {} },
    { "is_c4_owner",                      "client.dll",       "56 8B F1 85 F6 74 31",                                          true,  0, {} },
    { "force_update_spectator_glow",      "client.dll",       "74 07 8B CB E8 ? ? ? ? 83 C7 10",                               true,  0, {} },
    { "find_hud_element",                 "client.dll",       "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39 77 28",               false, 0, {} },

    // -- vstdlib.dll --
    { "interface_engine_cvar",            "vstdlib.dll",      "8B 0D ? ? ? ? C7 05",                                            true,  0, {2} },
    { "convar_name_hash_table",           "vstdlib.dll",      "8B 3C 85",                                                       true,  0, {3} },

    // -- shaderapidx9.dll --
    { "dwppDirect3DDevice9",              "shaderapidx9.dll", "A1 ? ? ? ? 50 8B 08 FF 51 0C",                                  true,  0, {1} },
};

// ---------------------------------------------------------------------------
// Netvar offsets — hazedumper 2023-09-05
// ---------------------------------------------------------------------------

static std::vector<Netvar> s_netvars = {
    { "cs_gamerules_data",              0x0 },
    { "m_ArmorValue",                   0x117CC },
    { "m_Collision",                    0x320 },
    { "m_CollisionGroup",               0x474 },
    { "m_Local",                        0x2FCC },
    { "m_MoveType",                     0x25C },
    { "m_OriginalOwnerXuidHigh",        0x31D4 },
    { "m_OriginalOwnerXuidLow",         0x31D0 },
    { "m_SurvivalGameRuleDecisionTypes",0x1328 },
    { "m_SurvivalRules",                0xD00 },
    { "m_aimPunchAngle",                0x303C },
    { "m_aimPunchAngleVel",             0x3048 },
    { "m_angEyeAnglesX",                0x117D0 },
    { "m_angEyeAnglesY",                0x117D4 },
    { "m_bBombDefused",                 0x29C0 },
    { "m_bBombPlanted",                 0x9A5 },
    { "m_bBombTicking",                 0x2990 },
    { "m_bFreezePeriod",                0x20 },
    { "m_bGunGameImmunity",             0x9990 },
    { "m_bHasDefuser",                  0x117DC },
    { "m_bHasHelmet",                   0x117C0 },
    { "m_bInReload",                    0x32B5 },
    { "m_bIsDefusing",                  0x997C },
    { "m_bIsQueuedMatchmaking",         0x74 },
    { "m_bIsScoped",                    0x9974 },
    { "m_bIsValveDS",                   0x7C },
    { "m_bSpotted",                     0x93D },
    { "m_bSpottedByMask",               0x980 },
    { "m_bStartedArming",               0x3400 },
    { "m_bUseCustomAutoExposureMax",    0x9D9 },
    { "m_bUseCustomAutoExposureMin",    0x9D8 },
    { "m_bUseCustomBloomScale",         0x9DA },
    { "m_clrRender",                    0x70 },
    { "m_dwBoneMatrix",                 0x26A8 },
    { "m_fAccuracyPenalty",             0x3340 },
    { "m_fFlags",                       0x104 },
    { "m_flC4Blow",                     0x29A0 },
    { "m_flCustomAutoExposureMax",      0x9E0 },
    { "m_flCustomAutoExposureMin",      0x9DC },
    { "m_flCustomBloomScale",           0x9E4 },
    { "m_flDefuseCountDown",            0x29BC },
    { "m_flDefuseLength",               0x29B8 },
    { "m_flFallbackWear",               0x31E0 },
    { "m_flFlashDuration",              0x10470 },
    { "m_flFlashMaxAlpha",              0x1046C },
    { "m_flLastBoneSetupTime",          0x2928 },
    { "m_flLowerBodyYawTarget",         0x9ADC },
    { "m_flNextAttack",                 0x2D80 },
    { "m_flNextPrimaryAttack",          0x3248 },
    { "m_flSimulationTime",             0x268 },
    { "m_flTimerLength",                0x29A4 },
    { "m_hActiveWeapon",                0x2F08 },
    { "m_hBombDefuser",                 0x29C4 },
    { "m_hMyWeapons",                   0x2E08 },
    { "m_hObserverTarget",              0x339C },
    { "m_hOwner",                       0x29DC },
    { "m_hOwnerEntity",                 0x14C },
    { "m_hViewModel",                   0x3308 },
    { "m_iAccountID",                   0x2FD8 },
    { "m_iClip1",                       0x3274 },
    { "m_iCompetitiveRanking",          0x1A84 },
    { "m_iCompetitiveWins",             0x1B88 },
    { "m_iCrosshairId",                 0x11838 },
    { "m_iDefaultFOV",                  0x333C },
    { "m_iEntityQuality",               0x2FBC },
    { "m_iFOV",                         0x31F4 },
    { "m_iFOVStart",                    0x31F8 },
    { "m_iGlowIndex",                   0x10488 },
    { "m_iHealth",                      0x100 },
    { "m_iItemDefinitionIndex",         0x2FBA },
    { "m_iItemIDHigh",                  0x2FD0 },
    { "m_iMostRecentModelBoneCounter",  0x2690 },
    { "m_iObserverMode",                0x3388 },
    { "m_iShotsFired",                  0x103E0 },
    { "m_iState",                       0x3268 },
    { "m_iTeamNum",                     0xF4 },
    { "m_lifeState",                    0x25F },
    { "m_nBombSite",                    0x2994 },
    { "m_nFallbackPaintKit",            0x31D8 },
    { "m_nFallbackSeed",                0x31DC },
    { "m_nFallbackStatTrak",            0x31E4 },
    { "m_nForceBone",                   0x268C },
    { "m_nModelIndex",                  0x258 },
    { "m_nTickBase",                    0x3440 },
    { "m_nViewModelIndex",              0x29D0 },
    { "m_rgflCoordinateFrame",          0x444 },
    { "m_szCustomName",                 0x304C },
    { "m_szLastPlaceName",              0x35C4 },
    { "m_thirdPersonViewAngles",        0x31E8 },
    { "m_vecOrigin",                    0x138 },
    { "m_vecVelocity",                  0x114 },
    { "m_vecViewOffset",                0x108 },
    { "m_viewPunchAngle",               0x3030 },
    { "m_zoomLevel",                    0x33E0 },
};

const std::vector<Entry>& all() { return s_sigs; }
const std::vector<Netvar>& netvars() { return s_netvars; }

// ---------------------------------------------------------------------------
// Local pattern scanner (we're inside csgo.exe — direct memory access)
// ---------------------------------------------------------------------------

static bool get_local_module(const char* name, const uint8_t*& base, size_t& size) {
    HMODULE mod = GetModuleHandleA(name);
    if (!mod) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    auto* nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    base = reinterpret_cast<const uint8_t*>(mod);
    size = nt->OptionalHeader.SizeOfImage;
    return true;
}

static std::vector<uint8_t> parse_pattern(const char* pat, std::vector<bool>& mask) {
    std::vector<uint8_t> bytes;
    mask.clear();
    std::istringstream ss(pat);
    std::string token;
    while (ss >> token) {
        if (token == "?") {
            bytes.push_back(0);
            mask.push_back(false);
        } else {
            bytes.push_back(static_cast<uint8_t>(strtoul(token.c_str(), nullptr, 16)));
            mask.push_back(true);
        }
    }
    return bytes;
}

static size_t scan(const uint8_t* buf, size_t size,
                   const std::vector<uint8_t>& bytes,
                   const std::vector<bool>& mask) {
    size_t pat_len = bytes.size();
    if (pat_len == 0 || size < pat_len) return 0;
    for (size_t i = 0; i <= size - pat_len; ++i) {
        bool hit = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (mask[j] && buf[i + j] != bytes[j]) { hit = false; break; }
        }
        if (hit) return i;
    }
    return 0;
}

int resolve_all() {
    int failed = 0;
    printf("[*] Resolving %zu signatures (local scan)...\n", s_sigs.size());

    for (auto& e : s_sigs) {
        const uint8_t* mod_base = nullptr;
        size_t mod_size = 0;
        if (!get_local_module(e.module, mod_base, mod_size)) {
            printf("    [-] %-35s FAILED (%s not loaded)\n", e.name, e.module);
            ++failed;
            continue;
        }

        std::vector<bool> mask;
        auto bytes = parse_pattern(e.pattern, mask);
        size_t match = scan(mod_base, mod_size, bytes, mask);

        if (match == 0) {
            printf("    [-] %-35s FAILED (pattern not found)\n", e.name);
            ++failed;
            continue;
        }

        // Dereference chain — direct pointer reads
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(mod_base);
        uint32_t result = static_cast<uint32_t>(match);
        for (int off : e.offsets) {
            uintptr_t addr = base_addr + result + off;
            result = *reinterpret_cast<uint32_t*>(addr);
            if (e.relative && result >= base_addr)
                result -= static_cast<uint32_t>(base_addr);
        }

        result += e.extra;
        e.resolved = result;
        printf("    [+] %-35s = 0x%X\n", e.name, result);
    }

    printf("[*] Resolved %zu/%zu (%d failed)\n", s_sigs.size() - failed, s_sigs.size(), failed);
    return failed;
}

void dump_status() {
    printf("\n=== Signature Catalog (58 entries, hazedumper 2023-09-05) ===\n");
    printf("%-36s %-20s %-6s %-6s %s\n", "Name", "Module", "Rel", "Extra", "Pattern");
    for (const auto& e : s_sigs) {
        printf("%-36s %-20s %-6s %-6d %.55s\n",
               e.name, e.module, e.relative ? "yes" : "no", e.extra, e.pattern);
    }
    printf("\nTotal: %zu signatures\n", s_sigs.size());
}

void dump_netvars() {
    printf("\n=== Netvars (%zu entries, hazedumper 2023-09-05) ===\n", s_netvars.size());
    for (const auto& n : s_netvars)
        printf("    %-40s 0x%X\n", n.name, n.offset);
}

uint32_t get(const char* name) {
    for (const auto& e : s_sigs)
        if (strcmp(e.name, name) == 0) return e.resolved;
    return 0;
}

uint32_t get_netvar(const char* name) {
    for (const auto& n : s_netvars)
        if (strcmp(n.name, name) == 0) return n.offset;
    return 0;
}

}
