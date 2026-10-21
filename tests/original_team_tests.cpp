// Native original36B2B0 executes all classification. Networkable fixtures are
// isolated dependency models, never linked into a gameplay implementation.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/menu_profile_data.h"
#include "../primordial_loader/src/original_team.h"
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
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::array<unsigned char,0x11000> first{},second{},observed{};
std::array<unsigned char,0x2000> resource{};
bool first_dormant=false,second_dormant=false;
int first_index=1,second_index=2;
bool local_present=true,observer_present=true;
int __fastcall local_index(void*,void*){return 1;}
void* __fastcall get_entity(void*,void*,int index){return local_present&&index==1?first.data():nullptr;}
void* __fastcall get_handle(void*,void*,unsigned handle){return observer_present&&handle==42?observed.data():nullptr;}
bool __fastcall dormant(void* network,void*){
    return network==first.data()+8?first_dormant:network==second.data()+8?second_dormant:false;
}
int __fastcall index(void* network,void*){
    return network==first.data()+8?first_index:network==second.data()+8?second_index:3;
}
void set(std::array<unsigned char,0x11000>& entity,unsigned offset,int value){
    std::memcpy(entity.data()+offset,&value,4);
}
void resource_team(unsigned index,int value){std::memcpy(resource.data()+0xF38+4*index,&value,4);}
DWORD fault_code=0;
int capture(EXCEPTION_POINTERS* info){fault_code=info->ExceptionRecord->ExceptionCode;return EXCEPTION_EXECUTE_HANDLER;}
bool execute(void* entity,void* other,const original_team::Bindings& b,bool* result){
    __try{*result=original_team::compare(entity,other,b);return true;}
    __except(capture(GetExceptionInformation())){return false;}
}
bool execute_enemy(void* entity,bool direct,const original_team::Bindings& b,bool* result){
    __try{*result=original_team::enemy(entity,direct,b);return true;}
    __except(capture(GetExceptionInformation())){return false;}
}
void check_restored(){
    require(at<unsigned>(0x4DE1B0)==0x12345678&&at<int>(0x4DE310)==123
        &&at<int>(0x4DE314)==456&&at<unsigned>(0x4DE318)==0xAABBCCDD,"scoped globals not restored");
    require(at<unsigned>(0x4DE174)==0x87654321&&at<unsigned>(0x4DE170)==0x12344321
        &&at<unsigned>(0x4DE23C)==0x11224488,"original local/interface globals not restored");
}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");
        std::memcpy(image,raw.data(),raw.size());
        const original_team::Fields fields{0xF38,0xF4,0x10410,0x25F,0x1140,0x3388,0x339C};
        require(!original_team::prepare(image,{0,0xF4,0x10410}),"invalid offset accepted");
        for(unsigned rva:{0x36B2B0u,0x36ACC0u,0xA06C0u,0x36ABF0u}){
            image[rva]^=1;require(!original_team::prepare(image,fields),"modified native dependency accepted");image[rva]^=1;
        }
        require(original_team::prepare(image,fields),"native team preparation failed");
        std::array<void*,11> methods{};methods[9]=reinterpret_cast<void*>(&dormant);methods[10]=reinterpret_cast<void*>(&index);
        void* table=methods.data();std::memcpy(first.data()+8,&table,4);std::memcpy(second.data()+8,&table,4);std::memcpy(observed.data()+8,&table,4);
        void* resource_object=resource.data();unsigned cases=0;
        // Inputs explicitly exercise original branches. They are dependency
        // fixtures, not values supplied by a production feature adapter.
        for(unsigned scenario=0;scenario<12;++scenario){
            set(first,0xF4,2);set(second,0xF4,2);set(first,0x10410,7);set(second,0x10410,7);
            first_index=1;second_index=2;first_dormant=second_dormant=false;
            resource.fill(0);resource_team(1,3);resource_team(2,2);resource_object=resource.data();
            original_team::Bindings b{&resource_object,0,0,false};
            void* other=second.data();bool expected=false;
            switch(scenario){
                case 0:other=nullptr;break;
                case 1:second_index=1;set(second,0xF4,3);break;
                case 2:break; // Same normal team.
                case 3:set(second,0xF4,3);expected=true;break;
                case 4:first_dormant=true;expected=true;break;
                case 5:second_dormant=true;set(first,0xF4,3);expected=true;break;
                case 6:first_dormant=second_dormant=true;expected=true;break;
                case 7:first_dormant=true;b.resource_slot=nullptr;break;
                case 8:b.game_type=6;break;
                case 9:b.game_type=6;set(second,0x10410,8);expected=true;break;
                case 10:b.game_type=6;set(first,0x10410,-1);expected=true;break;
                case 11:b.teammates_are_enemies=true;expected=true;break;
            }
            at<unsigned>(0x4DE1B0)=0x12345678;at<int>(0x4DE310)=123;at<int>(0x4DE314)=456;at<unsigned>(0x4DE318)=0xAABBCCDD;
            at<unsigned>(0x4DE174)=0x87654321;at<unsigned>(0x4DE170)=0x12344321;at<unsigned>(0x4DE23C)=0x11224488;
            bool result=false;require(execute(first.data(),other,b,&result),"native team comparison unexpectedly faulted");
            require(result==expected,"native team decision mismatch");check_restored();++cases;
        }
        // Non-null address of a NULL resource object is distinct from a NULL
        // slot address: native code dereferences it without an object guard.
        first_dormant=true;second_dormant=false;resource_object=nullptr;
        original_team::Bindings b{&resource_object,0,0,false};bool result=false;fault_code=0;
        require(!execute(first.data(),second.data(),b,&result)&&fault_code==EXCEPTION_ACCESS_VIOLATION,"native resource indirection fault changed");
        check_restored();++cases;
        std::array<void*,13> engine_methods{};engine_methods[12]=reinterpret_cast<void*>(&local_index);
        std::array<void*,5> entity_methods{};entity_methods[3]=reinterpret_cast<void*>(&get_entity);entity_methods[4]=reinterpret_cast<void*>(&get_handle);
        void* engine_table=engine_methods.data();void* entity_table=entity_methods.data();
        b.engine=&engine_table;b.entities=&entity_table;
        for(unsigned scenario=0;scenario<10;++scenario){
            local_present=observer_present=true;first_dormant=second_dormant=false;
            set(first,0xF4,2);set(second,0xF4,3);set(observed,0xF4,3);
            set(first,0x25F,1);set(first,0x3388,4);set(first,0x339C,42);
            resource_object=nullptr;bool direct=false,expected=false;void* target=second.data();
            switch(scenario){
                case 0:direct=true;expected=true;break;
                case 1:set(first,0x25F,0);expected=true;break;
                case 2:break; // Dead mode4 compares observed teammate.
                case 3:set(first,0x3388,5);break;
                case 4:observer_present=false;expected=true;break;
                case 5:set(first,0x339C,-1);expected=true;break;
                case 6:set(first,0x3388,3);expected=true;break;
                case 7:local_present=false;break;
                case 8:target=nullptr;break;
                case 9:first_dormant=true;resource_object=resource.data();resource[0x1141]=1;resource_team(1,2);expected=true;break;
            }
            require(execute_enemy(target,direct,b,&result),"original observer helper unexpectedly faulted");
            require(result==expected,"original observer-aware decision mismatch");check_restored();++cases;
        }
        require(!std::memcmp(image+0x36B2B0,raw.data()+0x36B2B0,0x10C),"payload original team routine was modified");
        require(!std::memcmp(image+0x36ACC0,raw.data()+0x36ACC0,0x13D)
            &&!std::memcmp(image+0xA06C0,raw.data()+0xA06C0,0x37)
            &&!std::memcmp(image+0x36ABF0,raw.data()+0x36ABF0,0x66),"payload original observer/local/alive helpers were modified");
        std::printf("Original native team comparison: %u branch/indirection/restoration cases pass.\n",cases);
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
