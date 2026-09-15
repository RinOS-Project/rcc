.text
.globl _start
.extern bitfield_static
.extern bitfield_runtime
_start:
    call bitfield_static
    test %eax, %eax
    jne .Lfailure
    call bitfield_runtime
    test %eax, %eax
    jne .Lfailure
    xor %edi, %edi
    mov $60, %eax
    syscall
.Lfailure:
    mov $1, %edi
    mov $60, %eax
    syscall
