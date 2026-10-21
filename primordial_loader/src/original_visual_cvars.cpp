#include "original_visual_cvars.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <cstdio>
namespace original_visual_cvars {
namespace {
unsigned char* code=nullptr;
struct Profile {unsigned rva,size;const char* hash;};
constexpr Profile profiles[]={
    {0x14B884,0x27,"a87ce36d79e10b632a0479a45d6ff58b16ea285677cb04a883695c4836420f41"},
    {0x14B914,0x1D,"f582b6aec1ebd11fff72b0cdf0a38eb1f83b1db6c1e9cff57e2f739c88f001f2"},
    {0x14B994,0x1D,"2f2bd717292f64809880dc9201f1849355a87b1411e330ff19b8c9492a6e1744"},
};
bool checked_prepare(unsigned char* payload){
    if(code||payload!=reinterpret_cast<unsigned char*>(0x44340000))return false;
    // Entire native leaf, including original UI read, scale and virtual call.
    BCRYPT_ALG_HANDLE provider=nullptr;unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    bool valid=true;
    for(const auto& profile:profiles){
        char hex[65]{};
        const auto status=BCryptHash(provider,nullptr,0,payload+profile.rva,profile.size,digest,sizeof(digest));
        if(status<0){valid=false;break;}
        for(unsigned i=0;i<32;++i)sprintf_s(hex+2*i,3,"%02x",digest[i]);
        if(std::strcmp(hex,profile.hash)){valid=false;break;}
    }
    BCryptCloseAlgorithmProvider(provider,0);
    unsigned scale=0;std::memcpy(&scale,payload+0x412EC4,4);
    if(!valid||scale!=0x3C23D70A)return false;
    auto* candidate=static_cast<unsigned char*>(VirtualAlloc(nullptr,3*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!candidate)return false;
    for(unsigned i=0;i<3;++i){std::memcpy(candidate+i*4096,payload+profiles[i].rva,profiles[i].size);candidate[i*4096+profiles[i].size]=0xC3;}
    DWORD old=0;
    if(!VirtualProtect(candidate,3*4096,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),candidate,3*4096)){
        VirtualFree(candidate,0,MEM_RELEASE);return false;
    }
    code=candidate;return true;
}
}
bool prepare(unsigned char* payload) noexcept {
    __try{return checked_prepare(payload);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void aspect(void* convar){reinterpret_cast<void(__thiscall*)(void*)>(code)(convar);}
void removals(void* blood,void* decals){
    reinterpret_cast<void(__thiscall*)(void*)>(code+8192)(decals);
    reinterpret_cast<void(__thiscall*)(void*)>(code+4096)(blood);
}
}
