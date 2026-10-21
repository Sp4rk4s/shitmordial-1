// Current Source interface/camera plumbing only. All feature math and gates
// execute as original primordial.bin instructions in original_fov.cpp.
#include "fov_features.h"
#include "original_fov.h"
#include "menu_ui.h"
#include "menu_profile.h"
#include <MinHook.h>
#include <cstring>
#include <cstdio>
#include <cmath>
extern "C" {
__declspec(dllexport) volatile LONG primordial_fov_state=0;
__declspec(dllexport) volatile LONG primordial_fov_views=0;
__declspec(dllexport) volatile LONG primordial_fov_models=0;
__declspec(dllexport) volatile float primordial_fov_render_before=0,primordial_fov_render_after=0;
__declspec(dllexport) volatile float primordial_fov_model_before=0,primordial_fov_model_after=0;
}
namespace {
constexpr unsigned base=0x44340000;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(base+rva);}
template<class T>T method(void* object,unsigned slot){return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[slot]);}
using View=void(__thiscall*)(void*,void*);
using Model=float(__thiscall*)(void*);
View original_view=nullptr;Model original_model=nullptr;
original_fov::Bindings bindings{};
HMODULE owner=nullptr;DWORD view_thread=0;
alignas(16) unsigned char last_view[0x14C]{};
bool have_view=false;
thread_local void* active_view=nullptr;
void* factory(const char* module,const char* version){
    using F=void*(__cdecl*)(const char*,int*);
    auto f=reinterpret_cast<F>(GetProcAddress(GetModuleHandleA(module),"CreateInterface"));return f?f(version,nullptr):nullptr;
}
struct Table;
struct Prop {const char* name;int type,flags,string_size,inside_array;void *extra,*array,*array_length,*proxy,*table_proxy;Table* table;int offset,stride,count;const char* parent;};
struct Table {Prop* props;int count;void* decoder;const char* name;bool initialized,in_list;};
struct Class {void *create,*event;const char* name;Table* table;Class* next;int id;};
static_assert(sizeof(Prop)==60 && sizeof(Class)==24);
unsigned find(Table* table,const char* name,unsigned parent=0,unsigned depth=0){
    if(!table || depth>16 || table->count<0 || table->count>4096)return 0;
    unsigned found=0;
    for(int i=0;i<table->count;++i){
        const auto& p=table->props[i];if(p.offset<0 || parent+p.offset>0x20000)return 0;
        const auto offset=parent+p.offset;
        const unsigned value=p.name&&!std::strcmp(p.name,name)?offset:p.type==6?find(p.table,name,offset,depth+1):0;
        if(value){if(found && found!=value)return 0;found=value;}
    }
    return found;
}
bool slider(unsigned rva,const wchar_t* expected){
    const auto* p=reinterpret_cast<unsigned char*>(base+rva);
    if(*reinterpret_cast<const unsigned*>(p)!=0x44750644)return false;
    const auto length=*reinterpret_cast<const unsigned*>(p+0x1C),capacity=*reinterpret_cast<const unsigned*>(p+0x20),key=*reinterpret_cast<const unsigned*>(p+0x24);
    if(length!=wcslen(expected)||length>64||capacity<length||capacity>4096)return false;
    const auto* text=capacity<8?reinterpret_cast<const wchar_t*>(p+12):*reinterpret_cast<const wchar_t*const*>(p+12);
    for(unsigned i=0;i<length;++i)if((text[i]^(key&0xFFFF))!=expected[i])return false;
    return true;
}
float __fastcall source_model(void* object,void*){
    const float result=original_model(object);primordial_fov_model_before=result;return result;
}
float __fastcall model_hook(void* object,void*){
    InterlockedIncrement(&primordial_fov_models);
    if(primordial_fov_state==4 || primordial_menu_state!=menu_ui::ready ||
       (!active_view && (!have_view || view_thread!=GetCurrentThreadId())))return source_model(object,nullptr);
    float value=0;
    __try {
        value=original_fov::viewmodel(object,active_view?active_view:last_view,bindings);
        if(!std::isfinite(value)){
            InterlockedExchange(&primordial_fov_state,4);return primordial_fov_model_before;
        }
        primordial_fov_model_after=value;
        InterlockedExchange(&primordial_fov_state,2);
        return value;
    } __except(EXCEPTION_EXECUTE_HANDLER){
        InterlockedExchange(&primordial_fov_state,4);
        std::printf("[FOV] Original callback stopped after exception %08lX.\n",GetExceptionCode());
        return source_model(object,nullptr);
    }
}
void __fastcall view_hook(void* object,void*,void* view){
    InterlockedIncrement(&primordial_fov_views);
    const auto previous=active_view;active_view=view;
    __try {
        if(view && primordial_fov_state!=4 && primordial_menu_state==menu_ui::ready){
            float before=0;std::memcpy(&before,static_cast<unsigned char*>(view)+0xB0,4);
            primordial_fov_render_before=before;
            __try {
                original_fov::render(view,bindings);
                primordial_fov_render_after=*reinterpret_cast<float*>(static_cast<unsigned char*>(view)+0xB0);
            } __except(EXCEPTION_EXECUTE_HANDLER){
                std::memcpy(static_cast<unsigned char*>(view)+0xB0,&before,4);InterlockedExchange(&primordial_fov_state,4);
                std::printf("[FOV] Original render block stopped after exception %08lX; camera field restored.\n",GetExceptionCode());
            }
        }
        original_view(object,view);
        if(view && primordial_fov_state!=4){
            // Exact bytes from the real camera argument, retained only for
            // callbacks outside OverrideView's stack lifetime. No camera
            // values or padding are synthesized.
            std::memcpy(last_view,view,sizeof(last_view));view_thread=GetCurrentThreadId();have_view=true;
        }
    } __finally {active_view=previous;}
}
bool checked_install(){
    if(!menu_profile::verify_module_files() || !slider(0x531DC8,L"render fov add") || !slider(0x532030,L"viewmodel fov add"))return false;
    auto* client=factory("client.dll","VClient018");bindings.engine=factory("engine.dll","VEngineClient014");bindings.entities=factory("client.dll","VClientEntityList003");
    if(!client||!bindings.engine||!bindings.entities||at<void*>(0x4DE174)!=bindings.engine)return false;
    Table *player=nullptr,*weapon=nullptr,*resource=nullptr;unsigned count=0;
    for(auto* c=method<Class*(__thiscall*)(void*)>(client,8)(client);c&&count++<1024;c=c->next){
        if(!std::strcmp(c->name,"CCSPlayer"))player=c->table;
        if(!std::strcmp(c->name,"CWeaponCSBase"))weapon=c->table;
        if(!std::strcmp(c->name,"CPlayerResource"))resource=c->table;
    }
    if(count>=1024||!player||!weapon||!resource)return false;
    const original_fov::Fields fields{find(player,"m_bIsScoped"),find(weapon,"m_iItemDefinitionIndex"),find(player,"m_hActiveWeapon"),find(player,"m_lifeState"),find(player,"m_iObserverMode"),find(player,"m_hObserverTarget"),find(resource,"m_bAlive")};
    auto* source=reinterpret_cast<unsigned char*>(GetModuleHandleA("client.dll"));
    auto* getter=method<unsigned char*>(client,10);
    if(getter!=source+0x260730 || std::memcmp(getter,"\x55\x8B\xEC\x8B\x0D",5)||std::memcmp(getter+9,"\x8B\x01\x5D\xFF\x60\x30",6))return false;
    auto* mode=**reinterpret_cast<void***>(getter+5);
    void* view_target=method<void*>(mode,18);void* model_target=method<void*>(mode,35);
    if(view_target!=source+0x276D70 || model_target!=source+0x278BD0 ||
       std::memcmp(view_target,"\x55\x8B\xEC\x83\xE4\xF8\x8B\x4D\x04",9)||
       std::memcmp(model_target,"\x55\x8B\xEC\x8B\x4D\x04\x83\xEC\x08\x57",10))return false;
    // Current slot35's baseline ConVar independently identifies its role.
    if(std::strcmp(*reinterpret_cast<const char**>(source+0x5245C18+12),"viewmodel_fov"))return false;
    bindings.resource_slot=reinterpret_cast<void**>(source+0x3231380);
    const auto slot=reinterpret_cast<unsigned>(bindings.resource_slot);
    if(std::memcmp(source+0x74DF3D,"\x8B\x3D",2)||*reinterpret_cast<unsigned*>(source+0x74DF3F)!=slot||
       std::memcmp(source+0x23C729,"\x89\x3D",2)||*reinterpret_cast<unsigned*>(source+0x23C72B)!=slot||
       std::memcmp(source+0x23C746,"\xC7\x05",2)||*reinterpret_cast<unsigned*>(source+0x23C748)!=slot||*reinterpret_cast<unsigned*>(source+0x23C74C))return false;
    const auto table=reinterpret_cast<unsigned>(source+0xBB1B08);
    auto* locator=*reinterpret_cast<unsigned char**>(table-4);auto* descriptor=*reinterpret_cast<unsigned char**>(locator+12);
    if(std::strcmp(reinterpret_cast<char*>(descriptor+8),".?AVC_PlayerResource@@"))return false;
    if(!original_fov::prepare(reinterpret_cast<unsigned char*>(base),fields))return false;
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    if(MH_CreateHook(view_target,reinterpret_cast<void*>(&view_hook),reinterpret_cast<void**>(&original_view))!=MH_OK)return false;
    if(MH_CreateHook(model_target,reinterpret_cast<void*>(&model_hook),reinterpret_cast<void**>(&original_model))!=MH_OK){MH_RemoveHook(view_target);return false;}
    HMODULE pin=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&view_hook),&pin)){MH_RemoveHook(view_target);MH_RemoveHook(model_target);return false;}
    bindings.source_viewmodel=reinterpret_cast<void*>(&source_model);
    // Publish before enabling so an immediate game callback cannot be overwritten
    // by an installer status write after it reports a failure.
    InterlockedExchange(&primordial_fov_state,1);
    if(MH_QueueEnableHook(view_target)!=MH_OK||MH_QueueEnableHook(model_target)!=MH_OK||MH_ApplyQueued()!=MH_OK){
        InterlockedExchange(&primordial_fov_state,4);MH_DisableHook(view_target);MH_DisableHook(model_target);return false;
    }
    std::printf("[FOV] Original render/viewmodel FOV code installed; scoped=%X item=%X weapon=%X resourceAlive=%X.\n",fields.scoped,fields.item,fields.weapon,fields.resource_alive);
    return true;
}
DWORD WINAPI worker(void*){
    for(unsigned i=0;i<300 && primordial_menu_state!=menu_ui::ready;++i){if(primordial_menu_state==menu_ui::failed)break;Sleep(50);}
    __try {if(primordial_menu_state!=menu_ui::ready||!checked_install())InterlockedExchange(&primordial_fov_state,4);}
    __except(EXCEPTION_EXECUTE_HANDLER){InterlockedExchange(&primordial_fov_state,4);}
    return 0;
}
}
namespace fov_features {
bool install(HMODULE module) noexcept {
    if(owner)return false;owner=module;
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(!thread)return false;CloseHandle(thread);return true;
}
}
