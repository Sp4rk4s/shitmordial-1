#include "../primordial_loader/src/menu_input.h"
#include <cstdio>
#include <cmath>
int main(){
    menu_input::Button b;
    auto check=[](bool ok){if(!ok){std::puts("input transition failed");return false;}return true;};
    b=menu_input::advance(b,true,0.016f);
    if(!check(b.state==1&&b.held==0))return 1;
    for(int i=0;i<50;++i)b=menu_input::advance(b,true,0.016f);
    if(!check(b.state==2&&std::abs(b.held-0.8f)<0.0001f))return 1;
    // Focus loss releases an active key once and clears repeat duration.
    b=menu_input::advance(b,false,0.016f);
    if(!check(b.state==3&&b.held==0))return 1;
    b=menu_input::advance(b,false,0.016f);
    if(!check(b.state==0&&b.held==0))return 1;
    b=menu_input::advance(b,true,5.0f);b=menu_input::advance(b,true,5.0f);
    if(!check(b.state==2&&b.held==0.1f))return 1;
    // A one-frame wheel pulse must be an edge, then release, then up.
    menu_input::Button wheel;
    wheel=menu_input::advance(wheel,true,0);
    if(!check(wheel.state==1))return 1;
    wheel=menu_input::advance(wheel,false,0);
    if(!check(wheel.state==3))return 1;
    wheel=menu_input::advance(wheel,false,0);
    return check(wheel.state==0)?0:1;
}
