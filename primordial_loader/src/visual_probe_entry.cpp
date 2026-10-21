// Optional extension for a currently loaded normal menu; no UI construction or
// replacement of existing movement/FOV/particle hooks.
#include "visual_cvars.h"
#include <windows.h>
BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH)return visual_cvars::install(module)?TRUE:FALSE;
    return TRUE;
}
