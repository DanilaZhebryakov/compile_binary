#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

#include "exec_output.h"

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

static size_t round_to_pages(size_t i){
    return i + (PAGESIZE-1 - ((i-1) % PAGESIZE));
}

static void elfFill_e_ident(Elf64_Ehdr *header) {
    header->e_ident[EI_MAG0] = ELFMAG0;
    header->e_ident[EI_MAG1] = ELFMAG1;
    header->e_ident[EI_MAG2] = ELFMAG2;
    header->e_ident[EI_MAG3] = ELFMAG3;

    header->e_ident[EI_CLASS  ] = ELFCLASS64;
    header->e_ident[EI_DATA   ] = ELFDATA2LSB;
    header->e_ident[EI_VERSION] = EV_CURRENT;
    header->e_ident[EI_OSABI  ] = ELFOSABI_SYSV;

    header->e_ident[EI_ABIVERSION] = 0;
    header->e_ident[EI_VERSION] = EV_CURRENT;
}

static int composeSegments(ExecOutput* out, Elf64_Phdr* prog_hdrs, size_t* sect_offs, uint64_t* sect_addr,  uint64_t* mem_addr, size_t* file_offset, uint64_t* entrypoint) {
    int segs_used = 0;
    for (int j = 0; j < 8; j++) { //j is 0b0RWX as bits
        bool seg_used = false;
        for (size_t i = 0; i < out->sect_count; i++) {
            if (!out->sects[i].name) // null-section, ignored
                continue;

            if (((out->sects[i].attr.readable << 2)
               | (out->sects[i].attr.writable << 1)
               | (out->sects[i].attr.executable)) != j) {
                continue;
            }
            if (!seg_used){
                    *mem_addr  = round_to_pages(*mem_addr);
                    *file_offset = round_to_pages(*file_offset);
                    prog_hdrs[segs_used].p_type = PT_LOAD;
                    prog_hdrs[segs_used].p_flags = (out->sects[i].attr.executable ? PF_X : 0)
                                                 | (out->sects[i].attr.readable   ? PF_R : 0)
                                                 | (out->sects[i].attr.writable   ? PF_W : 0);
                    prog_hdrs[segs_used].p_filesz = 0;
                    prog_hdrs[segs_used].p_memsz  = 0;
                    prog_hdrs[segs_used].p_vaddr = *mem_addr;
                    prog_hdrs[segs_used].p_offset= *file_offset;
                    prog_hdrs[segs_used].p_align = PAGESIZE;
                    segs_used++;
                    seg_used = true;
            }
            if (out->sects[i].attr.fill_type != BIN_SECTION_INITIALISED) {
                continue;
            }

            if (out->sects[i].attr.entry_point){
                if (*entrypoint) {
                    error_log("Multiple entrypoint sects defined. Merge them first\n");
                }
                *entrypoint = *mem_addr;
            }
            sect_offs[i]  = *file_offset;
            sect_addr[i]  = *mem_addr;

            prog_hdrs[segs_used-1].p_filesz += out->sects[i].size;
            prog_hdrs[segs_used-1].p_memsz  += out->sects[i].size;
            *file_offset += prog_hdrs[segs_used-1].p_filesz;
            *mem_addr    += prog_hdrs[segs_used-1].p_memsz;

        }
        for (size_t i = 0; i < out->sect_count; i++) {
            if (!out->sects[i].name) // null-section, ignored
                continue;
            if (out->sects[i].attr.fill_type == BIN_SECTION_INITIALISED) {
                continue;
            }
            if (out->sects[i].attr.fill_type == BIN_SECTION_ONE_ED) {
                error_log("Sections of 'oned' type currently not supported\n");
                continue;
            }
            if (((out->sects[i].attr.readable << 2)
               | (out->sects[i].attr.writable << 1)
               | (out->sects[i].attr.executable)) != j) {
                continue;
            }

            if (out->sects[i].attr.entry_point){
                warn_log("Entrypoint in non-initialised section. Is it OK?\n");
                if (*entrypoint) {
                    error_log("Multiple entrypoint sects defined. Merge them first\n");
                    return -1;
                }
                *entrypoint = *mem_addr;
            }

            sect_offs[i] = *file_offset;
            sect_addr[i] = *mem_addr;

            prog_hdrs[segs_used-1].p_memsz += out->sects[i].size;
            *mem_addr += prog_hdrs[segs_used-1].p_memsz;
        }   
    }
    return segs_used;
}

int writeElfFile(ExecOutput*  out, FILE* file) {
    Elf64_Ehdr header = {};
    elfFill_e_ident(&header);
    const int max_ph_cnt = 10;

    header.e_type = ET_EXEC;
    header.e_machine = EM_X86_64;
    header.e_version = EV_CURRENT;
    header.e_ehsize = sizeof(Elf64_Ehdr);
    header.e_phentsize = sizeof(Elf64_Phdr);
    header.e_shentsize = sizeof(Elf64_Shdr);
    header.e_phoff = sizeof(header);
    header.e_shoff = header.e_phoff + (max_ph_cnt * header.e_phentsize);
    header.e_shnum = out->sect_count + 1;
    
    uint64_t sect_place_offset = header.e_phoff + (header.e_phentsize * max_ph_cnt) + (header.e_shnum * header.e_shentsize);
    //uint64_t sect_place_offset = header.e_phoff + (header.e_phnum * header.e_phentsize);
    uint64_t prog_start_addr   = 0x400000;    

    Elf64_Phdr* prog_hdrs = (Elf64_Phdr*)calloc(max_ph_cnt , sizeof(*prog_hdrs));
    size_t*   sect_offs  = (size_t*  )calloc(out->sect_count, sizeof(size_t  ));
    uint64_t* sect_addr  = (uint64_t*)calloc(out->sect_count, sizeof(uint64_t));

    if (!(prog_hdrs && sect_offs && sect_addr)) {
        if (prog_hdrs)
            free(prog_hdrs);
        if (sect_offs)
            free(sect_offs);
        if (sect_addr)
            free(sect_addr);
        return -1;
    }

    prog_hdrs[0].p_type = PT_LOAD;
    prog_hdrs[0].p_flags = PF_R;
    prog_hdrs[0].p_vaddr = prog_start_addr;
    prog_hdrs[0].p_filesz = sect_place_offset;
    prog_hdrs[0].p_memsz  = sect_place_offset;
    prog_hdrs[0].p_offset = 0;
    prog_hdrs[0].p_align = 0x1000;

    uint64_t curr_file_offset = sect_place_offset;
    size_t   curr_mem_addr    = sect_place_offset + prog_start_addr;

    int segs_used = 1 + composeSegments(out, prog_hdrs, sect_offs, sect_addr, &curr_mem_addr, &curr_file_offset, &(header.e_entry));
    if (segs_used < 1) {
        free(prog_hdrs);
        free(sect_offs);
        free(sect_addr);
        return -1;
    }    

    prog_hdrs[segs_used].p_type = PT_GNU_STACK;
    prog_hdrs[segs_used].p_flags = PF_R | PF_W;
    prog_hdrs[segs_used].p_align = 0x10;
    segs_used++;
    header.e_phnum = segs_used;

    if (!(header.e_entry)) {
        error_log("No entrypoint defined");
        free(prog_hdrs);
        return 1;
    }
    header.e_shstrndx = header.e_shnum-1;

    if (fwrite(&header  , sizeof(header)    , 1         , file) != 1
     || fwrite(prog_hdrs, sizeof(*prog_hdrs), max_ph_cnt, file) != max_ph_cnt) {

        free(prog_hdrs);
        free(sect_offs);
        free(sect_addr);
        return -2;
    }

    free(prog_hdrs);    

    Elf64_Shdr* sect_hdrs = (Elf64_Shdr*)calloc(header.e_shnum , sizeof(*sect_hdrs));

    if (!sect_hdrs) {
        free(sect_offs);
        free(sect_addr);
        return -1;
    }
    size_t curr_shstr_pos = 1;

    const char* entryp_sect_str = ".text";
    const char* shstr_sect_str  = ".shstrtab";

    for (size_t i = 0; i < out->sect_count; i++) {
        sect_hdrs[i].sh_name = curr_shstr_pos;
        sect_hdrs[i].sh_type = (out->sects[i].attr.fill_type == BIN_SECTION_INITIALISED) ? SHT_PROGBITS : SHT_NOBITS;
        sect_hdrs[i].sh_offset = sect_offs[i];
        sect_hdrs[i].sh_flags  = (out->sects[i].attr.fill_type == BIN_SECTION_INITIALISED ? SHF_ALLOC : 0)
                               | (out->sects[i].attr.executable ? SHF_EXECINSTR : 0)
                               | (out->sects[i].attr.writable   ? SHF_WRITE     : 0);
        sect_hdrs[i].sh_addr = sect_addr[i];
        sect_hdrs[i].sh_size = prog_hdrs[out->sect_count].p_filesz;
        sect_hdrs[i].sh_addralign = 0;
        if (out->sects[i].attr.entry_point) {
            curr_shstr_pos += strlen(entryp_sect_str)+1;
        }
        else{
            curr_shstr_pos += strlen(out->sects[i].name)+1;
        }
    }

    sect_hdrs[header.e_shnum-1].sh_name = curr_shstr_pos;
    curr_shstr_pos += strlen(shstr_sect_str) + 1;
    sect_hdrs[header.e_shnum-1].sh_type = SHT_STRTAB; 
    sect_hdrs[header.e_shnum-1].sh_offset = curr_file_offset;
    sect_hdrs[header.e_shnum-1].sh_size = curr_shstr_pos;

    if (fwrite(sect_hdrs, sizeof(*sect_hdrs), header.e_shnum, file) != header.e_shnum) {
        free(sect_hdrs);
        free(sect_offs);
        free(sect_addr);
        return -2;
    }    
    free(sect_hdrs);

    void* file_sects_buffer = calloc(curr_file_offset - sect_place_offset, 1);

    for (size_t i = 0; i < out->sect_count; i++){
        if (out->sects[i].attr.fill_type == BIN_SECTION_INITIALISED){
            memcpy(((char*)file_sects_buffer) + sect_offs[i] - sect_place_offset, out->sects[i].data, out->sects[i].size);
        }
    }

    for (RelocationEntry* rel = out->relt.data + out->relt.size-1; rel >= out->relt.data; rel--){
        size_t offset = (size_t)(sect_addr[rel->relative_to]);
        if (out->sects[rel->sect_id].attr.fill_type != BIN_SECTION_INITIALISED) {
            error_log("Relocation in uninitialised section\n");
        }
        if (rel->relative) {
            offset -= sect_addr[rel->sect_id];
        }
        if (!relocationEntryApply(rel, ((char*)file_sects_buffer) + sect_offs[rel->sect_id] - sect_place_offset, offset)){
            free(file_sects_buffer);
            free(sect_offs);
            free(sect_addr);
            return 1;
        }
    }
    free(sect_offs);
    free(sect_addr);    

    if (fwrite(file_sects_buffer, curr_file_offset - sect_place_offset, 1, file) != 1) {
        free(file_sects_buffer);
        return -2;
    }
    free(file_sects_buffer);

    fprintf(file, "%c", 0);
    for (size_t i = 0; i < out->sect_count; i++){
        if (out->sects[i].attr.entry_point) {
            if (fprintf(file, "%s%c", entryp_sect_str, 0) < 0) {
                return -2;
            }
        }
        else{
            if (fprintf(file, "%s%c", out->sects[i].name , 0) < 0) {
                return -2;
            }
        }
    }

    if (fprintf(file, "%s%c", shstr_sect_str, 0)) {
        return -2;
    }
    return 0;
}

int createExecElfFile(ExecOutput*  out, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file)
        return false;
    int ret = writeElfFile(out, file);
    fclose(file);
    return ret;
}