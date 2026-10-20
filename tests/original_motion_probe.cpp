// Isolated ORIGINAL x86 feature execution. Source/UI dependencies are explicit
// test fixtures; no DLL entry, constructors, game hooks or bootstrap is invoked.
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/imports.h"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int report(EXCEPTION_POINTERS* exception){
    std::printf("ORIGINAL PROBE exception=%08lX eip=%08lX eax=%08lX ecx=%08lX edx=%08lX esp=%08lX\n",
        exception->ExceptionRecord->ExceptionCode,exception->ContextRecord->Eip,exception->ContextRecord->Eax,
        exception->ContextRecord->Ecx,exception->ContextRecord->Edx,exception->ContextRecord->Esp);
    if(exception->ExceptionRecord->NumberParameters>=2)
        std::printf("operation=%lu address=%08lX\n",exception->ExceptionRecord->ExceptionInformation[0],exception->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_EXECUTE_HANDLER;
}
bool execute(void* entry){
    __try {reinterpret_cast<void(__cdecl*)()>(entry)();return true;}
    __except(report(GetExceptionInformation())){return false;}
}
bool correct_movement(void* entry,float* output_angles,float* saved_angles,void* command){
    __try {reinterpret_cast<int(__cdecl*)(float*,float*,void*)>(entry)(output_angles,saved_angles,command);return true;}
    __except(report(GetExceptionInformation())){return false;}
}
void operand(unsigned char* image,unsigned rva,unsigned expected,const void* cell){
    unsigned actual=0;std::memcpy(&actual,image+rva,4);require(actual==expected,"original operand guard failed");
    const unsigned address=reinterpret_cast<unsigned>(cell);std::memcpy(image+rva,&address,4);
}
}
int main(int argc,char** argv){
    try {
        require(argc==2,"original dump required");
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> raw{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        require(raw.size()==cfg::BINARY_SIZE,"unexpected dump size");
        const unsigned char expected_hash[]={0xe1,0x6d,0xd0,0xec,0xf4,0xf7,0x63,0xbe,0x49,0x13,0x9d,0x5e,0xdf,0xf2,0x1f,0x2e,0x61,0x06,0xcf,0xf1,0x04,0x0e,0x2f,0x6a,0x68,0x4f,0xab,0xcb,0x7e,0xeb,0x97,0x68};
        unsigned char actual_hash[32]{};BCRYPT_ALG_HANDLE algorithm=nullptr;
        require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider unavailable");
        const auto hash_status=BCryptHash(algorithm,nullptr,0,raw.data(),static_cast<ULONG>(raw.size()),actual_hash,sizeof(actual_hash));
        BCryptCloseAlgorithmProvider(algorithm,0);
        require(hash_status>=0 && !std::memcmp(expected_hash,actual_hash,sizeof(actual_hash)),"dump SHA256 mismatch");
        auto* image=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE),cfg::IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        require(image==reinterpret_cast<void*>(cfg::IMAGE_BASE),"fixed-base isolated allocation unavailable");
        std::memcpy(image,raw.data(),raw.size());
        const auto imports=imports::apply_recovered(image,raw.size());require(imports.ok,imports.error.c_str());
        std::array<unsigned char,4096> entity{};std::array<unsigned char,100> command{};
        const unsigned water=0x25E,flags=0x104,velocity=0x114;
        operand(image,0xF92F3,0x2D9CB32C,&water);
        operand(image,0xF9337,0x2D8F778B,&flags);
        operand(image,0xF9352,0x2D8F1D4F,&velocity);
        // Model only the already verified public local-player lookup and an
        // inactive slow-walk binding. ORIGINAL F92D0/math/ring code runs intact.
        require(!std::memcmp(image+0xA06C0,"\x56\x8B\xF1\x8B\x0D",5),"local lookup profile changed");
        image[0xA06C0]=0xB8;const auto entity_pointer=reinterpret_cast<unsigned>(entity.data());
        std::memcpy(image+0xA06C1,&entity_pointer,4);image[0xA06C5]=0xC3;
        require(!std::memcmp(image+0x33AE00,"\x56\x8B\xF1",3),"bind getter profile changed");
        std::memcpy(image+0x33AE00,"\x31\xC0\xC3",3);
        const auto command_pointer=reinterpret_cast<unsigned>(command.data());std::memcpy(image+0x4DE214,&command_pointer,4);
        // Original predicate 10ACC0 reads this captured byte. False is the
        // explicit non-ladder fixture; no replacement predicate code is used.
        image[0x7DBEFE]=0;
        require(FlushInstructionCache(GetCurrentProcess(),image,cfg::IMAGE_SIZE)!=FALSE,"cache flush failed");
        unsigned cases=0;
        for(unsigned scenario=0;scenario<12;++scenario){
            entity.fill(0);command.fill(0);std::memset(image+0x8244B8,0,150*44);
            const int number=1;std::memcpy(command.data()+4,&number,4);
            float speed[3]={250,0,0};if(scenario==5)speed[0]=1;
            if(scenario==9){speed[0]=0;speed[1]=250;}
            if(scenario==10){speed[0]=-250;speed[1]=120;}
            if(scenario==11)speed[2]=300;
            std::memcpy(entity.data()+velocity,speed,sizeof(speed));
            image[0x53D360]=scenario!=1;image[0x7DBBDD]=scenario==6;
            const float smoothing=75;std::memcpy(image+0x53D498,&smoothing,4);
            if(scenario==2)entity[water]=2;
            if(scenario==3)entity[flags]=1;
            if(scenario==4){const unsigned buttons=0x20000;std::memcpy(command.data()+0x30,&buttons,4);}
            if(scenario==7){const float forward=200;std::memcpy(command.data()+0x24,&forward,4);}
            if(scenario==8){const float side=-200;std::memcpy(command.data()+0x28,&side,4);}
            const auto before=command;
            require(execute(image+0xF92D0),"original autostrafer failed in isolated fixture");
            float forward=0,side=0,angle[3]{};std::memcpy(&forward,command.data()+0x24,4);std::memcpy(&side,command.data()+0x28,4);
            std::memcpy(angle,image+0x8244B8+44+20,12);
            if(scenario>=1 && scenario<=5){
                require(command==before,"original gate unexpectedly changed command");
                require(image[0x7DBBDD]==0,"original gate changed alternation state");
            }else{
                require(std::isfinite(forward) && std::isfinite(side),"nonfinite native movement");
                require(std::fabs(side)==450,"native active strafe did not produce side speed");
                for(float value:angle)require(std::isfinite(value),"nonfinite original angle state");
                require(image[0x7DBBDD]==(scenario==6?0:1),"original alternation state failed");
                if(scenario==0)require(side==450 && angle[1]>3 && angle[1]<4,"original baseline changed");
                if(scenario==6)require(side==-450 && angle[1]<-3 && angle[1]>-4,"original alternate side failed");
                auto* output_angles=reinterpret_cast<float*>(command.data()+0xC);
                auto* saved_angles=reinterpret_cast<float*>(image+0x8244B8+44+20);
                require(correct_movement(image+0xF7A50,output_angles,saved_angles,nullptr),"original movement correction failed");
                float corrected_forward=0,corrected_side=0;
                std::memcpy(&corrected_forward,command.data()+0x24,4);
                std::memcpy(&corrected_side,command.data()+0x28,4);
                require(std::isfinite(corrected_forward) && std::isfinite(corrected_side),"nonfinite original corrected movement");
                const float magnitude=std::sqrt(corrected_forward*corrected_forward+corrected_side*corrected_side);
                require(magnitude>449.9F && magnitude<450.1F,"original correction lost movement magnitude");
                for(unsigned axis=0;axis<3;++axis)require(output_angles[axis]==0,"original correction changed view angles");
                std::printf("ORIGINAL F7A50 case=%u forward=%.9g side=%.9g magnitude=%.9g\n",scenario,corrected_forward,corrected_side,magnitude);
            }
            std::printf("ORIGINAL F92D0 case=%u forward=%.9g side=%.9g saved_angles=%.9g,%.9g,%.9g\n",scenario,forward,side,angle[0],angle[1],angle[2]);
            ++cases;
        }
        std::printf("PASS %u original-code scenarios (isolated Source/UI fixtures; not live gameplay)\n",cases);
        VirtualFree(image,0,MEM_RELEASE);return 0;
    }catch(const std::exception& exception){std::fprintf(stderr,"FAIL: %s\n",exception.what());return 1;}
}
