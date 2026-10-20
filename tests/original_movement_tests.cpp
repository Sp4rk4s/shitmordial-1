// Actual production capsules and original callees, with only public Source
// interface methods represented by explicit isolated test fixtures.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/imports.h"
#include "../primordial_loader/src/original_movement.h"
#include "../primordial_loader/src/menu_profile_data.h"
#include "../shared/mapping.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::array<unsigned char,4096> player{};
unsigned local_calls=0,entity_calls=0,ground_calls=0;
int __fastcall local_index(void*,void*){++local_calls;return 1;}
void* __fastcall get_entity(void*,void*,int index){++entity_calls;return index==1?player.data():nullptr;}
void* __fastcall from_handle(void*,void*,unsigned handle){++ground_calls;return handle==1?player.data():nullptr;}
int report(EXCEPTION_POINTERS* e){
    std::printf("FAIL native exception=%08lX eip=%08lX operand=%08lX\n",e->ExceptionRecord->ExceptionCode,e->ContextRecord->Eip,
        e->ExceptionRecord->NumberParameters>=2?e->ExceptionRecord->ExceptionInformation[1]:0);
    return EXCEPTION_EXECUTE_HANDLER;
}
bool run(void* command,void* engine,void* entities){
    __try {original_movement::run(command,engine,entities);return true;}
    __except(report(GetExceptionInformation())){return false;}
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE && mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        auto* image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");
        std::memcpy(image,raw.data(),raw.size());const auto bound=imports::apply_recovered(image,raw.size());require(bound.ok,bound.error.c_str());
        // The production profile must refuse a mismatching original function.
        image[0xF92D0]^=1;
        require(!original_movement::prepare(image,{0x25E,0x104,0x114,0x150}),"modified original code was accepted");
        image[0xF92D0]^=1;
        require(original_movement::prepare(image,{0x25E,0x104,0x114,0x150}),"original profile rejected");
        std::array<void*,27> engine_methods{};engine_methods[12]=reinterpret_cast<void*>(&local_index);
        std::array<void*,5> entity_methods{};entity_methods[3]=reinterpret_cast<void*>(&get_entity);entity_methods[4]=reinterpret_cast<void*>(&from_handle);
        void* engine_table=engine_methods.data();void* entity_table=entity_methods.data();
        void* engine=&engine_table;void* entities=&entity_table;
        std::memcpy(image+0x4DE174,&engine,4);std::memset(image+0x4DE170,0,4);
        image[0x7DBEFE]=0;image[0x519560]=0;image[0x53F5E8]=0;
        std::array<unsigned char,100> command{};
        unsigned cases=0;
        for(unsigned scenario=0;scenario<17;++scenario){
            command.fill(0);player.fill(0);image[0x7DBBDC]=0;image[0x7DBBDD]=0;
            image[0x7DBEFE]=0;image[0x519560]=0;image[0x53F5E8]=0;
            const unsigned never=4;std::memcpy(image+0x53D700+0x134,&never,4);std::memcpy(image+0x5192B0+0x134,&never,4);
            for(unsigned rva:{0x53D220u,0x53DD58u,0x53D360u,0x53DC18u})image[rva]=0;
            const int number=149+scenario;std::memcpy(command.data()+4,&number,4);
            const float speed[3]={250,0,0};std::memcpy(player.data()+0x114,speed,12);
            unsigned buttons=2,flags=0,handle=0;
            if(scenario<=3){image[0x53D220]=1;if(scenario==1)flags=1;if(scenario==2)player[0x25E]=2;if(scenario==3)flags=0x40;}
            if(scenario==4)image[0x53DD58]=1;
            if(scenario>=5 && scenario<=7){image[0x53D360]=1;const float smooth=75;std::memcpy(image+0x53D498,&smooth,4);if(scenario==6)flags=1;if(scenario==7)buttons|=0x20000;}
            if(scenario>=8 && scenario<=11){image[0x53DC18]=1;buttons=0;flags=1;handle=scenario==9?0:1;if(scenario==10)buttons=8;if(scenario==11){const float slow[3]={1,0,0};std::memcpy(player.data()+0x114,slow,12);}}
            if(scenario==13){image[0x53D360]=1;const unsigned always=3;std::memcpy(image+0x53D700+0x134,&always,4);}
            if(scenario==14){image[0x53DD58]=1;image[0x53F5E8]=1;}
            if(scenario==15){const unsigned always=3;std::memcpy(image+0x5192B0+0x134,&always,4);}
            if(scenario==16){image[0x53D360]=1;image[0x7DBEFE]=1;image[0x519560]=1;}
            std::memcpy(command.data()+0x30,&buttons,4);std::memcpy(player.data()+0x104,&flags,4);std::memcpy(player.data()+0x150,&handle,4);
            // Ensure scoped native globals are restored, not leaked to UI.
            const unsigned saved_command=0x12345678,saved_local=0x23456789;
            std::memcpy(image+0x4DE214,&saved_command,4);std::memcpy(image+0x4DE23C,&saved_local,4);
            require(run(command.data(),engine,entities),"native integration failed");
            require(!std::memcmp(image+0x4DE214,&saved_command,4) && !std::memcmp(image+0x4DE23C,&saved_local,4),"scoped native state leaked");
            unsigned result=0;float forward=0,side=0;std::memcpy(&result,command.data()+0x30,4);std::memcpy(&forward,command.data()+0x24,4);std::memcpy(&side,command.data()+0x28,4);
            require(std::isfinite(forward) && std::isfinite(side),"nonfinite production movement");
            if(scenario==0)require(!(result&2),"original bunnyhop did not clear airborne jump");
            if(scenario>=1 && scenario<=3)require(result&2,"original bunnyhop gate failed");
            if(scenario==4)require(result&0x400000,"original duck block failed");
            if(scenario==5)require(side>440 && forward>20,"original strafe/correction integration failed");
            if(scenario==6 || scenario==7)require(forward==0 && side==0,"original strafe gate failed");
            if(scenario==8)require(forward<0,"original fast-stop did not oppose velocity");
            if(scenario>=9 && scenario<=11)require(forward==0 && side==0,"original fast-stop gate failed");
            if(scenario==13)require(forward==0 && side==0,"native slow-walk bind gate failed");
            if(scenario==14)require(!(result&0x400000),"native duck suppression gate failed");
            if(scenario==15)require(result&0x400000,"native duck binding override failed");
            if(scenario==16)require(forward==0 && side==0,"unresolved special-mode state was not refused");
            std::printf("PASS native scenario=%u command=%d buttons=%X forward=%.9g side=%.9g\n",scenario,number,result,forward,side);++cases;
        }
        require(local_calls>0 && entity_calls>0 && ground_calls>0,"original public Source lookup path was not exercised");
        require(!std::memcmp(image+0xF92D0,raw.data()+0xF92D0,0x49D),"original mapped code was modified");
        std::printf("PASS %u production-capsule scenarios; original Source getters/bind getter/math retained\n",cases);
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
