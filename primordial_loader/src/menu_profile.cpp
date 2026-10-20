#include "menu_profile.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <cstdio>
#include <iterator>

namespace menu_profile {
bool matches(const SourceGuard& guard,const std::uint8_t* actual,
             std::size_t available,std::uint32_t load_base) noexcept {
    if(!actual||guard.module_index>=std::size(modules)||!guard.size||
       guard.size>sizeof(guard.bytes)||available<guard.size)return false;
    std::uint8_t expected[sizeof(guard.bytes)];
    std::memcpy(expected,guard.bytes,sizeof(expected));
    for(unsigned i=0;i<32;++i){
        if(!(guard.relocations&(1u<<i)))continue;
        if(i>=guard.size||i+4>sizeof(expected))return false;
        std::uint32_t operand=0;std::memcpy(&operand,expected+i,4);
        operand+=load_base-modules[guard.module_index].preferred_base;
        std::memcpy(expected+i,&operand,4);
    }
    return std::memcmp(actual,expected,guard.size)==0;
}
namespace {
bool file_hash_matches(const wchar_t* path,const unsigned char* digest) noexcept {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char object[1024],buffer[16384],actual[32];
    DWORD length=0,used=0,read=0;bool ok=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)goto done;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&used,0)<0||length>sizeof(object))goto done;
    if(BCryptCreateHash(algorithm,&hash,object,length,nullptr,0,0)<0)goto done;
    for(;;){
        if(!ReadFile(file,buffer,sizeof(buffer),&read,nullptr))goto done;
        if(!read)break;
        if(BCryptHashData(hash,buffer,read,0)<0)goto done;
    }
    if(BCryptFinishHash(hash,actual,sizeof(actual),0)<0)goto done;
    ok=std::memcmp(actual,digest,sizeof(actual))==0;
done:
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    return ok;
}
}
bool verify_module_files() noexcept {
    for(const auto& profile:modules){
        HMODULE module=GetModuleHandleA(profile.name);wchar_t path[MAX_PATH];
        if(!module){std::printf("[menu] Missing game module %s.\n",profile.name);return false;}
        const DWORD n=GetModuleFileNameW(module,path,MAX_PATH);
        if(!n||n>=MAX_PATH||!file_hash_matches(path,profile.sha256)){
            std::printf("[menu] Unsupported/unreadable %s; installed-file SHA256 differs.\n",profile.name);return false;
        }
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x100000)return false;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const unsigned char*>(module)+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
           nt->FileHeader.TimeDateStamp!=profile.timestamp||nt->OptionalHeader.SizeOfImage!=profile.image_size){
            std::printf("[menu] Loaded %s PE metadata differs.\n",profile.name);return false;
        }
    }
    return true;
}
}
