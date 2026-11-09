.text
.globl _start
.extern _rcc_entry
_start:
    movabs $0x13579bdf2468ace0, %rbx
    movabs $0x55aa33cc6d3a91e7, %r12
    movabs $0x7142a3b4c5d6e7f8, %r13
    movabs $0x89abcdef01234567, %r14
    movabs $0xfedcba9876543210, %r15
    movabs $0x1029384756abcdef, %rbp
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    movabs $0x13579bdf2468ace0, %rax
    cmp %rax, %rbx
    jne .Lfailure
    movabs $0x55aa33cc6d3a91e7, %rax
    cmp %rax, %r12
    jne .Lfailure
    movabs $0x7142a3b4c5d6e7f8, %rax
    cmp %rax, %r13
    jne .Lfailure
    movabs $0x89abcdef01234567, %rax
    cmp %rax, %r14
    jne .Lfailure
    movabs $0xfedcba9876543210, %rax
    cmp %rax, %r15
    jne .Lfailure
    movabs $0x1029384756abcdef, %rax
    cmp %rax, %rbp
    jne .Lfailure
    xor %edi, %edi
    jmp .Lexit
.Lfailure:
    mov $1, %edi
.Lexit:
    mov $60, %eax
    syscall
