// Current Source bindings/call ordering only; all feature calculations execute
// original primordial.bin instruction blocks from original_visual_cvars.cpp.
#include "visual_cvars.h"
#include "original_visual_cvars.h"
#include "menu_ui.h"
#include "menu_profile.h"
#include <MinHook.h>
#include <cstring>
#include <cstdio>
#include <initializer_list>
extern "C" {
__declspec(dllexport) volatile LONG primordial_visual_cvars_state=0;
__declspec(dllexport) volatile LONG primordial_visual_cvars_frames=0;
}
namespace {
using Frame=void(__thiscall*)(void*,int);
Frame original=nullptr;void* aspect_convar=nullptr;void* blood_convar=nullptr;void* decals_convar=nullptr;
void* factory(const char* module,const char* name){
    using F=void*(__cdecl*)(const char*,int*);
    auto f=reinterpret_cast<F>(GetProcAddress(GetModuleHandleA(module),"CreateInterface"));
    return f?f(name,nullptr):nullptr;
}
template<class T>T method(void* object,unsigned slot){return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[slot]);}
void __fastcall frame(void* object,void*,int stage){
    // Exact original14F740 gate and ordering:14B830 is before the game's
    // FrameStageNotify for stage5. Other original feature roots remain separate.
    if(stage==5&&primordial_visual_cvars_state!=4&&primordial_menu_state==menu_ui::ready){
        __try {
            original_visual_cvars::removals(blood_convar,decals_convar);
            original_visual_cvars::aspect(aspect_convar);
            InterlockedIncrement(&primordial_visual_cvars_frames);
            InterlockedExchange(&primordial_visual_cvars_state,2);
        } __except(EXCEPTION_EXECUTE_HANDLER){
            InterlockedExchange(&primordial_visual_cvars_state,4);
            std::printf("[visuals] Original aspect callback stopped after exception %08lX.\n",GetExceptionCode());
        }
    }
    original(object,stage);
}
bool checked_install(){
    if(!menu_profile::verify_module_files())return false;
    auto* control=reinterpret_cast<unsigned char*>(0x44873E78);
    if(*reinterpret_cast<unsigned*>(control)!=0x44750644)return false;
    const auto length=*reinterpret_cast<unsigned*>(control+0x1C),capacity=*reinterpret_cast<unsigned*>(control+0x20),key=*reinterpret_cast<unsigned*>(control+0x24);
    if(length!=12||capacity<length||capacity>4096)return false;
    const auto* label=capacity<8?reinterpret_cast<const wchar_t*>(control+12):*reinterpret_cast<const wchar_t**>(control+12);
    for(unsigned i=0;i<12;++i)if((label[i]^(key&65535))!=L"aspect ratio"[i])return false;
    for(const auto rva:{0x537CF0u,0x537BB0u}){
        auto* p=reinterpret_cast<unsigned char*>(0x44340000+rva);
        if(*reinterpret_cast<unsigned*>(p)!=0x447505D4)return false;
        const auto n=*reinterpret_cast<unsigned*>(p+0x1C),cap=*reinterpret_cast<unsigned*>(p+0x20),xor_key=*reinterpret_cast<unsigned*>(p+0x24);
        const auto* expected=rva==0x537CF0?L"blood spray":L"decals";
        if(n!=wcslen(expected)||cap<n||cap>4096)return false;
        const auto* text=cap<8?reinterpret_cast<const wchar_t*>(p+12):*reinterpret_cast<const wchar_t**>(p+12);
        for(unsigned i=0;i<n;++i)if((text[i]^(xor_key&65535))!=expected[i])return false;
    }
    void* cvar=factory("vstdlib.dll","VEngineCvar007");void* client=factory("client.dll","VClient018");
    if(!cvar||!client)return false;
    // Registered command list is verified against this installed CCvar layout.
    unsigned visited=0;
    for(auto* node=*reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(cvar)+0x30);node&&visited++<10000;node=*reinterpret_cast<unsigned char**>(node+4)){
        const auto* name=*reinterpret_cast<const char**>(node+12);
        if(name&&!std::strcmp(name,"r_aspectratio")){
            if(aspect_convar)return false;
            aspect_convar=node;
        }
        if(name&&!std::strcmp(name,"violence_hblood")){if(blood_convar)return false;blood_convar=node;}
        if(name&&!std::strcmp(name,"r_drawdecals")){if(decals_convar)return false;decals_convar=node;}
    }
    if(!aspect_convar||visited>=10000||*reinterpret_cast<void**>(static_cast<unsigned char*>(aspect_convar)+0x1C)!=aspect_convar)return false;
    auto* engine=reinterpret_cast<unsigned char*>(GetModuleHandleA("engine.dll"));
    if(!blood_convar||!decals_convar)return false;
    for(void* convar:{blood_convar,decals_convar}){
        if(*reinterpret_cast<void**>(static_cast<unsigned char*>(convar)+0x1C)!=convar||method<void*>(convar,16)!=engine+0x2FBE50||
           std::memcmp(engine+0x2FBE50,"\x55\x8B\xEC\x83\xEC\x28\x56\x8B\xF1\x57\x8B\x7D\x08",13))return false;
    }
    if(method<void*>(aspect_convar,15)!=engine+0x2FBD50||
       std::memcmp(engine+0x2FBD50,"\x55\x8B\xEC\x83\xEC\x28\x56\x8B\xF1",9))return false;
    auto* source=reinterpret_cast<unsigned char*>(GetModuleHandleA("client.dll"));
    void* target=method<void*>(client,37);
    if(target!=source+0x262B20||std::memcmp(target,"\x55\x8B\xEC\x8B\x0D",5)||
       std::memcmp(static_cast<unsigned char*>(target)+9,"\x8B\x01\x8B\x80\x74\x01\x00\x00\xFF\xD0",10))return false;
    if(!original_visual_cvars::prepare(reinterpret_cast<unsigned char*>(0x44340000)))return false;
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    if(MH_CreateHook(target,reinterpret_cast<void*>(&frame),reinterpret_cast<void**>(&original))!=MH_OK)return false;
    HMODULE pin=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&frame),&pin)){
        MH_RemoveHook(target);return false;
    }
    InterlockedExchange(&primordial_visual_cvars_state,1);
    if(MH_EnableHook(target)!=MH_OK){InterlockedExchange(&primordial_visual_cvars_state,4);MH_RemoveHook(target);return false;}
    std::puts("[visuals] Original aspect/blood/decal instructions bound to current Source ConVars at original frame stage.");return true;
}
DWORD WINAPI worker(void*){
    for(unsigned i=0;i<300&&primordial_menu_state!=menu_ui::ready;++i){if(primordial_menu_state==menu_ui::failed)break;Sleep(50);}
    __try {if(primordial_menu_state!=menu_ui::ready||!checked_install())InterlockedExchange(&primordial_visual_cvars_state,4);}
    __except(EXCEPTION_EXECUTE_HANDLER){InterlockedExchange(&primordial_visual_cvars_state,4);}
    return 0;
}
}
namespace visual_cvars {
bool install(HMODULE) noexcept {
    if(InterlockedCompareExchange(&primordial_visual_cvars_state,-1,0)!=0)return false;
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if(!thread){InterlockedExchange(&primordial_visual_cvars_state,4);return false;}
    CloseHandle(thread);return true;
}
}
