#include "elf.h"

// Ref: http://www.skyfree.org/linux/references/ELF_Format.pdf

// Test if the buff is a ELF image by checking the magic number
bool is_elf(const char* buff) {
    return (buff[0] == ELFMAG[0]) && (buff[1] == ELFMAG[1]) && (buff[2] == ELFMAG[2]) && (buff[3] == ELFMAG[3]);
}

// Load a ELF executable to memory according to ELF meta information
//
// buff: pointer to the start of the ELF file buffer
//
// return: the physical address of the program entry point
Elf32_Addr load_elf(const char* buff) {
    const Elf32_Ehdr* header = (const Elf32_Ehdr*) buff;
    Elf32_Half n_program_header = header->e_phnum;
    const Elf32_Phdr* program_header_table = (const Elf32_Phdr*) (((uint32_t) buff) + header->e_phoff);
    Elf32_Addr entry_pioint = header->e_entry;
    Elf32_Addr entry_pioint_physical = entry_pioint;
    for(Elf32_Half i=0; i<n_program_header; i++) {
        Elf32_Phdr program_header = program_header_table[i];
        if(program_header.p_type == PT_LOAD){
            char* dest_ptr = (char*) program_header.p_paddr;
            char* src_ptr = (char*) buff + program_header.p_offset;
            // Detect offset of virtual (linear) address vs physical address
            if(entry_pioint>=program_header.p_vaddr && entry_pioint<program_header.p_vaddr + program_header.p_memsz) {
                // use if-else to avoid negative number
                if(program_header.p_paddr > program_header.p_vaddr) {
                    entry_pioint_physical = entry_pioint + (program_header.p_paddr - program_header.p_vaddr);
                } else {
                    entry_pioint_physical = entry_pioint - (program_header.p_vaddr - program_header.p_paddr);
                }
            }

            for(Elf32_Word j=0; j<program_header.p_memsz; j++) {
                if(j<program_header.p_filesz) {
                    dest_ptr[j] = src_ptr[j];
                } else {
                    dest_ptr[j] = 0;
                }
            }
        }
    }
    return entry_pioint_physical;
}