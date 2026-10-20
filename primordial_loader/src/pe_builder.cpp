#include "pe_builder.h"
#include "config.h"
#include <cstring>

namespace pe {

std::vector<uint8_t> build_header() {
    std::vector<uint8_t> hdr(0x1000, 0);

    // DOS header
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hdr.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = sizeof(IMAGE_DOS_HEADER);

    // PE signature
    auto* sig = reinterpret_cast<uint32_t*>(hdr.data() + dos->e_lfanew);
    *sig = IMAGE_NT_SIGNATURE;

    // COFF header
    auto* coff = reinterpret_cast<IMAGE_FILE_HEADER*>(hdr.data() + dos->e_lfanew + 4);
    coff->Machine              = IMAGE_FILE_MACHINE_I386;
    coff->NumberOfSections     = cfg::NUM_SECTIONS;
    coff->SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
    coff->Characteristics      = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_32BIT_MACHINE | IMAGE_FILE_DLL;

    // Optional header (PE32)
    auto* opt = reinterpret_cast<IMAGE_OPTIONAL_HEADER32*>(
        hdr.data() + dos->e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER));
    opt->Magic                 = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    opt->MajorLinkerVersion    = 14;
    opt->AddressOfEntryPoint   = cfg::ENTRY_RVA;
    opt->BaseOfCode            = 0x1000;
    opt->BaseOfData            = 0x400000;
    opt->ImageBase             = cfg::IMAGE_BASE;
    opt->SectionAlignment      = 0x1000;
    opt->FileAlignment         = 0x200;
    opt->MajorOperatingSystemVersion = 6;
    opt->SizeOfImage           = 0x1410000;
    opt->SizeOfHeaders         = 0x1000;
    opt->Subsystem             = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    // No ASLR, no DEP — needed for fixed-base loading
    opt->DllCharacteristics    = 0;
    opt->SizeOfStackReserve    = 0x100000;
    opt->SizeOfStackCommit     = 0x1000;
    opt->SizeOfHeapReserve     = 0x100000;
    opt->SizeOfHeapCommit      = 0x1000;
    opt->NumberOfRvaAndSizes   = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    // All data directories zero — no IAT, no imports, no relocs (binary resolves its own)

    // Section headers
    auto* sec = reinterpret_cast<IMAGE_SECTION_HEADER*>(
        reinterpret_cast<uint8_t*>(opt) + coff->SizeOfOptionalHeader);

    for (int i = 0; i < cfg::NUM_SECTIONS; ++i) {
        std::memset(&sec[i], 0, sizeof(IMAGE_SECTION_HEADER));
        std::strncpy(reinterpret_cast<char*>(sec[i].Name), cfg::SECTIONS[i].name, 8);
        sec[i].Misc.VirtualSize = cfg::SECTIONS[i].vsize;
        sec[i].VirtualAddress   = cfg::SECTIONS[i].rva;
        sec[i].SizeOfRawData    = cfg::SECTIONS[i].raw_size;
        sec[i].PointerToRawData = cfg::SECTIONS[i].raw_offset;
        sec[i].Characteristics  = cfg::SECTIONS[i].characteristics;
    }

    return hdr;
}

void patch_auth(uint8_t* image, size_t size) {
    if (cfg::AUTH_CHECK_RVA + 6 > size) return;
    // mov eax, 1; ret
    image[cfg::AUTH_CHECK_RVA + 0] = 0xB8;
    image[cfg::AUTH_CHECK_RVA + 1] = 0x01;
    image[cfg::AUTH_CHECK_RVA + 2] = 0x00;
    image[cfg::AUTH_CHECK_RVA + 3] = 0x00;
    image[cfg::AUTH_CHECK_RVA + 4] = 0x00;
    image[cfg::AUTH_CHECK_RVA + 5] = 0xC3;
}

}
