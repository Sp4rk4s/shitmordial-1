// Executes the intact32BA40 coordinator. External UI/subsystem calls are
// explicit isolated fixtures; none of these dependencies are linked into DLLs.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/menu_profile_data.h"
#include "../shared/mapping.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace {
unsigned char* image=nullptr;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(image+rva);}
std::vector<unsigned> events;
std::array<std::array<unsigned char,256>,4> windows{};
std::array<void*,4> pointers{};
std::array<float,8> timing{};
std::array<unsigned,3> configs{};
unsigned syncs=0,refreshes=0,alternates=0;
bool mark_visible=true;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
unsigned index(void* p){for(unsigned i=0;i<4;++i)if(p==pointers[i])return i;throw std::runtime_error("unknown window");}
void __fastcall update(void* object,void*){events.push_back(10+index(object));}
void __fastcall draw(void* object,void*){
    events.push_back(20+index(object));
    if(mark_visible){at<unsigned char>(0x804DF4)=1;at<unsigned char>(0x804DF1)=1;}
}
void __cdecl sync(){++syncs;}
void __cdecl refresh(){++refreshes;}
void __cdecl alternate(){++alternates;}
void* __cdecl config_vector(){return configs.data();}
unsigned __stdcall cursor(void*){return 1;}
void patch(unsigned rva,void* function){
    image[rva]=0xE9;
    const auto relative=reinterpret_cast<unsigned>(function)-reinterpret_cast<unsigned>(image+rva+5);
    std::memcpy(image+rva+1,&relative,4);
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");std::memcpy(image,raw.data(),raw.size());
        patch(0x3504B0,reinterpret_cast<void*>(&update));patch(0x351AB0,reinterpret_cast<void*>(&draw));
        patch(0xE3EF0,reinterpret_cast<void*>(&sync));patch(0x2CAB10,reinterpret_cast<void*>(&config_vector));
        for(unsigned rva:{0x2CBBB0,0x14DB50,0x14B330,0x164580})patch(rva,reinterpret_cast<void*>(&refresh));
        for(unsigned rva:{0x2DF710,0x2DFD20})patch(rva,reinterpret_cast<void*>(&alternate));
        patch(0xA4F992,reinterpret_cast<void*>(&cursor));
        for(unsigned i=0;i<4;++i)pointers[i]=windows[i].data();
        for(unsigned scenario=0;scenario<7;++scenario){
            std::memset(image+0x804DF0,0,0x700);std::memset(image+0x807F08,0,114*4);
            events.clear();syncs=refreshes=alternates=0;
            at<unsigned char>(0x804DF0)=1; // Already centered by an earlier native frame.
            at<void*>(0x804E5C)=pointers.data();at<void*>(0x804E60)=pointers.data()+4;
            at<void*>(0x804DF8)=pointers[1];at<void*>(0x4DE164)=timing.data();
            at<unsigned>(0x5407DC)=0;at<unsigned>(0x804E0C)=0;
            at<unsigned>(0x5404E0)=72;at<unsigned>(0x5404E4)=0;
            mark_visible=scenario!=4;timing[0]=scenario==1?2.0f:0.0f;
            if(scenario==2){at<unsigned>(0x807F08+83*4)=2;at<unsigned>(0x807F08+16*4)=1;}
            if(scenario==3){at<unsigned char>(0x804E07)=1;at<unsigned>(0x807F08+70*4)=1;}
            if(scenario==5)at<unsigned>(0x804E34)=1;
            if(scenario==6)at<unsigned char>(0x804E30)=1;
            reinterpret_cast<void(__cdecl*)()>(image+0x32BA40)();
            if(scenario>=5){require(alternates==1&&events.empty()&&syncs==0,"alternate UI dispatch changed");continue;}
            const std::vector<unsigned> expected{10,20,12,22,13,23,11,21};
            require(events==expected,"native focus-last ordering changed");require(syncs==1,"native per-frame sync not reached");
            require(refreshes==(scenario==1?4u:0u),"native periodic refresh gating changed");
            require(at<unsigned char>(0x804DF3)==1,"cursor visibility result not preserved");
            require(at<unsigned char>(0x80869B)==static_cast<unsigned char>(mark_visible),"native capture publication changed");
            require(at<unsigned char>(0x804E07)==static_cast<unsigned char>(scenario==2),"native search shortcuts changed");
            require(at<unsigned>(0x4B344C)==72,"native toggle button binding changed");
            require(!std::memcmp(image+0x32BA40,raw.data()+0x32BA40,0x6E5),"original coordinator instructions changed");
        }
        std::puts("Original full menu frame: seven ordering/search/capture/refresh/alternate cases pass; frame bytes unchanged.");
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
