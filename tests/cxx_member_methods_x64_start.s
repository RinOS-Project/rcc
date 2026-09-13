.text
.globl _start
.extern _rcc_entry
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
