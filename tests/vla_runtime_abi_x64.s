.text
.globl stack_probe_vla_boundaries_abi
stack_probe_vla_boundaries_abi:
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
    call stack_probe_vla_boundaries
    test %eax, %eax
    jne .Lvla_abi_fail
    movabs $0x13579bdf2468ace0, %rax
    cmp %rax, %rbx
    jne .Lvla_abi_fail
    movabs $0x55aa33cc6d3a91e7, %rax
    cmp %rax, %r12
    jne .Lvla_abi_fail
    movabs $0x7142a3b4c5d6e7f8, %rax
    cmp %rax, %r13
    jne .Lvla_abi_fail
    movabs $0x89abcdef01234567, %rax
    cmp %rax, %r14
    jne .Lvla_abi_fail
    movabs $0xfedcba9876543210, %rax
    cmp %rax, %r15
    jne .Lvla_abi_fail
    movabs $0x1029384756abcdef, %rax
    cmp %rax, %rbp
    jne .Lvla_abi_fail
    xor %eax, %eax
    jmp .Lvla_abi_done
.Lvla_abi_fail:
    mov $1, %eax
.Lvla_abi_done:
    add $8, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
