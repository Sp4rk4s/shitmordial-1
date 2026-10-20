// Differential test: the recovered original bunnyhop decision executes as x86
// in an isolated test process, with explicit local entity/offset fixtures.
#include "../primordial_loader/src/movement_features.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void word(unsigned char* where,unsigned expected,unsigned replacement){
    unsigned actual=0;std::memcpy(&actual,where,4);require(actual==expected,"excerpt operand profile changed");
    std::memcpy(where,&replacement,4);
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"original dump required");
        constexpr unsigned start=0xF8674,length=0xF870E-start;
        std::ifstream file(argv[1],std::ios::binary);file.seekg(start);
        std::array<unsigned char,length> source{};file.read(reinterpret_cast<char*>(source.data()),source.size());
        require(file.good(),"original movement bytes unavailable");
        auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(code!=nullptr,"excerpt allocation failed");
        std::memcpy(code,source.data(),length);code[length]=0xC3;
        std::array<unsigned char,4096> entity{};
        std::array<unsigned char,100> command{};
        unsigned command_pointer=reinterpret_cast<unsigned>(command.data()),flags_offset=0x104,water_offset=0x25E;
        unsigned char enabled=0,jump_bypass=0;
        word(code+2,0x4487D220,reinterpret_cast<unsigned>(&enabled));
        word(code+0x19,0x2D9CB32C,reinterpret_cast<unsigned>(&water_offset));
        for(unsigned offset:{0x2Fu,0x87u})word(code+offset,0x2D8F778B,reinterpret_cast<unsigned>(&flags_offset));
        for(unsigned offset:{0x3Fu,0x56u})word(code+offset,0x44B1BBDC,reinterpret_cast<unsigned>(&jump_bypass));
        for(unsigned offset:{0x47u,0x92u})word(code+offset,0x4481E214,reinterpret_cast<unsigned>(&command_pointer));
        // Explicit model for already-reversed refresh_cached_local_player:
        // this fixture always returns the one synthetic local entity.
        code[0x200]=0xB8;const auto player=reinterpret_cast<unsigned>(entity.data());
        std::memcpy(code+0x201,&player,4);code[0x205]=0xC3;
        for(unsigned offset:{0x12u,0x28u,0x80u}){
            require(code[offset]==0xE8,"lookup call profile changed");
            const auto destination=static_cast<unsigned>(0x44340000+start+offset+5+*reinterpret_cast<int*>(code+offset+1));
            require(destination==0x443E06C0,"lookup is not the verified native local getter");
            const int relative=0x200-static_cast<int>(offset+5);std::memcpy(code+offset+1,&relative,4);
        }
        require(FlushInstructionCache(GetCurrentProcess(),code,4096)!=FALSE,"cache flush failed");
        const unsigned flags_values[]={0,1,0x40,0x41,0x200};
        unsigned cases=0;
        for(unsigned on=0;on<2;++on)for(unsigned flags:flags_values)for(unsigned water=0;water<4;++water)
        for(unsigned buttons:{0u,2u,3u,0x402u}){
            enabled=static_cast<unsigned char>(on);jump_bypass=0;
            std::memcpy(entity.data()+flags_offset,&flags,4);entity[water_offset]=static_cast<unsigned char>(water);
            std::memcpy(command.data()+0x30,&buttons,4);
            reinterpret_cast<void(__cdecl*)()>(code)();
            unsigned actual=0;std::memcpy(&actual,command.data()+0x30,4);
            const movement_features::Player state{100,flags,0,static_cast<unsigned char>(water),2};
            require(actual==movement_features::apply(buttons,state,on!=0,false),"movement bridge differs from original bunnyhop decision");
            ++cases;
        }
        VirtualFree(code,0,MEM_RELEASE);
        std::printf("PASS: %u original-x86 bunnyhop decisions agree; all models local to test process\n",cases);
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
