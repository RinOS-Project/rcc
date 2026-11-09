.text
.globl compiler_static_stack_probe_abi
compiler_static_stack_probe_abi:
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov $0x13579bdf, %ebx
    mov $0x2468ace0, %esi
    mov $0x55aa33cc, %edi
    mov $0x6d3a91e7, %ebp
    call compiler_static_stack_probe
    test %eax, %eax
    jne .Lstatic_abi_fail
    cmp $0x13579bdf, %ebx
    jne .Lstatic_abi_fail
    cmp $0x2468ace0, %esi
    jne .Lstatic_abi_fail
    cmp $0x55aa33cc, %edi
    jne .Lstatic_abi_fail
    cmp $0x6d3a91e7, %ebp
    jne .Lstatic_abi_fail
    xor %eax, %eax
    jmp .Lstatic_abi_done
.Lstatic_abi_fail:
    mov $1, %eax
.Lstatic_abi_done:
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
