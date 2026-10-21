#pragma once
namespace original_team {
struct Fields {unsigned resource_team,team,survival_team,life,resource_alive,observer_mode,observer_target;};
// Values must come from actual Source game_type/game_mode/
// mp_teammates_are_enemies. resource_slot is an ADDRESS OF a pointer slot.
struct Bindings {void** resource_slot;int game_type,game_mode;bool teammates_are_enemies;void* engine=nullptr;void* entities=nullptr;};
bool prepare(unsigned char* image,const Fields& fields) noexcept;
// Executes original36B2B0, with only verified field operand cells repaired.
// The caller supplies actual Source entities and owns thread synchronization.
bool compare(void* entity,void* other,const Bindings& bindings);
// Original36ACC0 resolves the local/observed player and calls original compare.
bool enemy(void* entity,bool direct_local,const Bindings& bindings);
}
