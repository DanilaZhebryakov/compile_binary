#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <limits.h> 

#include "lib/logging.h"
#include "exec_output.h"

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

static size_t round_to_pages(size_t i){
    return i + (PAGESIZE-1 - ((i-1) % PAGESIZE));
}

bool execOutputJitRun(ExecOutput* out) {
    bool ret = true;
    // first, allocate new buffers and copy stuff there

    void** buffer_ptrs = (void**)calloc(out->sect_count, sizeof(void*));
    for (size_t i = 0; i < out->sect_count; i++){
        buffer_ptrs[i] = aligned_alloc(PAGESIZE, round_to_pages(out->sects[i].size));
        switch (out->sects[i].attr.fill_type) {
            case BIN_SECTION_INITIALISED:
                memcpy(buffer_ptrs[i], out->sects[i].data, out->sects[i].size);
                break;
            case BIN_SECTION_ZEROED:
                memset(buffer_ptrs[i], 0, out->sects[i].size);
                break;
            case BIN_SECTION_ONE_ED:
                memset(buffer_ptrs[i], 0xFF, out->sects[i].size);
                break;
            case BIN_SECTION_UNINITIALISED:
                break;
            default:
                error_log("Unknown filltype %d", out->sects[i].attr.fill_type);
                ret = false;
                break;
        }
    }

    if (!ret){
        for (size_t i = 0; i < out->sect_count; i++){
            free(buffer_ptrs[i]);
        }
        free(buffer_ptrs);
        return false;
    }

    //then, relocate everything to actual adresses of these buffers
    for (RelocationEntry* rel = out->relt.data + out->relt.size-1; rel >= out->relt.data; rel--){
        assert(rel->size <= 8);
        size_t offset = (size_t)(buffer_ptrs[rel->relative_to]);

        if (!relocationEntryApply(rel, buffer_ptrs[rel->sect_id], offset)){
            ret = false;
            break;
        }
    }

    //then, edit section protection and get 'main' section
    void* entry_point = nullptr;
    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
        if (sect->size > 0) {
            if (0 != mprotect(buffer_ptrs[sect - out->sects], sect->size, 
                (sect->attr.readable   ? PROT_READ : 0)
            | (sect->attr.writable   ? PROT_WRITE: 0)
            | (sect->attr.executable ? PROT_EXEC : 0)
            )) {
                perror_log("Memory protect failed\n");
                ret = false;
                break;
            }
            if (sect->attr.entry_point) {
                if (!entry_point){
                    entry_point = buffer_ptrs[sect - out->sects];
                }
                else {
                    error_log("More than one entrypoint section defined. execOutPrepareCode() can probably fix this.\n");
                    ret = false;
                    break;
                }
            }
        }
    }

    if (!entry_point) {
        error_log("Run failed: no entrypoint section\n");
        ret = false;
    }
    if (ret){
        fprintf(stderr, "Running at: %p", entry_point);
        ((void (*)())entry_point)(); //RUN
    }

    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
        if (sect->size > 0) {
            if (0 != mprotect(buffer_ptrs[sect - out->sects], sect->size, PROT_READ | PROT_WRITE)) {
                perror_log("Memory un-protect failed\n");
                ret = false;
            }
            else{
                free(buffer_ptrs[sect - out->sects]);
            }
        }
    }
    free(buffer_ptrs);
    return ret;
}