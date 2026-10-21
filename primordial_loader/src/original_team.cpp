#include "original_team.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <initializer_list>
namespace original_team {
namespace {
struct Operand {unsigned offset,expected;unsigned Fields::*field;};
struct Relocation {unsigned offset,target;};
struct Profile {unsigned rva,size;unsigned char hash[32];};
#include "original_team_profile.inc"
unsigned char* payload=nullptr;
unsigned char* code=nullptr;
Fields offsets{};
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(payload+rva);}
bool checked_prepare(unsigned char* image,const Fields& fields){
    if(payload||image!=reinterpret_cast<unsigned char*>(0x44340000))return false;
    for(unsigned field:{fields.resource_team,fields.team,fields.survival_team,fields.life,
        fields.resource_alive,fields.observer_mode,fields.observer_target})if(!field||field>0x20000)return false;
    BCRYPT_ALG_HANDLE provider=nullptr;unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    bool matches=true;
    for(const auto& profile:profiles){
        const auto status=BCryptHash(provider,nullptr,0,image+profile.rva,profile.size,digest,sizeof(digest));
        if(status<0||std::memcmp(digest,profile.hash,sizeof(digest))){matches=false;break;}
    }
    BCryptCloseAlgorithmProvider(provider,0);
    if(!matches)return false;
    offsets=fields;
    auto* candidate=static_cast<unsigned char*>(VirtualAlloc(nullptr,4*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!candidate)return false;
    for(unsigned index=0;index<4;++index){
      auto* output=candidate+index*4096;
      std::memcpy(output,image+profiles[index].rva,profiles[index].size);
      for(const auto& relocation:relocations[index]){
        if(relocation.offset==~0u)break;
        unsigned target=0;
        for(unsigned i=0;i<4;++i)if(profiles[i].rva==relocation.target)target=reinterpret_cast<unsigned>(candidate+i*4096);
        if(!target){VirtualFree(candidate,0,MEM_RELEASE);return false;}
        const unsigned relative=target-reinterpret_cast<unsigned>(output+relocation.offset+4);
        std::memcpy(output+relocation.offset,&relative,4);
      }
      for(const auto& operand:operands[index]){
        if(operand.offset==~0u)break;
        unsigned actual=0;std::memcpy(&actual,output+operand.offset,4);
        if(actual!=operand.expected){VirtualFree(candidate,0,MEM_RELEASE);return false;}
        const auto address=reinterpret_cast<unsigned>(&(offsets.*operand.field));
        std::memcpy(output+operand.offset,&address,4);
      }
    }
    DWORD old=0;
    if(!VirtualProtect(candidate,4*4096,PAGE_EXECUTE_READ,&old)
        ||!FlushInstructionCache(GetCurrentProcess(),candidate,4*4096)){
        VirtualFree(candidate,0,MEM_RELEASE);return false;
    }
    code=candidate;payload=image;return true;
}
struct Saved {void* resource_slot;int game_type,game_mode;unsigned char teammates;void* engine;void* entities;void* local;};
Saved enter(const Bindings& b){
    Saved saved{at<void*>(0x4DE1B0),at<int>(0x4DE310),at<int>(0x4DE314),at<unsigned char>(0x4DE318),at<void*>(0x4DE174),at<void*>(0x4DE170),at<void*>(0x4DE23C)};
    at<void*>(0x4DE1B0)=b.resource_slot;at<int>(0x4DE310)=b.game_type;
    at<int>(0x4DE314)=b.game_mode;at<unsigned char>(0x4DE318)=b.teammates_are_enemies;
    at<void*>(0x4DE174)=b.engine;at<void*>(0x4DE170)=b.entities;
    return saved;
}
void leave(const Saved& saved){
    at<void*>(0x4DE1B0)=saved.resource_slot;at<int>(0x4DE310)=saved.game_type;
    at<int>(0x4DE314)=saved.game_mode;at<unsigned char>(0x4DE318)=saved.teammates;
    at<void*>(0x4DE174)=saved.engine;at<void*>(0x4DE170)=saved.entities;at<void*>(0x4DE23C)=saved.local;
}
}
bool prepare(unsigned char* image,const Fields& fields) noexcept {
    __try{return checked_prepare(image,fields);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool compare(void* entity,void* other,const Bindings& bindings){
    const auto saved=enter(bindings);bool result=false;
    __try{result=reinterpret_cast<bool(__thiscall*)(void*,void*)>(code)(entity,other);}
    __finally{leave(saved);}
    return result;
}
bool enemy(void* entity,bool direct_local,const Bindings& bindings){
    const auto saved=enter(bindings);bool result=false;
    __try{result=reinterpret_cast<bool(__thiscall*)(void*,bool)>(code+4096)(entity,direct_local);}
    __finally{leave(saved);}
    return result;
}
}
