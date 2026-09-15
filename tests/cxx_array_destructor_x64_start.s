.bss
.align 8
.Lheap:
    .zero 64
.text
.globl _start
.globl rin_malloc
.globl rin_free
.extern _rcc_entry

rin_malloc:
    lea .Lheap(%rip), %rax
    ret

rin_free:
    ret

_start:
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    xor %edi, %edi
    mov $60, %eax
    syscall
.Lfailure:
    mov $1, %edi
    mov $60, %eax
    syscall
