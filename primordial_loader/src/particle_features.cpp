// Hook plumbing only. Both complete, unchanged primordial.bin callbacks perform
// every particle-name decision, UI gate, RGB conversion and state restoration.
#include "particle_features.h"
#include "menu_ui.h"
#include "menu_profile.h"
#include <MinHook.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
extern "C" {__declspec(dllexport) volatile LONG primordial_particle_state=0;}
namespace {
constexpr unsigned base=0x44340000;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(base+rva);}
bool hash(unsigned rva,unsigned size,const char* expected){
    BCRYPT_ALG_HANDLE provider=nullptr;unsigned char digest[32]{};char hex[65]{};
    if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    const auto status=BCryptHash(provider,nullptr,0,reinterpret_cast<PUCHAR>(base+rva),size,digest,sizeof(digest));
    BCryptCloseAlgorithmProvider(provider,0);if(status<0)return false;
    for(unsigned i=0;i<32;++i)sprintf_s(hex+2*i,3,"%02x",digest[i]);
    return !std::strcmp(hex,expected);
}
bool label(unsigned rva,unsigned vtable,const wchar_t* expected){
    auto* p=reinterpret_cast<const unsigned char*>(base+rva);
    if(*reinterpret_cast<const unsigned*>(p)!=vtable)return false;
    const auto length=*reinterpret_cast<const unsigned*>(p+0x1C),capacity=*reinterpret_cast<const unsigned*>(p+0x20),key=*reinterpret_cast<const unsigned*>(p+0x24);
    if(length!=wcslen(expected)||length>64||capacity<length||capacity>4096)return false;
    const auto* text=capacity<8?reinterpret_cast<const wchar_t*>(p+12):*reinterpret_cast<const wchar_t*const*>(p+12);
    for(unsigned i=0;i<length;++i)if((text[i]^(key&0xFFFF))!=expected[i])return false;
    return true;
}
bool checked_install(){
    if(!menu_profile::verify_module_files()||
       !label(0x538F90,0x447505D4,L"modulate fire")||!label(0x5372F0,0x44750664,L"smoke"))return false;
    if(!hash(0x155F40,0x307,"a4c8e2d09759c9f6626caf27c91b396baf837455a985a8ca09fa795ef62747c5")||
       !hash(0x155C30,0x30A,"52fe8cc5f20d9e68b1bb558032f6301086fb76eaeb387b5d64d33347fd1961f7")||
       !hash(0x6CF70,0x39,"4dfe4697a93dca45c27473dba635b89d7bb4bd8dc4e0fbaf704ec709c95e3940")||
       at<float>(0x416900)!=255.0f||at<void*>(0x7FB8F8)||at<void*>(0x7FB8FC))return false;
    auto* client=reinterpret_cast<unsigned char*>(GetModuleHandleA("client.dll"));if(!client)return false;
    auto* table=client+0xD09110;
    auto* locator=*reinterpret_cast<unsigned char**>(table-4);
    auto* descriptor=*reinterpret_cast<unsigned char**>(locator+12);
    if(std::strcmp(reinterpret_cast<char*>(descriptor+8),".?AVC_INIT_RandomColor@@"))return false;
    auto* single=client+0x9597A0;auto* block=client+0x959B30;
    if(*reinterpret_cast<void**>(table+29*4)!=single||*reinterpret_cast<void**>(table+30*4)!=block||
       std::memcmp(single,"\x55\x8B\xEC\x83\xEC\x18\x56\x8B\xF1\xC7\x45\xFC\xFF\xFF\xFF\xFF",16)||
       std::memcmp(block,"\x55\x8B\xEC\x83\xE4\xF0\x81\xEC\x48\x01\x00\x00",12)||
       std::memcmp(single+0x30,"\x8B\x8F\x88\x00\x00\x00",6)||
       std::memcmp(block+0x4D,"\xF3\x0F\x10\x4E\x5C",5))return false;
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    if(MH_CreateHook(single,reinterpret_cast<void*>(base+0x155F40),&at<void*>(0x7FB8F8))!=MH_OK)return false;
    if(MH_CreateHook(block,reinterpret_cast<void*>(base+0x155C30),&at<void*>(0x7FB8FC))!=MH_OK){
        MH_RemoveHook(single);at<void*>(0x7FB8F8)=nullptr;return false;
    }
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&checked_install),&pinned)){
        MH_RemoveHook(single);MH_RemoveHook(block);at<void*>(0x7FB8F8)=at<void*>(0x7FB8FC)=nullptr;return false;
    }
    if(MH_QueueEnableHook(single)!=MH_OK||MH_QueueEnableHook(block)!=MH_OK||MH_ApplyQueued()!=MH_OK){
        MH_DisableHook(single);MH_DisableHook(block);return false;
    }
    InterlockedExchange(&primordial_particle_state,1);
    std::puts("[visuals] Original fire/smoke particle callbacks installed unchanged; ESP/chams still recovering.");
    return true;
}
DWORD WINAPI worker(void*){
    for(unsigned i=0;i<300&&primordial_menu_state!=menu_ui::ready;++i){if(primordial_menu_state==menu_ui::failed)break;Sleep(50);}
    __try {if(primordial_menu_state!=menu_ui::ready||!checked_install())InterlockedExchange(&primordial_particle_state,4);}
    __except(EXCEPTION_EXECUTE_HANDLER){InterlockedExchange(&primordial_particle_state,4);}
    return 0;
}
}
namespace particle_features {
bool install(HMODULE) noexcept {
    if(InterlockedCompareExchange(&primordial_particle_state,-1,0)!=0)return false;
    HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if(!thread){InterlockedExchange(&primordial_particle_state,4);return false;}
    CloseHandle(thread);return true;
}
}
