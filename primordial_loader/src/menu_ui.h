#pragma once
#include <windows.h>

namespace menu_ui {
enum State : LONG { dormant=0, waiting_for_paint=1, constructing=2, ready=3, failed=4 };
// Called by the loader worker after mapper::inject succeeds. No separate DLL.
// True means hooks installed. First successful draw publishes ready asynchronously.
bool install(HMODULE owner);
}

// Separate from mapping::Handoff: that state still describes payload entry return.
#ifdef MENU_UI_IMPORT
extern "C" __declspec(dllimport) volatile LONG primordial_menu_state;
#else
extern "C" __declspec(dllexport) volatile LONG primordial_menu_state;
#endif
