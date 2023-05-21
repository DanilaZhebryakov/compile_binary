section .text

    .add:
        pop rdx
        pop rax
	    add rax, rdx
        push rax
    .out:
        mov rax, 0x01 ;write
        mov rdi, 1    ;stdout
        mov rsi, rsp
        mov rdx, 1
        syscall
        add rsp, 8
    .in:
        push qword 0
        mov rax, 0x00 ;read
        mov rdi, 0    ;stdin
        mov rsi, rsp
        mov rdx, 1
        syscall

    .mul:
        pop rcx
        pop rax
        xor rdx, rdx
        mul rcx
        push rax

    .div:
        pop rcx
        pop rax
        xor rdx, rdx
        div rcx
        push rax

    .end:
        mov rsp, rbp
        pop rbp
        mov rax, 0x3C ;exit
        pop rdi       ;exitcode
        syscall

    .pop:
        add rsp, 8
    .dup:
        push qword[rsp]
    .entr:
        enter 0, 1
        leave
        
        add rsp, 8
        add rsp, 0x0BAADF
        sub rsp, 8
        sub rsp, 0x0BAADF
        nop
    .suff:
        mov rsp, rbp
        pop rbp
        mov rax, 0x3C ;exit
        xor rdi, rdi
        syscall
        cmp rax, rdx
        test rax, rdx
        pop r9