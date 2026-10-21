// Tests original feature instructions with an isolated Source ConVar receiver.
#include "../primordial_loader/src/original_visual_cvars.h"
#include "../primordial_loader/src/config.h"
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
void* receiver=nullptr;float observed=0;unsigned calls=0;
std::vector<std::pair<void*,int>> integer_calls;
void __fastcall set_float(void* object,void*,float value){receiver=object;observed=value;++calls;}
void __fastcall set_integer(void* object,void*,int value){integer_calls.emplace_back(object,value);}
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        auto* image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");std::memcpy(image,raw.data(),raw.size());
        for(unsigned rva:{0x14B884u,0x14B914u,0x14B994u}){image[rva]^=1;require(!original_visual_cvars::prepare(image),"changed original leaf accepted");image[rva]^=1;}
        image[0x412EC4]^=1;require(!original_visual_cvars::prepare(image),"changed native scale accepted");image[0x412EC4]^=1;
        require(original_visual_cvars::prepare(image),"original leaf preparation failed");
        std::array<void*,16> methods{};methods[15]=reinterpret_cast<void*>(&set_float);void* table=methods.data();
        const std::array<std::array<float,2>,6> cases{{{0,0},{100,1},{125,1.25f},{150,1.5f},{250,2.5f},{-50,-.5f}}};
        for(const auto& c:cases){
            std::memcpy(image+0x533FA8,&c[0],4);original_visual_cvars::aspect(&table);
            require(receiver==&table&&std::fabs(observed-c[1])<.000001f,"native scale/receiver/slot/argument contract changed");
        }
        require(calls==cases.size(),"native Source setter count differs");
        std::array<void*,17> integer_methods{};integer_methods[16]=reinterpret_cast<void*>(&set_integer);
        void* blood=integer_methods.data();void* decals=integer_methods.data();
        for(unsigned state=0;state<4;++state){
            image[0x537E28]=static_cast<unsigned char>(state&1);image[0x537CE8]=static_cast<unsigned char>(state&2);
            integer_calls.clear();original_visual_cvars::removals(&blood,&decals);
            require(integer_calls==std::vector<std::pair<void*,int>>{{&decals,state&2?0:1},{&blood,state&1?0:1}},"native removal gates/ABI/call ordering differs");
        }
        require(!std::memcmp(image+0x14B884,raw.data()+0x14B884,0x27)&&!std::memcmp(image+0x14B914,raw.data()+0x14B914,0x1D)&&!std::memcmp(image+0x14B994,raw.data()+0x14B994,0x1D),"payload original leaf modified");
        std::puts("Original visual ConVars: six aspect and four removal/ABI/order cases plus source/constant guards pass; native code unchanged.");
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
