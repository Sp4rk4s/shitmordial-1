// Optional extension for an already loaded working normal menu. It adds only
// distinct camera/FOV hooks; does not replace the movement hook or initialize UI.
#include "fov_features.h"
#include <windows.h>
BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH)return fov_features::install(module)?TRUE:FALSE;
    return TRUE;
}
