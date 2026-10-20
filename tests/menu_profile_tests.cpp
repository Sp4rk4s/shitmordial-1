#include "../primordial_loader/src/menu_profile.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <iterator>
#include <string>

namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void fixture(const menu_profile::SourceGuard& guard,const char* path){
    unsigned char bytes[0x600]{};
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(bytes+0x80);nt->Signature=IMAGE_NT_SIGNATURE;
    auto& file=nt->FileHeader;file.Machine=IMAGE_FILE_MACHINE_I386;file.NumberOfSections=2;
    file.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);file.Characteristics=IMAGE_FILE_EXECUTABLE_IMAGE|IMAGE_FILE_32BIT_MACHINE|IMAGE_FILE_DLL;
    auto& opt=nt->OptionalHeader;opt.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    opt.ImageBase=menu_profile::modules[guard.module_index].preferred_base;
    opt.SectionAlignment=0x1000;opt.FileAlignment=0x200;opt.SizeOfImage=0x3000;opt.SizeOfHeaders=0x200;
    opt.MajorOperatingSystemVersion=6;opt.MajorSubsystemVersion=6;opt.Subsystem=IMAGE_SUBSYSTEM_WINDOWS_CUI;
    opt.DllCharacteristics=IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE|IMAGE_DLLCHARACTERISTICS_NX_COMPAT;
    opt.SizeOfStackReserve=0x100000;opt.SizeOfStackCommit=0x1000;opt.SizeOfHeapReserve=0x100000;opt.SizeOfHeapCommit=0x1000;
    opt.NumberOfRvaAndSizes=IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    auto* sections=IMAGE_FIRST_SECTION(nt);
    std::memcpy(sections[0].Name,".text",5);sections[0].Misc.VirtualSize=0x200;sections[0].VirtualAddress=0x1000;
    sections[0].SizeOfRawData=0x200;sections[0].PointerToRawData=0x200;
    sections[0].Characteristics=IMAGE_SCN_CNT_CODE|IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ;
    std::memcpy(sections[1].Name,".reloc",6);sections[1].Misc.VirtualSize=0x200;sections[1].VirtualAddress=0x2000;
    sections[1].SizeOfRawData=0x200;sections[1].PointerToRawData=0x400;
    sections[1].Characteristics=IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_DISCARDABLE;
    std::memcpy(bytes+0x200,guard.bytes,sizeof(guard.bytes));
    // Windows performs the actual HIGHLOW adjustments; no payload code executes.
    auto* block=reinterpret_cast<IMAGE_BASE_RELOCATION*>(bytes+0x400);block->VirtualAddress=0x1000;
    auto* entries=reinterpret_cast<WORD*>(block+1);unsigned count=0;
    for(unsigned i=0;i<32;++i)if(guard.relocations&(1u<<i))entries[count++]=static_cast<WORD>(0x3000|i);
    entries[count++]=0x3080; // Every fixture relocates, including prefixes without operands.
    *reinterpret_cast<DWORD*>(bytes+0x280)=opt.ImageBase+0x1080;
    if(count%2)entries[count++]=0;
    block->SizeOfBlock=sizeof(*block)+2*count;opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]={0x2000,block->SizeOfBlock};
    HANDLE output=CreateFileA(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(output!=INVALID_HANDLE_VALUE,"fixture file creation");DWORD written=0;
    BOOL ok=WriteFile(output,bytes,sizeof(bytes),&written,nullptr);CloseHandle(output);
    require(ok&&written==sizeof(bytes),"fixture file write");
}
}
int main(){
    try {
        char temporary[MAX_PATH],directory[MAX_PATH];require(GetTempPathA(MAX_PATH,temporary)!=0,"temporary path");
        sprintf_s(directory,"%smenu_profile_%lu",temporary,GetCurrentProcessId());require(CreateDirectoryA(directory,nullptr)!=0,"private fixture directory");
        unsigned cases=0,relocations=0;
        for(const auto& profile:menu_profile::source){
            const auto preferred=menu_profile::modules[profile.module_index].preferred_base;
            void* reservation=VirtualAlloc(reinterpret_cast<void*>(preferred),4096,MEM_RESERVE,PAGE_NOACCESS);
            MEMORY_BASIC_INFORMATION info{};require(VirtualQuery(reinterpret_cast<void*>(preferred),&info,sizeof(info))&&info.State!=MEM_FREE,"force preferred-base conflict");
            char path[MAX_PATH];sprintf_s(path,"%s\\prefix_%u.dll",directory,cases);fixture(profile,path);
            HMODULE module=LoadLibraryExA(path,nullptr,DONT_RESOLVE_DLL_REFERENCES);
            require(module!=nullptr,"Windows data-only PE load");
            const auto actual_base=static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(module));
            require(actual_base!=preferred,"fixture must exercise ASLR relocation");
            auto* code=reinterpret_cast<const std::uint8_t*>(module)+0x1000;
            require(menu_profile::matches(profile,code,32,actual_base),"real Windows-relocated prefix rejected");
            require(!menu_profile::matches(profile,code,profile.size-1,actual_base),"truncated prefix accepted");
            std::uint8_t altered[32];std::memcpy(altered,code,32);altered[0]^=1;
            require(!menu_profile::matches(profile,altered,32,actual_base),"changed instruction accepted");
            auto invalid=profile;invalid.module_index=static_cast<unsigned>(std::size(menu_profile::modules));
            require(!menu_profile::matches(invalid,code,32,actual_base),"invalid module profile accepted");
            invalid=profile;invalid.relocations|=1u<<31;
            require(!menu_profile::matches(invalid,code,32,actual_base),"invalid relocation accepted");
            if(profile.relocations){require(!menu_profile::matches(profile,code,32,preferred),"wrong load base accepted");++relocations;}
            FreeLibrary(module);if(reservation)VirtualFree(reservation,0,MEM_RELEASE);
            require(DeleteFileA(path)!=0,"fixture file cleanup");++cases;
        }
        require(RemoveDirectoryA(directory)!=0,"empty fixture directory cleanup");
        std::printf("PASS: %u Source prefixes verified after real Windows relocation; %u contain relocated operands. No target functions executed.\n",cases,relocations);
        return 0;
    }catch(const std::exception& e){std::printf("FAIL: %s (Win32 %lu)\n",e.what(),GetLastError());return 1;}
}
