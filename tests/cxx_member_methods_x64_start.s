.text
.globl _start
.globl abort
.extern _rcc_entry
abort:
    mov $134, %edi
    mov $60, %eax
    syscall
    ud2

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
