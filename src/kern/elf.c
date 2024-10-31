#include "elf.h"
#include "io.h"
#include <stdint.h>
#include <string.h> 

//           Relevant defines
#define EI_NIDENT   16
#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define PT_LOAD     1  // Program header type for loadable segments

#define RV64_MACHINE 243

// Validate that file is in ELF format
#define ELF_MAGIC_OK(ehdr) ((ehdr).e_ident[0] == ELFMAG0 && \
                            (ehdr).e_ident[1] == ELFMAG1 && \
                            (ehdr).e_ident[2] == ELFMAG2 && \
                            (ehdr).e_ident[3] == ELFMAG3)

#define LOAD_START  0x80100000
#define LOAD_END    0x81000000

// Elf Header Structure for 64 bit ELF
typedef struct elf64_hdr
{
    unsigned char   e_ident[EI_NIDENT];     // ELF identification bytes
    uint16_t        e_type;                 // Object file type
    uint16_t        e_machine;              // Architecture
    uint32_t        e_version;              // ELF Version
    uint64_t        e_entry;                // Entry point address
    uint64_t        e_phoff;                // Program header table offset
    uint64_t        e_shoff;                // Section header table offset
    uint32_t        e_flags;                // Processor specific flags
    uint16_t        e_ehsize;               // ELF header size
    uint16_t        e_phentsize;            // Program Header entry size
    uint16_t        e_phnum;
    uint16_t        e_shentsize;
    uint16_t        e_shnum;
    uint16_t        e_shstrndx;
} Elf64_Ehdr;

// Program Header Strucutre for 64 bit ELF
typedef struct elf64_phdr{
    uint32_t        p_type;
    uint32_t        p_flags;
    uint64_t        p_offset;
    uint64_t        p_vaddr;
    uint64_t        p_paddr;
    uint64_t        p_filesz;
    uint64_t        p_memsz;
    uint64_t        p_align;
} Elf64_Phdr;

int elf_load(struct io_intf *io, void (**entryptr)(struct io_intf *io)){
    Elf64_Ehdr elf_header;

    // 1. Read and validate ELF Header
    if(ioread_full(io, &elf_header, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)){
        return -1; // Failed to read ELF Header
    }

    if (!ELF_MAGIC_OK(elf_header)){
        return -2; // Invalid ELF magic number
    }

    // 2. Verify ELF type and architecture for 64-bit RISC-V
    if (elf_header.e_type != 2 || elf_header.e_machine != RV64_MACHINE){
        return -3; // Unsupported ELF type or machine
    }

    // 3. Parse and load each program header
    for (uint16_t i = 0; i < elf_header.e_phnum; i++) {
        Elf64_Phdr phdr;
        
        // Seek to the next program header using ioseek
        if (ioseek(io, elf_header.e_phoff + i * elf_header.e_phentsize) != 0) {
            return -4; // Failed to seek to program header
        }
        
        // Read the program header
        if (ioread_full(io, &phdr, sizeof(Elf64_Phdr)) != sizeof(Elf64_Phdr)) {
            return -5; // Failed to read program header
        }

        if (phdr.p_type == PT_LOAD) {
            // Check if segment is within the allowed memory range
            if (phdr.p_vaddr < LOAD_START || (phdr.p_vaddr + phdr.p_memsz) > LOAD_END) {
                return -6; // Segment is out of bounds
            }

            // Load the segment into memory at p_vaddr
            if (ioseek(io, phdr.p_offset) != 0) {
                return -7; // Failed to seek to segment offset
            }
            
            if (ioread_full(io, (void *)phdr.p_vaddr, phdr.p_filesz) != phdr.p_filesz) {
                return -8; // Failed to load segment
            }

            // Zero out remaining memory if p_memsz > p_filesz
            if (phdr.p_memsz > phdr.p_filesz) {
                memset((void *)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
            }
        }
    }


    // 4. Set the entry point function pointer
    *entryptr = (void (*)(struct io_intf *io))elf_header.e_entry;

    return 0; // Success
}

