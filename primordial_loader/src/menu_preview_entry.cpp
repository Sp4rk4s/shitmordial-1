// Optional diagnostic build; normal primordial_loader calls menu_ui::install itself.
#include "menu_ui.h"
namespace {
DWORD WINAPI start(void* module){return menu_ui::install(static_cast<HMODULE>(module))?0:1;}
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH){
        HANDLE thread=CreateThread(nullptr,0,start,module,0,nullptr);
        if(!thread)return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
