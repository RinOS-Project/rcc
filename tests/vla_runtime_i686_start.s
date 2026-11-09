.text
.globl _start
.extern _rcc_entry
_start:
    mov $0x13579bdf, %ebx
    mov $0x2468ace0, %esi
    mov $0x55aa33cc, %edi
    mov $0x6d3a91e7, %ebp
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    cmp $0x13579bdf, %ebx
    jne .Lfailure
    cmp $0x2468ace0, %esi
    jne .Lfailure
    cmp $0x55aa33cc, %edi
    jne .Lfailure
    cmp $0x6d3a91e7, %ebp
    jne .Lfailure
    xor %ebx, %ebx
    jmp .Lexit
.Lfailure:
    mov $1, %ebx
.Lexit:
    mov $1, %eax
    int $0x80
