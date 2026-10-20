// Optional diagnostic only: imports readiness from the already loaded normal
// loader. It neither constructs another menu nor calls payload startup again.
#include "menu_ui.h"
#include "movement_features.h"
#include <windows.h>
namespace {
DWORD WINAPI worker(void*) {
    if(primordial_menu_state!=menu_ui::ready || !movement_features::install())
        InterlockedExchange(&primordial_movement_state,4);
    return 0;
}
}
BOOL WINAPI DllMain(HMODULE,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){
        HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
    }
    return TRUE;
}
