.text
.globl _start
.extern _rcc_entry
_start:
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    xor %ebx, %ebx
    jmp .Lexit
.Lfailure:
    mov $1, %ebx
.Lexit:
    mov $1, %eax
    int $0x80
