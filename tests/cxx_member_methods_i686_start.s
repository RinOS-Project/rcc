.text
.globl _start
.globl abort
.extern _rcc_entry
abort:
    mov $134, %ebx
    mov $1, %eax
    int $0x80
    ud2

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
