#include "../primordial_loader/src/movement_features.h"
#include <cstdio>
int main(){
    using movement_features::apply;
    movement_features::Player player{100,1,0,0,2};
    auto check=[](bool value){if(!value)std::puts("movement gate failed");return value;};
    constexpr unsigned other=1|8|0x400;
    if(!check(apply(other|2,player,true,false)==(other|2)))return 1;
    player.flags=0;
    if(!check(apply(other|2,player,true,false)==other))return 1;
    if(!check(apply(other|2,player,false,false)==(other|2)))return 1;
    if(!check(apply(other,player,true,false)==other))return 1;
    if(!check(apply(other,player,false,true)==(other|0x400000)))return 1;
    if(!check(apply(other|2,player,true,true)==(other|0x400000)))return 1;
    player.water=2;
    if(!check(apply(other|2,player,true,false)==(other|2)))return 1;
    player.water=0;player.flags=0x40;
    if(!check(apply(other|2,player,true,false)==(other|2)))return 1;
    player.flags=0;
    const int modes[]={8,9,10};
    for(auto mode:modes){
        player.move=static_cast<unsigned char>(mode);
        if(!check(apply(other|2,player,true,true)==(other|2)))return 1;
    }
    player.move=2;player.health=0;
    if(!check(apply(other|2,player,true,true)==(other|2)))return 1;
    player.health=100;player.life=1;
    return check(apply(other|2,player,true,true)==(other|2))?0:1;
}
