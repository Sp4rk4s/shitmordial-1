#include "original_fov.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <initializer_list>
namespace original_fov {
namespace {
struct Relocation {unsigned offset,target;};
struct Operand {unsigned offset,expected;unsigned Fields::*field;};
struct Profile {unsigned rva,size;unsigned char hash[32];};
#include "original_fov_profile.inc"
unsigned char* payload=nullptr;
unsigned char* code=nullptr;
Fields offsets{};
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(payload+rva);}
bool matches(const Profile& p){
    BCRYPT_ALG_HANDLE provider=nullptr;unsigned char hash[32]{};
    if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    const auto status=BCryptHash(provider,nullptr,0,payload+p.rva,p.size,hash,sizeof(hash));
    BCryptCloseAlgorithmProvider(provider,0);return status>=0 && !std::memcmp(hash,p.hash,sizeof(hash));
}
unsigned target(unsigned rva){
    for(unsigned i=0;i<4;++i)if(profiles[i].rva==rva)return reinterpret_cast<unsigned>(code+i*4096);
    return reinterpret_cast<unsigned>(payload+rva);
}
bool checked_prepare(unsigned char* image,const Fields& fields){
    if(payload || image!=reinterpret_cast<unsigned char*>(0x44340000))return false;
    for(unsigned field:{fields.scoped,fields.item,fields.weapon,fields.life,fields.observer_mode,fields.observer_target,fields.resource_alive})
        if(!field || field>0x20000)return false;
    payload=image;
    for(const auto& p:profiles)if(!matches(p)){payload=nullptr;return false;}
    offsets=fields;code=static_cast<unsigned char*>(VirtualAlloc(nullptr,5*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!code){payload=nullptr;return false;}
    for(unsigned i=0;i<4;++i)std::memcpy(code+4096*i,payload+profiles[i].rva,profiles[i].size);
    for(unsigned i=0;i<4;++i){
        auto* output=code+4096*i;
        for(const auto& r:relocations[i]){
            if(r.offset==~0u)break;
            const unsigned relative=target(r.target)-reinterpret_cast<unsigned>(output+r.offset+4);
            std::memcpy(output+r.offset,&relative,4);
        }
        for(const auto& o:operands[i]){
            if(o.offset==~0u)break;
            unsigned actual=0;std::memcpy(&actual,output+o.offset,4);
            if(actual!=o.expected){VirtualFree(code,0,MEM_RELEASE);code=nullptr;payload=nullptr;return false;}
            const auto address=reinterpret_cast<unsigned>(&(offsets.*(o.field)));std::memcpy(output+o.offset,&address,4);
        }
    }
    // Original render-FOV leaf1500BD..1500DB uses ESI=view. Preserve ESI and
    // supply the verified argument; the copied instructions perform all math.
    auto* world=code+4*4096;
    const unsigned char prefix[]={0x56,0x8B,0x74,0x24,0x08};
    std::memcpy(world,prefix,sizeof(prefix));std::memcpy(world+sizeof(prefix),payload+profiles[4].rva,profiles[4].size);
    world[sizeof(prefix)+profiles[4].size]=0x5E;world[sizeof(prefix)+profiles[4].size+1]=0xC3;
    DWORD old=0;
    if(!VirtualProtect(code,5*4096,PAGE_EXECUTE_READ,&old)||!FlushInstructionCache(GetCurrentProcess(),code,5*4096)){
        VirtualFree(code,0,MEM_RELEASE);code=nullptr;payload=nullptr;return false;
    }
    return true;
}
struct Saved {void* engine;void* entities;void* resource;void* local;void* view;void* original;};
Saved enter(const void* view,const Bindings& b){
    Saved saved{at<void*>(0x4DE174),at<void*>(0x4DE170),at<void*>(0x4DE1B0),at<void*>(0x4DE23C),at<void*>(0x4DE1E0),at<void*>(0x7FB6B8)};
    at<void*>(0x4DE174)=b.engine;at<void*>(0x4DE170)=b.entities;at<void*>(0x4DE1B0)=b.resource_slot;
    at<const void*>(0x4DE1E0)=view;at<void*>(0x7FB6B8)=b.source_viewmodel;return saved;
}
void leave(const Saved& s){
    at<void*>(0x4DE174)=s.engine;at<void*>(0x4DE170)=s.entities;at<void*>(0x4DE1B0)=s.resource;
    at<void*>(0x4DE23C)=s.local;at<void*>(0x4DE1E0)=s.view;at<void*>(0x7FB6B8)=s.original;
}
}
bool prepare(unsigned char* image,const Fields& fields) noexcept {
    __try{return checked_prepare(image,fields);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
float viewmodel(void* client_mode,const void* view,const Bindings& bindings){
    const auto saved=enter(view,bindings);float result=0;
    __try {result=reinterpret_cast<float(__thiscall*)(void*)>(code)(client_mode);}
    __finally {leave(saved);}
    return result;
}
void render(void* view,const Bindings& bindings){
    const auto saved=enter(view,bindings);
    __try {
        // Exact native14FF30 entry gate: A1CD0 must resolve a view entity.
        if(reinterpret_cast<void*(__thiscall*)(void*)>(code+4096)(payload+0x4DE210))
            reinterpret_cast<void(__cdecl*)(void*)>(code+4*4096)(view);
    } __finally {leave(saved);}
}
}
