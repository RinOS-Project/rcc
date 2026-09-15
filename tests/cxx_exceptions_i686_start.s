.text
.globl _start
.globl setjmp
.globl longjmp
.globl rin_cpp_exception_install
.globl rin_cpp_exception_leave
.globl rin_cpp_exception_throw
.globl rin_cpp_exception_rethrow
.globl rin_cpp_exception_throw_object
.globl rin_cpp_exception_rethrow_frame
.globl rin_cpp_exception_release_frame
.extern _rcc_entry

.bss
.align 4
rin_cpp_exception_top:
    .long 0
rin_cpp_exception_current_value:
    .long 0
rin_cpp_exception_current_type:
    .long 0
rin_cpp_exception_object_used:
    .long 0
rin_cpp_exception_object_storage:
    .space 4096

.text
setjmp:
    mov 4(%esp), %eax
    mov %ebx, 0(%eax)
    mov %esi, 4(%eax)
    mov %edi, 8(%eax)
    mov %ebp, 12(%eax)
    lea 4(%esp), %edx
    mov %edx, 16(%eax)
    mov (%esp), %edx
    mov %edx, 20(%eax)
    xor %eax, %eax
    ret

longjmp:
    mov 4(%esp), %edx
    mov 8(%esp), %eax
    test %eax, %eax
    jnz 1f
    mov $1, %eax
1:
    mov 0(%edx), %ebx
    mov 4(%edx), %esi
    mov 8(%edx), %edi
    mov 12(%edx), %ebp
    mov 16(%edx), %esp
    mov 20(%edx), %ecx
    jmp *%ecx

rin_cpp_exception_install:
    mov 4(%esp), %eax
    mov rin_cpp_exception_top, %edx
    mov %edx, 24(%eax)
    mov %eax, rin_cpp_exception_top
    ret

rin_cpp_exception_leave:
    mov 4(%esp), %eax
    cmp %eax, rin_cpp_exception_top
    jne 2f
    mov 24(%eax), %edx
    mov %edx, rin_cpp_exception_top
2:
    ret

rin_cpp_exception_throw:
    mov rin_cpp_exception_top, %edx
    test %edx, %edx
    jz 3f
    mov 4(%esp), %eax
    mov %eax, 28(%edx)
    mov %eax, rin_cpp_exception_current_value
    mov 8(%esp), %eax
    mov %eax, 32(%edx)
    mov %eax, rin_cpp_exception_current_type
    mov 24(%edx), %eax
    mov %eax, rin_cpp_exception_top
    push $1
    push %edx
    call longjmp
    ud2

rin_cpp_exception_rethrow:
    mov rin_cpp_exception_current_value, %eax
    push rin_cpp_exception_current_type
    push %eax
    call rin_cpp_exception_throw
    ud2

rin_cpp_exception_throw_object:
    mov 8(%esp), %ecx
    test %ecx, %ecx
    jz 5f
    mov rin_cpp_exception_object_used, %eax
    mov %eax, %edx
    add %ecx, %edx
    jc 5f
    cmp $4096, %edx
    ja 5f
    lea rin_cpp_exception_object_storage(%eax), %edi
    mov 4(%esp), %esi
    test %esi, %esi
    jz 5f
    push %ecx
    cld
    rep movsb
    pop %ecx
    mov %edx, rin_cpp_exception_object_used
    lea rin_cpp_exception_object_storage(%eax), %eax
    mov 12(%esp), %edx
    push %edx
    push %eax
    call rin_cpp_exception_throw
    add $8, %esp
    ud2

rin_cpp_exception_rethrow_frame:
    mov 4(%esp), %edx
    mov 28(%edx), %eax
    push 32(%edx)
    push %eax
    call rin_cpp_exception_throw
    ud2

rin_cpp_exception_release_frame:
    mov 4(%esp), %eax
    test %eax, %eax
    jz 6f
    mov 32(%eax), %edx
    test $0x80000000, %edx
    jz 6f
    mov $0, 28(%eax)
    mov $0, 32(%eax)
    mov $0, rin_cpp_exception_object_used
6:
    ret
3:
    mov $1, %ebx
    mov $1, %eax
    int $0x80
    ud2
5:
    mov $134, %ebx
    mov $1, %eax
    int $0x80
    ud2

_start:
    call _rcc_entry
    test %eax, %eax
    jne 4f
    xor %ebx, %ebx
    mov $1, %eax
    int $0x80
4:
    mov $1, %ebx
    mov $1, %eax
    int $0x80
