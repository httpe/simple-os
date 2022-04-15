#include <stdbool.h>
#include <string.h>
#include <kernel/elf.h>


// Ref: http://www.skyfree.org/linux/references/ELF_Format.pdf

// Test if the buff is a ELF image by checking the magic number
bool is_elf(const char* buff) {
    return (buff[0] == ELFMAG[0]) && (buff[1] == ELFMAG[1]) && (buff[2] == ELFMAG[2]) && (buff[3] == ELFMAG[3]);
}

// Load a ELF executable to memory according to ELF meta information
//
// buff: pointer to the start of the ELF file buffer
//
// return: the virtual address of the program entry point
// vaddr_ub: returning the virtual address upper bound used by the loaded program
Elf32_Addr load_elf(pde* page_dir, const char* buff, uint32_t* vaddr_ub) {
    const Elf32_Ehdr* header = (const Elf32_Ehdr*)buff;
    Elf32_Half n_program_header = header->e_phnum;
    const Elf32_Phdr* program_header_table = (const Elf32_Phdr*)(((uint32_t)buff) + header->e_phoff);
    Elf32_Addr entry_pioint = header->e_entry;
    *vaddr_ub = 0;
    for (Elf32_Half i = 0; i < n_program_header; i++) {
        Elf32_Phdr program_header = program_header_table[i];
        if (program_header.p_type == PT_LOAD) {
            char* src_ptr = (char*)buff + program_header.p_offset;
            char* dest_ptr = (char*) link_pages(page_dir, program_header.p_vaddr, program_header.p_memsz, curr_page_dir(), true, (program_header.p_flags & PF_W)==PF_W, true);
            memset(dest_ptr, 0, program_header.p_memsz);
            memmove(dest_ptr, src_ptr, program_header.p_filesz);
            unmap_pages(curr_page_dir(), (uint32_t)dest_ptr, program_header.p_memsz);
            
            // calculate upper bound of the mapped user space memory
            // uint32_t mapped_vaddr_ub = (program_header.p_vaddr + program_header.p_memsz + (PAGE_SIZE - 1))/PAGE_SIZE*PAGE_SIZE;
            uint32_t mapped_vaddr_ub = program_header.p_vaddr + program_header.p_memsz - 1;
            if(mapped_vaddr_ub > *vaddr_ub) {
                *vaddr_ub = mapped_vaddr_ub;
            } 
        }
    }
    return entry_pioint;
}