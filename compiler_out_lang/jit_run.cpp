#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

#include "lib/logging.h"
#include "exec_output.h"

void execOutputJitRun(ExecOutput* out) {
    //first, relocate everything to actual adresses
    for (RelocationEntry* rel = out->relt.data + out->relt.size; rel >= out->relt.data; rel--){
        assert(rel->size > 8);
        size_t offset = (size_t)(out->sects[rel->rel_to_sect].data);
        size_t* rel_ptr = (size_t*)(((char*)out->sects[rel->sect_id].data) + rel->sect_offset);
        *rel_ptr = ((*rel_ptr + offset) & (-1L >> (8*(8-rel->size))))
                    | ((*rel_ptr) & (-1L >> rel->size));
    }

    //then, edit section protection and get 'main' section
    void* entry_point = nullptr;
    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count-1; sect++){
        if (sect->size > 0) {
            if (0 != mprotect(sect->data, sect->size, 
                (sect->attr.readable   ? PROT_READ : 0)
            | (sect->attr.writable   ? PROT_WRITE: 0)
            | (sect->attr.executable ? PROT_EXEC : 0)
            )) {
                perror_log("Memory protect failed\n");
                return;
            }
            if (sect->attr.entry_point) {
                if (!entry_point){
                    entry_point = sect->data;
                }
                else {
                    error_log("More than one entrypoint section defined. execOutPrepareCode() can probably fix this.\n");
                    return;
                }
            }
        }
    }
    if (!entry_point) {
        error_log("Run failed: no entrypoint section\n");
        return;
    }

    ((void (*)())entry_point)(); //RUN
    
    info_log("How did we get there?");
    return;
}