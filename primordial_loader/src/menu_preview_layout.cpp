// Incremental UI-thread layout probe for the already loaded menu-only preview.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "../../third_party/minhook/include/MinHook.h"
namespace {
using Paint=void(__thiscall*)(void*,unsigned,bool,bool);
Paint original=nullptr;
void* panel_api=nullptr;
HANDLE log_file=INVALID_HANDLE_VALUE;
HMODULE own=nullptr;
unsigned selected_panel=0;
bool failed=false,reported=false;
bool content_ready=false;
template<class T>T& field(void* p,unsigned offset){return *reinterpret_cast<T*>(static_cast<unsigned char*>(p)+offset);}
template<class T>T& global(unsigned address){return *reinterpret_cast<T*>(address);}
void message(const char* text){DWORD count=0;WriteFile(log_file,text,static_cast<DWORD>(strlen(text)),&count,nullptr);FlushFileBuffers(log_file);}
int fault(EXCEPTION_POINTERS* e){char text[256];sprintf_s(text,"STOP layout exception=%08lX address=%p; no further writes\r\n",e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress);message(text);return EXCEPTION_EXECUTE_HANDLER;}
void layout() {
    auto* w=reinterpret_cast<void*>(0x447F33B0);
    unsigned begin=field<unsigned>(w,0xD4),end=field<unsigned>(w,0xD8),selected=field<unsigned>(w,0xE0);
    if(!begin||end<begin||(end-begin)%20||(end-begin)/20>32||selected>=(end-begin)/20){failed=true;message("REFUSED tab vector\r\n");return;}
    int x=field<int>(w,0x3C),y=field<int>(w,0x40),width=field<int>(w,0x44),height=field<int>(w,0x48),title=field<int>(w,0xD0);
    if(x<0||y<0||width<200||width>4000||height<100||height>4000||title<1||title>100){failed=true;message("REFUSED window dimensions\r\n");return;}
    // Exact stores in350DBC..350DFD. The original updater is not called.
    field<int>(w,0x5C)=x;field<int>(w,0x60)=y+title;
    field<int>(w,0x64)=width;field<int>(w,0x68)=height-title-(begin==end?8:55);
    for(unsigned entry=begin,index=0;entry<end;entry+=20,++index){
        // Settled read-only tab animation values instead of native input processing.
        global<float>(entry+4)=index==selected?1.0f:0.0f;
        global<float>(entry+8)=index==selected?1.0f:0.0f;
    }
    auto* tab=*reinterpret_cast<void**>(begin+20*selected);
    // Verified34DB30 is a pure four-DWORD rectangle assignment, RET4.
    reinterpret_cast<void(__thiscall*)(void*,const int*)>(0x4468DB30)(tab,&field<int>(w,0x5C));
#ifdef CONTENT_LAYOUT_PROBE
    if(!content_ready){
        auto groups=field<unsigned>(tab,0x84),groups_end=field<unsigned>(tab,0x88),group_index=field<unsigned>(tab,0x80);
        if(!groups||groups_end<groups||(groups_end-groups)%20||group_index>=(groups_end-groups)/20){failed=true;message("REFUSED group vector\r\n");return;}
        for(unsigned entry=groups,index=0;entry<groups_end;entry+=20,++index){global<float>(entry+4)=index==group_index?1.0f:0.0f;global<float>(entry+8)=index==group_index?1.0f:0.0f;}
        auto* group=*reinterpret_cast<void**>(groups+20*group_index);
        int rect[]={x+130,y+title,width-130,field<int>(w,0x68)};
        // Pure rectangle assignment34B860, used by34C9C0 at34CEAE.
        reinterpret_cast<void(__thiscall*)(void*,const int*)>(0x4468B860)(group,rect);
        // First layout only. Nonzero preexisting scrolling needs further recovery.
        if(field<unsigned char>(group,0x68)){failed=true;message("REFUSED already scrolling group\r\n");return;}
        message("group initial layout begin (child input blocked)\r\n");
        reinterpret_cast<void(__thiscall*)(void*,bool)>(0x4468A4B0)(group,true);
        content_ready=true;
        char detail[256];sprintf_s(detail,"group initial layout returned group=%p columns=%d scroll=%u\r\n",group,field<int>(group,0x64),field<unsigned char>(group,0x68));message(detail);
    }
#endif
    if(!reported){
        char text[256];sprintf_s(text,"UI layout applied thread=%lu body=%d,%d,%d,%d selected=%u tab=%p\r\n",GetCurrentThreadId(),x,y+title,width,field<int>(w,0x68),selected,tab);message(text);reported=true;
    }
}
void __fastcall hook(void* object,void*,unsigned panel,bool force,bool allow){
    __try {
        if(!failed){
            if(!selected_panel){using Name=const char*(__thiscall*)(void*,unsigned);auto fn=reinterpret_cast<Name>((*reinterpret_cast<void***>(panel_api))[36]);auto* name=fn(panel_api,panel);if(name&&!strcmp(name,"MatSystemTopPanel"))selected_panel=panel;}
            if(selected_panel&&panel==selected_panel)layout();
        }
    }__except(fault(GetExceptionInformation())){failed=true;}
    original(object,panel,force,allow);
}
DWORD WINAPI worker(void*){
    char path[MAX_PATH];GetModuleFileNameA(own,path,MAX_PATH);auto* slash=strrchr(path,'\\');
#ifdef CONTENT_LAYOUT_PROBE
    const char* log_name="primordial_menu_content.log";
#else
    const char* log_name="primordial_menu_layout.log";
#endif
    if(slash)strcpy_s(slash+1,MAX_PATH-(slash+1-path),log_name);
    log_file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    __try {
        constexpr unsigned char rectangle[]={0x55,0x8B,0xEC,0x8B,0x55,0x08,0x8B,0x02,0x89,0x41,0x6C};
        if(!GetModuleHandleA("menu_preview.dll")||!global<unsigned>(0x44B44E5C)||
           memcmp(reinterpret_cast<void*>(0x4468DB30),rectangle,sizeof(rectangle))){message("REFUSED preview/code guard\r\n");return 1;}
#ifdef CONTENT_LAYOUT_PROBE
        // Use the complete native update prefix, including SEH setup, from the dump.
        constexpr unsigned char actual[]={0x55,0x8B,0xEC,0x64,0xA1,0,0,0,0,0x6A,0xFF,0x68};
        if(!GetModuleHandleA("menu_preview_layout.dll")||memcmp(reinterpret_cast<void*>(0x4468A4B0),actual,sizeof(actual))){message("REFUSED content update profile\r\n");return 1;}
#endif
        using Create=void*(__cdecl*)(const char*,int*);
        auto create=reinterpret_cast<Create>(GetProcAddress(GetModuleHandleA("vgui2.dll"),"CreateInterface"));
        panel_api=create?create("VGUI_Panel009",nullptr):nullptr;if(!panel_api){message("REFUSED panel interface\r\n");return 1;}
        auto target=(*reinterpret_cast<void***>(panel_api))[41];
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return 1;
        status=MH_CreateHook(target,reinterpret_cast<void*>(&hook),reinterpret_cast<void**>(&original));
        if(status!=MH_OK){message("FAILED hook creation\r\n");return 1;}
        HMODULE pinned=nullptr;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&hook),&pinned))return 1;
        status=MH_EnableHook(target);message(status==MH_OK?"Layout-only hook enabled; waiting for paint\r\n":"FAILED hook enable\r\n");
    }__except(EXCEPTION_EXECUTE_HANDLER){message("STOP preflight exception\r\n");return 1;}
    return 0;
}
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){own=module;DisableThreadLibraryCalls(module);auto t=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(!t)return FALSE;CloseHandle(t);}return TRUE;}
