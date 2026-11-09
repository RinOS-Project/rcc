.text
.globl stack_probe_vla_boundaries_abi
stack_probe_vla_boundaries_abi:
    push %ebp
    push %ebx
    push %esi
    push %edi
    mov $0x13579bdf, %ebx
    mov $0x2468ace0, %esi
    mov $0x55aa33cc, %edi
    mov $0x6d3a91e7, %ebp
    call stack_probe_vla_boundaries
    test %eax, %eax
    jne .Lvla_abi_fail
    cmp $0x13579bdf, %ebx
    jne .Lvla_abi_fail
    cmp $0x2468ace0, %esi
    jne .Lvla_abi_fail
    cmp $0x55aa33cc, %edi
    jne .Lvla_abi_fail
    cmp $0x6d3a91e7, %ebp
    jne .Lvla_abi_fail
    xor %eax, %eax
    jmp .Lvla_abi_done
.Lvla_abi_fail:
    mov $1, %eax
.Lvla_abi_done:
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
