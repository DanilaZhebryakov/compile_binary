#include <stddef.h>

struct InstructionBinCode {
    size_t size;
    const char* data;
};

#define INSTR(str) \
{sizeof(str), str},

static const InstructionBinCode ginstrBinCodes[] = {
     /*BAD*/INSTR("\xF1") //invalid
     /*NOP*/INSTR("\x90") //nop
     /*ADD*/INSTR("\x5a" "\x58"   // pop rdx; pop rax
                  "\x48\x01\xd0"  // add rax, rdx
                  "\x50"          // push rax
                )

     /*SUB*/INSTR("\x5a" "\x58"   // pop rdx; pop rax
                  "\x48\x29\xd0"  // add rax, rdx
                  "\x50"          // push rax
                )
     /*MUL*/INSTR("\x59" "\x58"   // pop rcx; pop rax
                  "\x48\x31\xd2"  // xor rdx, rdx
                  "\x48\xf7\xe1"  // mul rcx
                  "\x50"          // push rax
                )
     /*DIV*/INSTR("\x59" "\x58"   // pop rcx; pop rax
                  "\x48\x31\xd2"  // xor rdx, rdx
                  "\x48\xf7\xf1"  // div rcx
                  "\x50"          // push rax
                )
     /*END*/INSTR("\x48\x89\xEC"    // mov rsp, rbp
                  "\x5d"            // pop rbp
                  "\xb8\x3c\x00\x00\x00"// mov eax, 0x3c (exit)
                  "\x5f"            // pop rdi
                  "\x0f\x05"        // syscall
                )

     /*DUP*/INSTR("\xff\x34\x24") //push qword(rsp)
     /*POP*/INSTR("\x48\x83\xC4\x08") // add rsp,8

     /*INP*/INSTR("\x6A\x00"       // push(0) 
                  "\xb8\0\0\0\0"   // mov eax, 0
                  "\xBF\0\0\0\0"   // mov edi, 0
                  "\x48\x89\xE6"   // mov rsi, rsp
                  "\xBA\x01\0\0\0" // mov edx, 1
                  "\x0F\x05"       // syscall
                )
     /*OUT*/INSTR("\xB8\x01\0\0\0"   // mov eax, 1
                  "\xBF\x01\0\0\0"   // mov edi, 1
                  "\x48\x89\xE6"     // mov rsi, rsp
                  "\xBA\x01\0\0\0"   // mov edx, 1
                  "\x0F\x05"         // syscall
                  "\x48\x83\xC4\x08" // add rsp, 8
                )

    /*TRAP*/INSTR("\xCC") //int 3 (trap)
     /*CMP*/INSTR("\x5a" "\x58"  // pop rdx; pop rax
                  "\x48\x39\xd0" // cmp rax, rdx
                )
     /*TST*/INSTR("\x5a" "\x58"  // pop rdx; pop rax
                  "\x48\x85\xd0" // test rax, rdx
                )
};
const int g_instr_cnt = sizeof(ginstrBinCodes) / sizeof(InstructionBinCode);

static const InstructionBinCode longJmpBinCodes[] = {
    /*NVR*/ INSTR("\x3D") // actually "cmp eax, imm32" , but this is good 'nop' for this case
    /*ALW*/ INSTR("\xE9") // jmp (imm32)
    /*EQ */ INSTR("\x0F\x84")
    /*NE */ INSTR("\x0F\x85")
    /*GT */ INSTR("\x0F\x8F")
    /*LE */ INSTR("\x0F\x8E")
    /*GE */ INSTR("\x0F\x8D")
    /*LT */ INSTR("\x0F\x8C")
};

static const InstructionBinCode shortJmpBinCodes[] = {
    /*NVR*/ INSTR("\x3C") // actually "cmp al, imm8" , but this is good 'nop' for this case
    /*ALW*/ INSTR("\xEB") // jmp (imm8)
    /*EQ */ INSTR("\x74")
    /*NE */ INSTR("\x75")
    /*GT */ INSTR("\x7F")
    /*LE */ INSTR("\x7E")
    /*GE */ INSTR("\x7D")
    /*LT */ INSTR("\x7C")
};

#undef INSTR