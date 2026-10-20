#pragma once
#include <algorithm>
namespace menu_input {
// Native Source-button states used by every original UI widget.
struct Button { unsigned state=0; float held=0; };
inline Button advance(Button previous,bool down,float seconds) {
    const bool was_down=previous.state==1||previous.state==2;
    unsigned state=down?(was_down?2u:1u):(was_down?3u:0u);
    return {state,state==2?previous.held+std::clamp(seconds,0.0f,0.1f):0.0f};
}
}
