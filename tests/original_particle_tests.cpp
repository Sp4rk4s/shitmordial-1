// Complete original callbacks with isolated Source argument fixtures. No fixture
// or substitute feature code from this executable is linked into production.
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
std::array<unsigned char,256> initializer{},collection{},parent{};
std::array<unsigned char,512> definition{},parent_definition{};
std::array<float,6> observed{};
unsigned observed_edx=0,calls=0;bool block=false;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(image+rva);}
template<class T,size_t N>T& field(std::array<unsigned char,N>& object,unsigned offset){return *reinterpret_cast<T*>(object.data()+offset);}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int forward(void* object,unsigned edx,void* particles,unsigned a,unsigned b,unsigned c,unsigned d){
    require(object==initializer.data()&&particles==collection.data(),"original receiver/collection forwarding differs");
    require(a==11&&b==22&&c==33&&d==44,"original stack forwarding differs");
    observed_edx=edx;std::memcpy(observed.data(),initializer.data()+0x5C,24);++calls;return 0x12345678;
}
int __fastcall single(void* o,unsigned edx,void* p,unsigned a,unsigned b,unsigned c,unsigned d){return forward(o,edx,p,a,b,c,d);}
int __fastcall batch(void* o,unsigned edx,void* p,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){require(e==55,"original sixth argument differs");return forward(o,edx,p,a,b,c,d);}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump path required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity differs");
        image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");std::memcpy(image,raw.data(),raw.size());
        at<void*>(0x7FB8F8)=reinterpret_cast<void*>(&single);at<void*>(0x7FB8FC)=reinterpret_cast<void*>(&batch);
        const std::array<float,6> baseline{.1f,.2f,.3f,.4f,.5f,.6f};
        const std::array<float,6> fire{1,0,128.0f/255,1,0,128.0f/255};
        const std::array<float,6> smoke{0,1,64.0f/255,0,1,64.0f/255};
        struct Case {const char* name;const char* parent;bool fire;int smoke_mode;int count;unsigned expected;};
        const Case cases[]={
            {"molotov_groundfire",nullptr,true,0,1,1},
            {"molotov_groundfire",nullptr,false,0,1,0},
            {"unrelated_particle",nullptr,true,2,1,0},
            {"molotov_groundfire",nullptr,true,0,0,0},
            {"molotov_groundfire",nullptr,true,0,4,1},
            {"child", "molotov_groundfire",true,0,1,1},
            {"molotov_smoking_ground_child01","molotov_groundfire",true,0,1,0},
            {"explosion_smokegrenade",nullptr,false,0,1,0},
            {"explosion_smokegrenade",nullptr,false,1,1,2},
            {"explosion_smokegrenade_fallback",nullptr,true,2,4,2},
            {"child","explosion_smokegrenade",false,1,1,2},
            {"explosion_smokegrenade",nullptr,false,1,0,0},
        };
        for(unsigned kind=0;kind<2;++kind)for(const auto& c:cases){
            initializer.fill(0xA5);collection.fill(0);parent.fill(0);definition.fill(0);parent_definition.fill(0);
            field<const char*>(definition,0x134)=c.name;field<void*>(collection,0x48)=definition.data();field<int>(collection,0x30)=c.count;
            if(c.parent){field<const char*>(parent_definition,0x134)=c.parent;field<void*>(parent,0x48)=parent_definition.data();field<void*>(collection,0x88)=parent.data();}
            std::memcpy(initializer.data()+0x5C,baseline.data(),24);const auto original=initializer;
            at<unsigned char>(0x5390C8)=c.fire;at<int>(0x537420)=c.smoke_mode;
            const unsigned char fire_rgb[]={255,0,128},smoke_rgb[]={0,255,64};
            std::memcpy(image+0x539328,fire_rgb,3);std::memcpy(image+0x5376A8,smoke_rgb,3);
            using Single=int(__fastcall*)(void*,unsigned,void*,unsigned,unsigned,unsigned,unsigned);
            using Batch=int(__fastcall*)(void*,unsigned,void*,unsigned,unsigned,unsigned,unsigned,unsigned);
            const auto result=kind?reinterpret_cast<Batch>(image+0x155C30)(initializer.data(),0x87654321,collection.data(),11,22,33,44,55):
                                   reinterpret_cast<Single>(image+0x155F40)(initializer.data(),0x87654321,collection.data(),11,22,33,44);
            require(result==0x12345678&&observed_edx==0x87654321,"original return/EDX forwarding differs");
            require(observed==(c.expected==1?fire:c.expected==2?smoke:baseline),"original native particle decision/RGB differs");
            require(initializer==original,"original initializer RGB restoration differs");
        }
        require(calls==24,"original callback count differs");
        for(const auto range:{std::pair{0x155F40u,0x307u},std::pair{0x155C30u,0x30Au},std::pair{0x6CF70u,0x39u}})
            require(!std::memcmp(image+range.first,raw.data()+range.first,range.second),"original particle/helper bytes changed");
        std::puts("Original particle callbacks: 24 fire/smoke/parent/exclusion/ABI/restoration cases pass; native code unchanged.");
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
