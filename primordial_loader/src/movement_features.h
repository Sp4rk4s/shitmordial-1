#pragma once
#include <cstdint>
namespace movement_features {
struct Player { int health; unsigned flags; unsigned char life,water,move; };
// Historical bridge oracle, retained for isolated differential regression tests.
// Production movement now executes original_movement::run, not this function.
inline unsigned apply(unsigned buttons,const Player& player,bool bunnyhop,bool duck) noexcept {
    if(player.health<=0 || player.life || player.move==8 || player.move==9 || player.move==10)return buttons;
    if(duck)buttons|=0x400000;
    if(bunnyhop && player.water<2 && !(player.flags&0x40) && !(player.flags&1))buttons&=~2u;
    return buttons;
}
bool install() noexcept;
}
extern "C" __declspec(dllexport) volatile long primordial_movement_state;
extern "C" __declspec(dllexport) volatile long primordial_movement_commands;
extern "C" __declspec(dllexport) volatile long primordial_movement_changes;
