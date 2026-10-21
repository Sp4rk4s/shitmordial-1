// Original132FF0 owns every material/pass/color/blend decision. External entity
// classification and Source methods below are explicit ISOLATED TEST fixtures.
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
unsigned char* image=nullptr;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(image+rva);}
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
bool close(float a,float b){return std::fabs(a-b)<0.00001f;}
std::array<unsigned char,0xA000> local{},player{};
std::array<void*,30> material_methods{};
std::array<void*,7> view_methods{};
std::array<void*,2> model_methods{};
struct Object{void** methods;};
Object lit{},flat{},view{},model{};
bool enemy_value=true,thirdperson=false,contract_ok=true,ignore_depth=false;
void* current_material=nullptr;
float current_blend=1;
std::array<float,3> current_color{1,1,1};
struct Draw{void* material;bool hidden;float blend;std::array<float,3> color;};
std::vector<Draw> draws;
std::vector<unsigned> events;
void* __fastcall local_lookup(void*,void*){return local.data();}
bool __fastcall classify(void* receiver,void*,bool direct){
    contract_ok=contract_ok&&receiver==player.data()&&!direct;return enemy_value;
}
bool __cdecl thirdperson_state(){return thirdperson;}
void __fastcall material_flag(void* receiver,void*,unsigned flag,bool enabled){
    contract_ok=contract_ok&&(receiver==&lit||receiver==&flat)&&flag==0x8000;
    ignore_depth=enabled;events.push_back(enabled?1:2);
}
void __fastcall force_material(void* receiver,void*,void* material,int kind,int overrides){
    contract_ok=contract_ok&&receiver==&model&&kind==0&&overrides==0;
    current_material=material;events.push_back(material?3:4);
}
void __fastcall blend(void* receiver,void*,float value){
    contract_ok=contract_ok&&receiver==&view;current_blend=value;events.push_back(5);
}
void __fastcall color(void* receiver,void*,const float* rgb){
    contract_ok=contract_ok&&receiver==&view;std::memcpy(current_color.data(),rgb,12);events.push_back(6);
}
void __fastcall original_draw(void* receiver,void* edx,unsigned a,unsigned b,unsigned c,unsigned d){
    contract_ok=contract_ok&&receiver==&model&&edx==nullptr&&a==0x11111111
        &&b==0x22222222&&c==0x33333333&&d==0x44444444;
    draws.push_back({current_material,ignore_depth,current_blend,current_color});events.push_back(7);
}
void fixture(unsigned rva,void* target){
    image[rva]=0xE9;const unsigned relative=reinterpret_cast<unsigned>(target)-reinterpret_cast<unsigned>(image+rva+5);
    std::memcpy(image+rva+1,&relative,4);
}
DWORD fault=0;
bool execute(){
    __try{reinterpret_cast<void(__cdecl*)(unsigned,unsigned,unsigned,unsigned)>(image+0x132FF0)(0x11111111,0x22222222,0x33333333,0x44444444);return true;}
    __except(fault=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
int main(int argc,char** argv){
    try{
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base allocation failed");std::memcpy(image,raw.data(),raw.size());
        auto* old_cell_page=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(0x2DBD0000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        require(old_cell_page==reinterpret_cast<void*>(0x2DBD0000),"old operand TEST-page allocation failed");
        const unsigned scoped=0x9974;std::memcpy(old_cell_page+0xB5D3,&scoped,4);
        fixture(0xA06C0,reinterpret_cast<void*>(&local_lookup));fixture(0x36ACC0,reinterpret_cast<void*>(&classify));fixture(0x14F340,reinterpret_cast<void*>(&thirdperson_state));
        material_methods[29]=reinterpret_cast<void*>(&material_flag);lit.methods=flat.methods=material_methods.data();
        view_methods[4]=reinterpret_cast<void*>(&blend);view_methods[6]=reinterpret_cast<void*>(&color);view.methods=view_methods.data();
        model_methods[1]=reinterpret_cast<void*>(&force_material);model.methods=model_methods.data();
        at<void*>(0x4DE1A0)=&model;at<void*>(0x4DE1AC)=&view;at<void*>(0x7DC950)=reinterpret_cast<void*>(&original_draw);
        unsigned cases=0;
        for(unsigned scenario=0;scenario<11;++scenario){
            draws.clear();events.clear();contract_ok=true;enemy_value=true;thirdperson=false;fault=0;
            ignore_depth=false;current_material=nullptr;current_blend=1;current_color={1,1,1};
            local[scoped]=0;at<float>(0x7DC940)=0.8f;at<float>(0x5355F8)=25.0f;
            at<void*>(0x7DC93C)=player.data();at<void*>(0x7DC930)=&lit;at<void*>(0x7DC934)=&flat;
            // Different control strides are literal original code operands:
            // materialtype/hidden gate15C; RGBA and hidden-checkbox260.
            for(unsigned category=0;category<3;++category){
                at<unsigned>(0x52DF60+category*0x15C)=0;
                at<unsigned>(0x52C448+category*0x15C)=1;
                at<unsigned>(0x52E4A0+category*0x260)=0x80402010;
                at<unsigned char>(0x52D0AB+category*0x260)=1;
            }
            unsigned category=1,type=2,expected_draws=2;bool hidden=true,local_case=false;
            switch(scenario){
                case 0:type=0;expected_draws=1;hidden=false;break;
                case 1:type=1;break;
                case 2:break;
                case 3:at<unsigned>(0x52C448+0x15C)=0;expected_draws=1;hidden=false;break;
                case 4:at<unsigned char>(0x52D0AB+0x260)=0;expected_draws=1;hidden=false;break;
                case 5:enemy_value=false;category=2;break;
                case 6:at<void*>(0x7DC93C)=local.data();category=0;expected_draws=1;hidden=false;local_case=true;break;
                case 7:at<void*>(0x7DC93C)=local.data();category=0;expected_draws=1;hidden=false;local_case=true;thirdperson=true;local[scoped]=1;break;
                case 8:at<void*>(0x7DC930)=nullptr;expected_draws=1;hidden=false;break;
                case 9:at<void*>(0x7DC93C)=local.data();category=0;type=0;expected_draws=1;hidden=false;local_case=true;thirdperson=true;local[scoped]=1;break;
                case 10:at<void*>(0x7DC93C)=local.data();category=0;expected_draws=1;hidden=false;local_case=true;thirdperson=true;break;
            }
            at<unsigned>(0x52DF60+category*0x15C)=type;
            require(execute(),"native material pass routine faulted");require(contract_ok,"native ABI/receiver/flag contract changed");
            require(draws.size()==expected_draws,"native pass count changed");
            require(!std::memcmp(image+0x132FF0,raw.data()+0x132FF0,0x477),"native material-pass instructions modified");
            const bool fallback=type==0||scenario==8;
            for(unsigned i=0;i<draws.size();++i){
                const auto& draw=draws[i];require(draw.material==(fallback?nullptr:type==1?static_cast<void*>(&flat):static_cast<void*>(&lit)),"original material selection changed");
                require(draw.hidden==(hidden&&i==0),"original ignore-depth pass ordering changed");
                if(scenario!=8){
                    // These are expected fixture observations only; the actual
                    // blend and normalization are calculated by native132FF0.
                    float expected_blend=(128.0f/255.0f)*0.8f;
                    if(local_case&&thirdperson&&local[scoped])expected_blend*=0.75f;
                    require(close(draw.blend,expected_blend),"original blend/scoped-opacity result changed");
                    const std::array<float,3> expected=fallback?std::array<float,3>{1,1,1}:std::array<float,3>{16.0f/255.0f,32.0f/255.0f,64.0f/255.0f};
                    for(unsigned j=0;j<3;++j)require(close(draw.color[j],expected[j]),"original normalized color changed");
                }
            }
            if(!fallback){
                require(current_material==nullptr&&!ignore_depth&&close(current_blend,1),"native draw state restoration changed");
                require(current_color==std::array<float,3>{1,1,1},"native color restoration changed");
                require(events.back()==4,"native override clear ordering changed");
            }else if(type==0){
                // Native pass-through leaves its requested alpha in place;
                // pretending it restores blend1 would hide a real contract.
                require(events==std::vector<unsigned>{5,6,7},"native zero-type pass-through ordering changed");
            }else require(events==std::vector<unsigned>{7},"native null-material fallback changed");
            ++cases;
        }
        std::printf("Original native chams passes: %u material/category/hidden/scoped/fallback cases pass.\n",cases);
        VirtualFree(old_cell_page,0,MEM_RELEASE);VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
