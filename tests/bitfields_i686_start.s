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
    xor %ebx, %ebx
    mov $1, %eax
    int $0x80
.Lfailure:
    mov $1, %ebx
    mov $1, %eax
    int $0x80
