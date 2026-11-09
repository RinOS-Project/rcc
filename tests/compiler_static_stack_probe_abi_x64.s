.text
.globl compiler_static_stack_probe_abi
compiler_static_stack_probe_abi:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    sub $8, %rsp
    movabs $0x13579bdf2468ace0, %rbx
    movabs $0x55aa33cc6d3a91e7, %r12
    movabs $0x7142a3b4c5d6e7f8, %r13
    movabs $0x89abcdef01234567, %r14
    movabs $0xfedcba9876543210, %r15
    movabs $0x1029384756abcdef, %rbp
    call compiler_static_stack_probe
    test %eax, %eax
    jne .Lstatic_abi_fail
    movabs $0x13579bdf2468ace0, %rax
    cmp %rax, %rbx
    jne .Lstatic_abi_fail
    movabs $0x55aa33cc6d3a91e7, %rax
    cmp %rax, %r12
    jne .Lstatic_abi_fail
    movabs $0x7142a3b4c5d6e7f8, %rax
    cmp %rax, %r13
    jne .Lstatic_abi_fail
    movabs $0x89abcdef01234567, %rax
    cmp %rax, %r14
    jne .Lstatic_abi_fail
    movabs $0xfedcba9876543210, %rax
    cmp %rax, %r15
    jne .Lstatic_abi_fail
    movabs $0x1029384756abcdef, %rax
    cmp %rax, %rbp
    jne .Lstatic_abi_fail
    xor %eax, %eax
    jmp .Lstatic_abi_done
.Lstatic_abi_fail:
    mov $1, %eax
.Lstatic_abi_done:
    add $8, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
