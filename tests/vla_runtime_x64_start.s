.text
.globl _start
.extern _rcc_entry
_start:
    call _rcc_entry
    test %eax, %eax
    jne .Lfailure
    xor %edi, %edi
    jmp .Lexit
.Lfailure:
    mov $1, %edi
.Lexit:
    mov $60, %eax
    syscall
