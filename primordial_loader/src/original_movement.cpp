#include "original_movement.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <cstdio>

namespace original_movement {
namespace {
constexpr unsigned base=0x44340000;
struct Relocation { unsigned offset,target; };
struct Operand { unsigned offset,expected; unsigned Fields::* field; };
struct Profile { unsigned rva,size; unsigned char hash[32]; };
#include "original_movement_profile.inc"
unsigned char* payload=nullptr;
unsigned char* capsules=nullptr;
Fields offsets{};
using Leaf=void(__cdecl*)();
Leaf bunnyhop=nullptr,duck=nullptr,strafe=nullptr,stop=nullptr;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(payload+rva);}
bool matches(const Profile& p){
    BCRYPT_ALG_HANDLE provider=nullptr;unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    const auto status=BCryptHash(provider,nullptr,0,payload+p.rva,p.size,digest,sizeof(digest));
    BCryptCloseAlgorithmProvider(provider,0);
    return status>=0 && !std::memcmp(digest,p.hash,sizeof(digest));
}
template<unsigned N,unsigned M>
Leaf copy(unsigned slot,const Profile& p,const Relocation(&relocations)[N],const Operand(&operands)[M],bool frame){
    auto* destination=capsules+slot*4096;
    const unsigned char prologue[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8,0x83,0xEC,0x18,0x56,0x57};
    const unsigned char epilogue[]={0x5F,0x5E,0x8B,0xE5,0x5D,0xC3};
    const unsigned prefix=frame?sizeof(prologue):0;
    if(frame)std::memcpy(destination,prologue,sizeof(prologue));
    auto* code=destination+prefix;
    std::memcpy(code,payload+p.rva,p.size);
    if(frame)std::memcpy(code+p.size,epilogue,sizeof(epilogue));
    for(const auto& relocation:relocations){
        if(relocation.offset==~0u)continue;
        // Internal branches keep their original displacement. All external
        // rel32 calls go to the original native callee, except the copied
        // ground-entity getter whose stale operand is repaired separately.
        const auto target=relocation.target==0x369540?reinterpret_cast<unsigned>(capsules+4*4096):base+relocation.target;
        const unsigned relative=target-reinterpret_cast<unsigned>(code+relocation.offset+4);
        std::memcpy(code+relocation.offset,&relative,4);
    }
    for(const auto& operand:operands){
        if(operand.offset==~0u)continue;
        unsigned actual=0;std::memcpy(&actual,code+operand.offset,4);
        if(actual!=operand.expected)return nullptr;
        const auto address=reinterpret_cast<unsigned>(&(offsets.*(operand.field)));
        std::memcpy(code+operand.offset,&address,4);
    }
    return reinterpret_cast<Leaf>(destination);
}
bool checked_prepare(unsigned char* image,const Fields& fields){
    if(payload || image!=reinterpret_cast<unsigned char*>(base))return false;
    if(!fields.water || !fields.flags || !fields.velocity || !fields.ground)return false;
    payload=image;
    for(const auto& p:guards)if(!matches(p)){payload=nullptr;return false;}
    offsets=fields;
    capsules=static_cast<unsigned char*>(VirtualAlloc(nullptr,5*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!capsules){payload=nullptr;return false;}
    const auto ground=copy(4,ground_profile,ground_calls,ground_operands,false);
    bunnyhop=copy(0,bhop_profile,bhop_calls,bhop_operands,true);
    duck=copy(1,duck_profile,duck_calls,duck_operands,true);
    strafe=copy(2,strafe_profile,strafe_calls,strafe_operands,false);
    stop=copy(3,stop_profile,stop_calls,stop_operands,true);
    DWORD old=0;
    if(!ground || !bunnyhop || !duck || !strafe || !stop ||
       !VirtualProtect(capsules,5*4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),capsules,5*4096)){
        VirtualFree(capsules,0,MEM_RELEASE);capsules=nullptr;payload=nullptr;return false;
    }
    return true;
}
}
bool prepare(unsigned char* image,const Fields& fields) noexcept {
    __try {return checked_prepare(image,fields);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void run(void* command,void* engine,void* entities){
    if(!capsules || !command || at<void*>(0x4DE174)!=engine)return;
    // These are the verified public interface slots consumed by native A06C0
    // and369540. Preserve an established different owner instead of guessing.
    if(at<void*>(0x4DE170) && at<void*>(0x4DE170)!=entities)return;
    at<void*>(0x4DE170)=entities;
    auto* bytes=static_cast<unsigned char*>(command);
    const int number=*reinterpret_cast<int*>(bytes+4);
    if(number<=0)return;
    if(at<unsigned char>(0x7DBEFE) && at<unsigned char>(0x519560))return;
    const auto saved_command=at<void*>(0x4DE214),saved_local=at<void*>(0x4DE23C);
    at<void*>(0x4DE214)=command;
    __try {
        auto* record=reinterpret_cast<unsigned char*(__cdecl*)(int)>(payload+0x3748D0)(number);
        std::memcpy(record+0x14,bytes+0xC,12);
        duck();
        bunnyhop();
        // F92D0's native slow-walk bind, enable/water/speed/ground gates and
        // original math run intact. No replacement feature calculation.
        strafe();
        stop();
        // Preserve F7A50's optional special-mode dependency: unsupported
        // nonzero state is refused instead of inventing its old Source operand.
        if(!at<unsigned char>(0x7DBEFE) || !at<unsigned char>(0x519560))
            reinterpret_cast<int(__cdecl*)(const float*,const float*,void*)>(payload+0xF7A50)(
                reinterpret_cast<float*>(bytes+0xC),reinterpret_cast<float*>(record+0x14),nullptr);
    } __finally {
        at<void*>(0x4DE214)=saved_command;
        at<void*>(0x4DE23C)=saved_local;
    }
}
}
