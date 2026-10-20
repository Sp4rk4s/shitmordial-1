#include "../primordial_loader/src/event_pattern.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"original dump required");
        // Execute the original x86 scanner, including its output pointer/RET12
        // convention, rather than testing only a replacement search algorithm.
        std::ifstream file(argv[1],std::ios::binary);file.seekg(0x155840);
        std::array<unsigned char,0x8C> bytes{};file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
        require(file.good(),"scanner bytes unavailable");
        auto* executable=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
        require(executable!=nullptr,"native scanner fixture allocation failed");
        std::memcpy(executable,bytes.data(),bytes.size());
        require(FlushInstructionCache(GetCurrentProcess(),executable,bytes.size())!=FALSE,"cache flush failed");
        using Scan=std::uint32_t*(__thiscall*)(const event_pattern::Pattern*,std::uint32_t*,const std::uint8_t*,std::uint32_t);
        const auto scan=reinterpret_cast<Scan>(executable);
        event_pattern::Pattern original{},adapted{};
        std::memcpy(original.bytes,event_pattern::old_bytes,18);original.padding[0]=0xA5;
        std::array<std::uint8_t,96> region{};
        std::memcpy(region.data()+7,event_pattern::new_bytes,18);
        const auto start=reinterpret_cast<std::uintptr_t>(region.data()),end=start+region.size();
        std::uint32_t result=0xFFFFFFFF;
        require(scan(&original,&result,region.data(),region.size())==&result && result==0,"stale signature unexpectedly matched");
        require(event_pattern::adapt(original,start,region.size(),start,end,adapted),"valid linked pattern refused");
        require(scan(&adapted,&result,region.data(),region.size())==&result && result==start+7,"adapted signature/native ABI failed");
        require(!std::memcmp(original.bytes,event_pattern::old_bytes,18) && adapted.padding[0]==0xA5,"input or padding modified");
        auto unchanged=adapted;
        original.mask=2;
        require(!event_pattern::adapt(original,start,96,start,end,adapted),"masked input adapted");
        original.mask=0;original.bytes[17]^=1;
        require(!event_pattern::adapt(original,start,96,start,end,adapted),"different consumer signature adapted");
        original.bytes[17]^=1;
        require(!event_pattern::adapt(original,start-1,96,start,end,adapted),"other module accepted");
        require(!event_pattern::adapt(original,start,97,start,end,adapted),"cross-module range accepted");
        require(!event_pattern::adapt(original,end,1,start,end,adapted),"end pointer accepted");
        require(!event_pattern::adapt(original,start,0,start,end,adapted),"empty range accepted");
        require(!event_pattern::adapt(original,0xFFFFFFF0,64,start,end,adapted),"wrapping range accepted");
        require(!std::memcmp(&unchanged,&adapted,sizeof(adapted)),"rejected input changed output");
        region.fill(0);std::memcpy(region.data()+region.size()-18,event_pattern::new_bytes,18);
        require(scan(&adapted,&result,region.data(),region.size())==&result && result==0,"original excluded-end behavior changed");
        VirtualFree(executable,0,MEM_RELEASE);
        std::puts("PASS: linked replacement, original x86 RET12/output ABI, input isolation and range rejection");
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
