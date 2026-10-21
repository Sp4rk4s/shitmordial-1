#pragma once
namespace original_visual_cvars {
bool prepare(unsigned char* payload) noexcept;
// Executes original14B884..14B8AA; actual Source ConVar is the ECX live-in.
void aspect(void* aspect_ratio_convar);
// Original14B994..14B9B0 then14B914..14B930, matching native frame order.
void removals(void* blood_convar,void* decals_convar);
}
