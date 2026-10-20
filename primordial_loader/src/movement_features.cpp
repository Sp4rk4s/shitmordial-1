// Current Source callback adapter for guarded original movement machine code.
// The full original CreateMove callback still needs unrelated dependency repair.
#include "movement_features.h"
#include "original_movement.h"
#include "menu_ui.h"
#include "menu_profile.h"
#include <windows.h>
#include <MinHook.h>
#include <cstddef>
#include <cstdio>
#include <cstring>

extern "C" {
__declspec(dllexport) volatile long primordial_movement_state=0;
__declspec(dllexport) volatile long primordial_movement_commands=0;
__declspec(dllexport) volatile long primordial_movement_changes=0;
}
namespace movement_features {
namespace {
struct Table;
struct Prop { const char* name; int type,flags,string_size,inside_array; void *extra,*array,*array_length,*proxy,*table_proxy; Table* table; int offset,stride,count; const char* parent; };
struct Table { Prop* props; int count; void* decoder; const char* name; bool initialized,in_list; };
struct Class { void *create,*create_event; const char* name; Table* table; Class* next; int id; };
static_assert(sizeof(Prop)==60 && offsetof(Prop,offset)==44 && sizeof(Class)==24);
struct Fields { unsigned health=0,flags=0,life=0,water=0,render=0,velocity=0,ground=0; } fields;
using Move=bool(__thiscall*)(void*,float,void*);
Move original=nullptr;
void *engine=nullptr,*entities=nullptr;
bool controls_checked=false;
template<class F> F method(void* object,unsigned slot){return reinterpret_cast<F>((*reinterpret_cast<void***>(object))[slot]);}
void* factory(const char* module,const char* version){
    using Create=void*(__cdecl*)(const char*,int*);
    auto create=reinterpret_cast<Create>(GetProcAddress(GetModuleHandleA(module),"CreateInterface"));
    return create?create(version,nullptr):nullptr;
}
unsigned find(Table* table,const char* name,unsigned parent=0,unsigned depth=0){
    if(!table || depth>16 || table->count<0 || table->count>4096)return 0;
    unsigned found=0;
    for(int i=0;i<table->count;++i){
        auto& prop=table->props[i];
        if(prop.offset<0 || prop.offset>0x20000 || parent+prop.offset>0x20000)return 0;
        const auto offset=parent+prop.offset;
        unsigned result=0;
        if(prop.name && !std::strcmp(prop.name,name))result=offset;
        else if(prop.type==6 && prop.table)result=find(prop.table,name,offset,depth+1);
        if(result){if(found && found!=result)return 0;found=result;}
    }
    return found;
}
bool label(unsigned rva,const wchar_t* expected){
    auto* object=reinterpret_cast<const unsigned char*>(0x44340000+rva);
    if(*reinterpret_cast<const unsigned*>(object)!=0x447505D4)return false;
    const auto length=*reinterpret_cast<const unsigned*>(object+0x1C);
    const auto capacity=*reinterpret_cast<const unsigned*>(object+0x20);
    const auto key=*reinterpret_cast<const unsigned*>(object+0x24);
    if(length!=wcslen(expected) || length>64 || capacity<length || capacity>4096)return false;
    auto* text=capacity<8?reinterpret_cast<const wchar_t*>(object+12):*reinterpret_cast<const wchar_t*const*>(object+12);
    for(unsigned i=0;i<length;++i)if((text[i]^(key&0xFFFF))!=expected[i])return false;
    return true;
}
void process(void* command){
    InterlockedIncrement(&primordial_movement_commands);
    if(primordial_movement_state==4 || primordial_menu_state!=menu_ui::ready || !command)return;
    if(!controls_checked){
        if(!label(0x53D0E8,L"bunnyhop") || !label(0x53DC20,L"infinite duck stamina") ||
           !label(0x53D228,L"autostrafer") || !label(0x53DAE0,L"fast stop")){
            InterlockedExchange(&primordial_movement_state,4);std::puts("[movement] Control identity mismatch; disabled.");return;
        }
        controls_checked=true;
    }
    // This is the same command layout verified in current CInput::CreateMove;
    // the game later copies its verified command and computes its own CRC.
    auto* bytes=static_cast<unsigned char*>(command);
    if(*reinterpret_cast<int*>(bytes+4)<=0 || !method<bool(__thiscall*)(void*)>(engine,26)(engine))return;
    const auto index=method<int(__thiscall*)(void*)>(engine,12)(engine);
    if(index<1 || index>64)return;
    auto* player=static_cast<unsigned char*>(method<void*(__thiscall*)(void*,int)>(entities,3)(entities,index));
    if(!player)return;
    const Player state{*reinterpret_cast<int*>(player+fields.health),*reinterpret_cast<unsigned*>(player+fields.flags),
        player[fields.life],player[fields.water],player[fields.render+1]};
    InterlockedExchange(&primordial_movement_state,2);
    if(*reinterpret_cast<volatile unsigned char*>(0x447F33B0+0x99))return;
    if(state.health<=0 || state.life || state.move==8 || state.move==9 || state.move==10)return;
    unsigned char before[100];std::memcpy(before,bytes,sizeof(before));
    __try {original_movement::run(command,engine,entities);}
    __except(EXCEPTION_EXECUTE_HANDLER){
        std::memcpy(bytes,before,sizeof(before));
        std::printf("[movement] Original movement stopped after exception %08lX; command restored.\n",GetExceptionCode());
        InterlockedExchange(&primordial_movement_state,4);return;
    }
    if(std::memcmp(before,bytes,sizeof(before))){InterlockedIncrement(&primordial_movement_changes);InterlockedExchange(&primordial_movement_state,3);}
}
bool __fastcall hook(void* object,void*,float frame,void* command){
    const bool result=original(object,frame,command);
    __try {process(command);}
    __except(EXCEPTION_EXECUTE_HANDLER){InterlockedExchange(&primordial_movement_state,4);}
    return result;
}
bool checked_install(){
    if(original)return true;
    if(!menu_profile::verify_module_files())return false;
    auto* client=factory("client.dll","VClient018");
    engine=factory("engine.dll","VEngineClient014");entities=factory("client.dll","VClientEntityList003");
    if(!client || !engine || !entities)return false;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA("client.dll"));
    auto* getter=method<unsigned char*>(client,10);
    const unsigned char prefix[]={0x55,0x8B,0xEC,0x8B,0x0D};
    const unsigned char suffix[]={0x8B,0x01,0x5D,0xFF,0x60,0x30};
    if(getter!=base+0x260730 || std::memcmp(getter,prefix,5) || std::memcmp(getter+9,suffix,6))return false;
    auto* mode=**reinterpret_cast<void***>(getter+5);
    auto* target=method<void*>(mode,24);
    if(target!=base+0x276D20 || std::memcmp(target,"\x55\x8B\xEC\x56\x8D\x75\x04\x8B\x0E",9))return false;
    // Confirm the actual caller: slot24, command pointer and sample-time pushes.
    const unsigned char caller[]={0x8B,0x40,0x60,0x56,0xF3,0x0F,0x11,0x46,0x54,0xF3,0x0F,0x10,0x45,0x0C,0x51,0xF3,0x0F,0x11,0x04,0x24,0xFF,0xD0};
    if(std::memcmp(base+0x305C76,caller,sizeof(caller)))return false;
    auto* classes=method<Class*(__thiscall*)(void*)>(client,8)(client);
    Table* player=nullptr;unsigned count=0;
    for(auto* item=classes;item && count++<1024;item=item->next)
        if(item->name && !std::strcmp(item->name,"CCSPlayer")){if(player)return false;player=item->table;}
    if(!player || count>=1024)return false;
    fields={find(player,"m_iHealth"),find(player,"m_fFlags"),find(player,"m_lifeState"),find(player,"m_nWaterLevel"),find(player,"m_nRenderMode"),
        find(player,"m_vecVelocity[0]"),find(player,"m_hGroundEntity")};
    if(!fields.health || !fields.flags || !fields.life || !fields.water || !fields.render || !fields.velocity || !fields.ground)return false;
    // NETVAR_OFFSET(CBaseEntity,m_nRenderMode,+1) is corroborated by the
    // installed player's native movement-mode byte access at client+42C129.
    const unsigned char mode_read[]={0x80,0xBE,0x5C,0x02,0x00,0x00,0x00};
    if(fields.render+1!=0x25C || std::memcmp(base+0x42C129,mode_read,sizeof(mode_read)))return false;
    if(!original_movement::prepare(reinterpret_cast<unsigned char*>(0x44340000),{fields.water,fields.flags,fields.velocity,fields.ground}))return false;
    auto status=MH_Initialize();if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    if(MH_CreateHook(target,reinterpret_cast<void*>(&hook),reinterpret_cast<void**>(&original))!=MH_OK)return false;
    HMODULE pin=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&hook),&pin) || MH_EnableHook(target)!=MH_OK){
        MH_RemoveHook(target);original=nullptr;return false;
    }
    std::printf("[movement] Original bunnyhop/duck/autostrafer/fast-stop code installed. Fields health=%X flags=%X life=%X water=%X move=%X velocity=%X ground=%X. Other features unresolved.\n",fields.health,fields.flags,fields.life,fields.water,fields.render+1,fields.velocity,fields.ground);
    InterlockedExchange(&primordial_movement_state,1);return true;
}
}
bool install() noexcept {
    __try {return checked_install();}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
