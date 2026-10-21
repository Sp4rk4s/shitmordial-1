// Executes the original viewmodel-FOV callback. Only external camera/player/
// weapon dependencies are explicit test fixtures. Never injected into a game.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/menu_profile_data.h"
#include "../primordial_loader/src/original_fov.h"
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
float baseline=68;
std::array<unsigned char,256> player{},observer{},weapon{},observer_weapon{};
bool local_present=true,weapon_present=true,dormant=false,target_present=true;
unsigned lookup_calls=0,handle_calls=0,network_calls=0;
float __cdecl original_source_fov(){return baseline;}
int __fastcall local_index(void*,void*){return 1;}
void* __fastcall local_entity(void*,void*,int index){++lookup_calls;return local_present&&index==1?player.data():nullptr;}
void* __fastcall handle_entity(void*,void*,unsigned handle){
    ++handle_calls;
    if(handle==2)return target_present?observer.data():nullptr;
    if(!weapon_present)return nullptr;
    return handle==42?weapon.data():handle==43?observer_weapon.data():nullptr;
}
bool __fastcall is_dormant(void*,void*){++network_calls;return dormant;}
int __fastcall entity_index(void* network,void*){return network==observer.data()+8?2:1;}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int report(EXCEPTION_POINTERS* e){std::printf("native exception=%08lX eip=%08lX\n",e->ExceptionRecord->ExceptionCode,e->ContextRecord->Eip);return EXCEPTION_EXECUTE_HANDLER;}
bool execute(const void* view,const original_fov::Bindings& bindings,float* result){
    __try {*result=original_fov::viewmodel(nullptr,view,bindings);return true;}
    __except(report(GetExceptionInformation())){return false;}
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE && mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        auto* image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base test allocation failed");std::memcpy(image,raw.data(),raw.size());
        std::array<unsigned char,512> view{};std::array<unsigned char,256> resource{};
        const unsigned scoped_offset=0x20,item_offset=0x24,life_offset=0x30,mode_offset=0x34,target_offset=0x38,weapon_offset=0x3C,alive_offset=0x20;
        image[0x14FE90]^=1;
        require(!original_fov::prepare(image,{scoped_offset,item_offset,weapon_offset,life_offset,mode_offset,target_offset,alive_offset}),"modified original callback accepted");
        image[0x14FE90]^=1;
        require(original_fov::prepare(image,{scoped_offset,item_offset,weapon_offset,life_offset,mode_offset,target_offset,alive_offset}),"production original FOV preparation failed");
        std::array<void*,13> engine_methods{};engine_methods[12]=reinterpret_cast<void*>(&local_index);
        std::array<void*,5> entity_methods{};entity_methods[3]=reinterpret_cast<void*>(&local_entity);entity_methods[4]=reinterpret_cast<void*>(&handle_entity);
        std::array<void*,11> network_methods{};network_methods[9]=reinterpret_cast<void*>(&is_dormant);network_methods[10]=reinterpret_cast<void*>(&entity_index);
        void* engine_table=engine_methods.data();void* entity_table=entity_methods.data();void* network_table=network_methods.data();
        void* engine=&engine_table;void* entities=&entity_table;
        std::memcpy(image+0x4DE174,&engine,4);std::memcpy(image+0x4DE170,&entities,4);
        unsigned resource_object=0;const auto resource_slot=reinterpret_cast<unsigned>(&resource_object);
        std::memcpy(image+0x4DE1B0,&resource_slot,4);
        const auto source_fov=reinterpret_cast<unsigned>(&original_source_fov);std::memcpy(image+0x7FB6B8,&source_fov,4);
        const original_fov::Bindings bindings{engine,entities,reinterpret_cast<void**>(&resource_object),reinterpret_cast<void*>(&original_source_fov)};
        unsigned cases=0;
        for(unsigned scenario=0;scenario<20;++scenario){
            player.fill(0);observer.fill(0);weapon.fill(0);observer_weapon.fill(0);view.fill(0);resource.fill(0);
            std::memcpy(player.data()+8,&network_table,4);std::memcpy(observer.data()+8,&network_table,4);
            local_present=scenario!=1;weapon_present=scenario!=3;dormant=scenario==16||scenario==17;target_present=scenario!=19;
            const unsigned view_ptr=scenario==2?0:reinterpret_cast<unsigned>(view.data());
            std::memcpy(image+0x4DE1E0,&view_ptr,4);resource_object=0;
            const unsigned local_handle=42,observed_handle=43;std::memcpy(player.data()+weapon_offset,&local_handle,4);std::memcpy(observer.data()+weapon_offset,&observed_handle,4);
            const unsigned short observed_item=49;std::memcpy(observer_weapon.data()+item_offset,&observed_item,2);
            baseline=68;float camera=90,addition=10;unsigned short item=7;bool scoped=false;float expected=78;
            if(scenario>=1 && scenario<=3)expected=68;
            if(scenario==4){scoped=true;item=39;expected=68;}
            if(scenario==5){scoped=true;item=8;expected=68;}
            if(scenario==6){item=49;expected=68;}
            if(scenario==7){item=39;expected=78;}
            if(scenario==8){item=8;expected=78;}
            if(scenario==9){camera=110;addition=20;expected=68;}
            if(scenario==10){camera=80;addition=-10;expected=68;}
            if(scenario==11){baseline=54;addition=30;expected=84;}
            if(scenario>=12){
                unsigned life=scenario==12||scenario==16||scenario==17?0:1;
                unsigned mode=scenario==14?6:scenario==18?0:4;
                unsigned target=scenario==15?~0u:2;
                std::memcpy(player.data()+life_offset,&life,4);std::memcpy(player.data()+mode_offset,&mode,4);std::memcpy(player.data()+target_offset,&target,4);
                if(scenario==16 || scenario==17){
                    resource_object=reinterpret_cast<unsigned>(resource.data());
                    resource[alive_offset+1]=scenario==16?1:0;
                }
                if(scenario==13 || scenario==17)expected=68;
            }
            player[scoped_offset]=scoped;std::memcpy(weapon.data()+item_offset,&item,2);
            std::memcpy(view.data()+0xB0,&camera,4);std::memcpy(image+0x532160,&addition,4);
            require(FlushInstructionCache(GetCurrentProcess(),image,cfg::IMAGE_SIZE)!=FALSE,"cache flush failed");
            float result=0;require(execute(reinterpret_cast<void*>(view_ptr),bindings,&result),"original viewmodel callback failed");
            require(std::isfinite(result) && std::fabs(result-expected)<0.0001F,"original viewmodel branch/result mismatch");
            std::printf("PASS original viewmodel scenario=%u result=%.9g\n",scenario,result);++cases;
        }
        require(!std::memcmp(image+0x14FE90,raw.data()+0x14FE90,0x92),"original mapped callback modified");
        require(!std::memcmp(image+0xA1CD0,raw.data()+0xA1CD0,0x98),"original mapped view-entity helper modified");
        dormant=false;resource_object=0;player.fill(0);std::memcpy(player.data()+8,&network_table,4);
        for(unsigned scenario=0;scenario<5;++scenario){
            view.fill(0);const float camera=90,addition=static_cast<float>(scenario*10)-10;
            std::memcpy(view.data()+0xB0,&camera,4);std::memcpy(image+0x531EF8,&addition,4);local_present=scenario!=4;
            original_fov::render(view.data(),bindings);
            float result=0;std::memcpy(&result,view.data()+0xB0,4);
            require(result==(scenario==4?90:90+addition),"original render-FOV leaf result/gate failed");
            std::printf("PASS original render FOV scenario=%u result=%.9g\n",scenario,result);
        }
        require(lookup_calls>0 && handle_calls>0 && network_calls>0,"native dependency paths not exercised");
        std::printf("PASS %u original viewmodel-FOV scenarios; original player/observer/weapon helpers retained, Source APIs are isolated fixtures\n",cases);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
