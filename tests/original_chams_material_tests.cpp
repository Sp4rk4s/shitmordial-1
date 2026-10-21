// Isolated execution of original134810. Source virtual methods below are TEST
// fixtures; no fixture or replacement material implementation is used by a DLL.
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
#include <string>
#include <vector>

namespace {
unsigned char* image=nullptr;
template<class T>T& at(unsigned rva){return *reinterpret_cast<T*>(image+rva);}
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::array<void*,85> system_methods{};
std::array<void*,13> material_methods{};
struct Object {void** methods;};
Object system_object{},ambient{},flat{};
std::vector<unsigned> events;
std::vector<std::string> names;
unsigned find_count=0,fail_find=0,addrefs=0;
bool contract_ok=true;
DWORD recorded_exception_code=0,exception_eip=0;
constexpr unsigned untouched=0x11223344;

void* __fastcall find_material(void* receiver,void*,const char* name,
    const char* group,bool complain,const char* prefix){
    ++find_count;events.push_back(10+find_count);
    contract_ok=contract_ok&&receiver==&system_object&&group==nullptr
        &&!complain&&prefix==nullptr&&name!=nullptr;
    names.emplace_back(name?name:"");
    if(find_count==1){
        contract_ok=contract_ok&&at<unsigned>(0x7DC930)==untouched
            &&at<unsigned>(0x7DC934)==untouched;
    }else if(find_count==2){
        contract_ok=contract_ok&&at<void*>(0x7DC930)==&ambient
            &&at<unsigned>(0x7DC934)==untouched&&addrefs==1;
    }else contract_ok=false;
    if(find_count==fail_find)return nullptr;
    return find_count==1?&ambient:&flat;
}
void __fastcall add_ref(void* receiver,void*){
    ++addrefs;events.push_back(20+addrefs);
    if(addrefs==1){
        contract_ok=contract_ok&&receiver==&ambient
            &&at<void*>(0x7DC930)==&ambient
            &&at<unsigned>(0x7DC934)==untouched;
    }else if(addrefs==2){
        contract_ok=contract_ok&&receiver==&flat
            &&at<void*>(0x7DC930)==&ambient&&at<void*>(0x7DC934)==&flat;
    }else contract_ok=false;
}
int capture(EXCEPTION_POINTERS* info){
    recorded_exception_code=info->ExceptionRecord->ExceptionCode;
    exception_eip=info->ContextRecord->Eip;
    return EXCEPTION_EXECUTE_HANDLER;
}
bool execute(){
    __try {reinterpret_cast<void(__cdecl*)()>(image+0x134810)();return true;}
    __except(capture(GetExceptionInformation())){return false;}
}
void reset(unsigned failing_request,bool null_system=false){
    events.clear();names.clear();find_count=addrefs=0;fail_find=failing_request;
    contract_ok=true;recorded_exception_code=exception_eip=0;
    at<void*>(0x4DE19C)=null_system?nullptr:&system_object;
    at<unsigned>(0x7DC930)=at<unsigned>(0x7DC934)=untouched;
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"dump required");std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE&&mapping::checksum(raw.data(),raw.size())==menu_profile::raw_checksum,"dump identity mismatch");
        image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base test allocation failed");
        std::memcpy(image,raw.data(),raw.size());
        system_methods[84]=reinterpret_cast<void*>(&find_material);
        material_methods[12]=reinterpret_cast<void*>(&add_ref);
        system_object.methods=system_methods.data();ambient.methods=flat.methods=material_methods.data();
        for(unsigned repetition=0;repetition<2;++repetition){
            reset(0);require(execute(),"original initializer faulted with valid fixtures");
            require(contract_ok,"native argument/receiver/storage ordering contract changed");
            require(names==std::vector<std::string>{"debug/debugambientcube","debug/debugdrawflat"},"original name decoding changed");
            require(events==std::vector<unsigned>{11,21,12,22},"native FindMaterial/AddRef ordering changed");
            require(at<void*>(0x7DC930)==&ambient&&at<void*>(0x7DC934)==&flat,"native material result slots incorrect");
        }
        // Null handling is observed, not repaired: the original code stores a
        // null return, then dereferences address0x30 to obtain virtual slot12.
        reset(1);require(!execute(),"first null material unexpectedly succeeded");
        require(contract_ok&&events==std::vector<unsigned>{11}&&at<void*>(0x7DC930)==nullptr
            &&at<unsigned>(0x7DC934)==untouched,"first null changed native partial state");
        require(recorded_exception_code==EXCEPTION_ACCESS_VIOLATION&&exception_eip==cfg::IMAGE_BASE+0x134877,"first null fault location changed");
        reset(2);require(!execute(),"second null material unexpectedly succeeded");
        require(contract_ok&&events==std::vector<unsigned>{11,21,12}&&at<void*>(0x7DC930)==&ambient
            &&at<void*>(0x7DC934)==nullptr,"second null changed native partial state");
        require(recorded_exception_code==EXCEPTION_ACCESS_VIOLATION&&exception_eip==cfg::IMAGE_BASE+0x1348DB,"second null fault location changed");
        reset(0,true);require(!execute(),"null MaterialSystem unexpectedly succeeded");
        require(events.empty()&&at<unsigned>(0x7DC930)==untouched&&at<unsigned>(0x7DC934)==untouched,"null system mutated native slots");
        require(recorded_exception_code==EXCEPTION_ACCESS_VIOLATION&&exception_eip==cfg::IMAGE_BASE+0x134857,"null system fault location changed");
        require(!std::memcmp(image+0x134810,raw.data()+0x134810,0xD5),"original initializer instructions changed");
        require(!std::memcmp(image+0x41DC80,raw.data()+0x41DC80,0x20),"original encrypted name constants changed");
        std::puts("Original chams materials: two native runs and three native null-fault cases pass; initializer and name constants unchanged.");
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
