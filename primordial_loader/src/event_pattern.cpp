#include "event_pattern.h"
#include "config.h"
#include "menu_profile.h"
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <intrin.h>

extern "C" { __declspec(dllexport) volatile long primordial_event_pattern_replacements=0; }
namespace event_pattern {
namespace {
using Scan=std::uint32_t*(__thiscall*)(const Pattern*,std::uint32_t*,const std::uint8_t*,std::uint32_t);
Scan original=nullptr;
std::uintptr_t lower=0,upper=0;
void* installed_target=nullptr;

bool digest_matches(const void* data,ULONG length,const unsigned char expected[32]) noexcept {
    BCRYPT_ALG_HANDLE algorithm=nullptr;unsigned char actual[32];
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    const auto status=BCryptHash(algorithm,nullptr,0,
        const_cast<PUCHAR>(static_cast<const unsigned char*>(data)),length,actual,sizeof(actual));
    BCryptCloseAlgorithmProvider(algorithm,0);
    return status>=0 && !std::memcmp(actual,expected,sizeof(actual));
}
unsigned count(const std::uint8_t* data,unsigned size,const std::uint8_t* bytes,unsigned width) {
    unsigned result=0;
    if(size<width)return 0;
    for(unsigned i=0;i<=size-width;++i)if(!std::memcmp(data+i,bytes,width))++result;
    return result;
}
std::uint32_t* __fastcall detour(const Pattern* pattern,void*,std::uint32_t* output,
                               const std::uint8_t* start,std::uint32_t length) {
    Pattern replacement{};bool change=false;
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    __try { if(pattern && (caller==cfg::IMAGE_BASE+0x155352 || caller==cfg::IMAGE_BASE+0x155375))
        change=adapt(*pattern,reinterpret_cast<std::uintptr_t>(start),length,lower,upper,replacement); }
    __except(EXCEPTION_EXECUTE_HANDLER) { change=false; }
    if(change)InterlockedIncrement(&primordial_event_pattern_replacements);
    // Original helper retains its strict end bound, output storage and RET12 ABI.
    return original(change?&replacement:pattern,output,start,length);
}
bool checked_install() {
    if(installed_target)return true;
    if(!menu_profile::verify_module_files())return false;
    auto* module=reinterpret_cast<std::uint8_t*>(GetModuleHandleA("engine.dll"));
    if(!module)return false;
    using Create=void*(__cdecl*)(const char*,int*);
    auto create=reinterpret_cast<Create>(GetProcAddress(reinterpret_cast<HMODULE>(module),"CreateInterface"));
    auto* manager=create?create("GAMEEVENTSMANAGER002",nullptr):nullptr;
    if(!manager)return false;
    auto** vtable=*reinterpret_cast<void***>(manager);
    // Exact installed-engine profile. Public methods independently converge on
    // the same private dispatcher; no function-name guess or first-hit fallback.
    constexpr unsigned rvas[]={0x211A00,0x211A50}, offsets[]={0x39,0xA};
    auto* target=module+0x211C70;
    for(unsigned i=0;i<2;++i){
        auto* wrapper=static_cast<std::uint8_t*>(vtable[8+i]);
        if(wrapper!=module+rvas[i] || std::memcmp(wrapper,"\x55\x8B\xEC",3) || wrapper[offsets[i]]!=0xE8)return false;
        std::int32_t displacement=0;std::memcpy(&displacement,wrapper+offsets[i]+1,4);
        if(wrapper+offsets[i]+5+displacement!=target)return false;
    }
    if(std::memcmp(vtable[9],"\x55\x8B\xEC\x6A\x01\x6A\x00\xFF\x75\x08",10))return false;
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(module+dos->e_lfanew);
    auto* section=IMAGE_FIRST_SECTION(nt);bool found=false;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
        if(!(section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        auto* begin=module+section[i].VirtualAddress;const auto size=section[i].Misc.VirtualSize;
        if(target<begin || target>=begin+size)continue;
        if(std::memcmp(target,new_bytes,18) || count(begin,size,new_bytes,18)!=1 || count(begin,size,old_bytes,18)!=0)return false;
        lower=reinterpret_cast<std::uintptr_t>(begin);upper=lower+size;found=true;break;
    }
    if(!found)return false;
    auto* helper=reinterpret_cast<void*>(cfg::IMAGE_BASE+0x155840);
    constexpr unsigned char hash[]={0x46,0x9C,0x00,0x46,0x65,0xD1,0xEF,0xD1,0x74,0x70,0x65,0xCA,0x5F,0xDD,0xD3,0x1E,0x29,0x56,0xB2,0x92,0x57,0xCF,0x38,0xC1,0x6E,0x4B,0x47,0x17,0x0A,0xCD,0xF5,0x96};
    if(!digest_matches(helper,0x8C,hash))return false;
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    if(MH_CreateHook(helper,reinterpret_cast<void*>(&detour),reinterpret_cast<void**>(&original))!=MH_OK)return false;
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&detour),&pinned)){
        MH_RemoveHook(helper);original=nullptr;return false;
    }
    if(MH_EnableHook(helper)!=MH_OK){MH_RemoveHook(helper);original=nullptr;return false;}
    installed_target=helper;return true;
}
}
bool install() noexcept {
    __try { return checked_install(); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
