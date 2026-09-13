.bss
.align 4
.Lheap:
    .zero 64

.text
.globl _start
.globl rin_malloc
.globl rin_free
.extern _rcc_entry

rin_malloc:
    mov $.Lheap, %eax
    ret

rin_free:
    ret

_start:
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    xor %ebx, %ebx
    mov $1, %eax
    int $0x80
.Lfailure:
    mov $1, %ebx
    mov $1, %eax
    int $0x80
