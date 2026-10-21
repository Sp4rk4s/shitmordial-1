// Runs the unchanged primordial.bin projection helper. The Source interface
// fixture is deliberately confined to this executable, never linked into a DLL.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/menu_profile_data.h"
#include "../shared/mapping.h"
#include <windows.h>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
struct Vector {float x,y,z;};
struct Screen {int x,y;unsigned char valid,padding[3];};
static_assert(sizeof(Screen)==12);
struct Overlay {void** table;};
Overlay overlay{};
Vector projected{},received{};
int status=0;unsigned calls=0;
int __fastcall screen_position(Overlay* object,void*,const Vector* input,Vector* output){
    if(object!=&overlay)throw std::runtime_error("original interface this differs");
    received=*input;*output=projected;++calls;return status;
}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump path required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity differs");
        auto* image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");
        std::memcpy(image,raw.data(),raw.size());
        std::array<void*,14> table{};table[13]=reinterpret_cast<void*>(&screen_position);overlay.table=table.data();
        *reinterpret_cast<void**>(image+0x4DE184)=&overlay;
        *reinterpret_cast<int*>(image+0x8087AC)=1920;*reinterpret_cast<int*>(image+0x8087B0)=1080;
        using Project=Screen*(__cdecl*)(Screen*,const Vector*);
        auto project=reinterpret_cast<Project>(image+0x362810);
        const Vector input{12.0f,-34.0f,56.0f};
        struct Case {float x,y;int api_status;bool valid;int expected_x,expected_y;};
        const float nan=std::numeric_limits<float>::quiet_NaN();
        const Case cases[]={
            {100.9f,200.8f,0,true,100,200},
            {100,200,1,false,0,0},{100,200,-1,false,0,0},
            {0,200,0,false,0,0},{100,0,0,false,0,0},
            {1920,200,0,false,0,0},{100,1080,0,false,0,0},
            {-10.9f,-20.8f,0,true,-10,-20},
            {2000,1200,0,true,2000,1200},
            {0.5f,0.5f,0,true,0,0},
            {-0.0f,200,0,false,0,0},
            {nan,200,0,true,INT_MIN,200},
            {100,nan,0,true,100,INT_MIN},
        };
        for(const auto& c:cases){
            projected={c.x,c.y,99};status=c.api_status;
            Screen out{123456,654321,7,{0xA1,0xB2,0xC3}};
            require(project(&out,&input)==&out,"original output-pointer return differs");
            require(out.valid==c.valid,"original validity gate differs");
            require(out.x==(c.valid?c.expected_x:123456)&&out.y==(c.valid?c.expected_y:654321),"original coordinate writes differ");
            require(out.padding[0]==0xA1&&out.padding[1]==0xB2&&out.padding[2]==0xC3,"original padding overwritten");
            require(!std::memcmp(&input,&received,sizeof(input)),"world argument forwarding differs");
        }
        require(calls==std::size(cases),"Source slot13 forwarding count differs");
        require(!std::memcmp(image+0x362810,raw.data()+0x362810,0x95),"original projection instructions changed");
        VirtualFree(image,0,MEM_RELEASE);
        std::puts("Original projection: 13 API/edge/truncation/NaN cases pass; original instructions unchanged.");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
