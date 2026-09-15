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
.globl rin_cpp_exception_register_cleanup
.globl rin_cpp_exception_unregister_cleanup
.globl rin_cpp_exception_unwind_cleanups
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
rin_cpp_exception_cleanup_used:
    .long 0
rin_cpp_exception_cleanup_storage:
    .space 768

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

rin_cpp_exception_register_cleanup:
    mov 4(%esp), %eax
    mov 8(%esp), %ecx
    mov 12(%esp), %edx
    test %eax, %eax
    jz 5f
    test %ecx, %ecx
    jz 5f
    test %edx, %edx
    jz 5f
    mov rin_cpp_exception_cleanup_used, %edx
    mov %edx, %ecx
    add $12, %ecx
    jc 5f
    cmp $768, %ecx
    jae 5f
    lea rin_cpp_exception_cleanup_storage(%edx), %ecx
    mov 8(%esp), %edx
    mov %edx, 0(%ecx)
    mov 12(%esp), %edx
    mov %edx, 4(%ecx)
    mov 36(%eax), %edx
    mov %edx, 8(%ecx)
    mov %ecx, 36(%eax)
    mov rin_cpp_exception_cleanup_used, %edx
    add $12, %edx
    mov %edx, rin_cpp_exception_cleanup_used
    ret

rin_cpp_exception_unregister_cleanup:
    push %esi
    push %edi
    mov 12(%esp), %eax
    mov 16(%esp), %ecx
    mov 20(%esp), %edx
    test %eax, %eax
    jz 5f
    test %ecx, %ecx
    jz 5f
    test %edx, %edx
    jz 5f
    mov 36(%eax), %edi
    xor %esi, %esi
7:
    test %edi, %edi
    jz 5f
    cmp %ecx, 0(%edi)
    jne 8f
    cmp %edx, 4(%edi)
    je 9f
8:
    mov %edi, %esi
    mov 8(%edi), %edi
    jmp 7b
9:
    mov 8(%edi), %edx
    test %esi, %esi
    jnz 10f
    mov %edx, 36(%eax)
    pop %edi
    pop %esi
    ret
10:
    mov %edx, 8(%esi)
    pop %edi
    pop %esi
    ret

rin_cpp_exception_unwind_cleanups:
    mov 4(%esp), %eax
    test %eax, %eax
    jz 5f
    mov 36(%eax), %edx
    movl $0, 36(%eax)
11:
    test %edx, %edx
    jz 12f
    mov 8(%edx), %ecx
    mov 4(%edx), %eax
    push %ecx
    push %eax
    call *0(%edx)
    add $4, %esp
    pop %edx
    jmp 11b
12:
    ret

rin_cpp_exception_install:
    mov 4(%esp), %eax
    mov rin_cpp_exception_top, %edx
    mov %edx, 24(%eax)
    movl $0, 36(%eax)
    mov %eax, rin_cpp_exception_top
    ret

rin_cpp_exception_leave:
    mov 4(%esp), %eax
    cmp %eax, rin_cpp_exception_top
    jne 2f
    mov 24(%eax), %edx
    mov %edx, rin_cpp_exception_top
    push %eax
    call rin_cpp_exception_unwind_cleanups
    add $4, %esp
2:
    ret

rin_cpp_exception_throw:
    mov rin_cpp_exception_top, %edx
    test %edx, %edx
    jz 3f
    mov 24(%edx), %eax
    mov %eax, rin_cpp_exception_top
    push %edx
    push %edx
    call rin_cpp_exception_unwind_cleanups
    add $4, %esp
    pop %edx
    mov 4(%esp), %eax
    mov %eax, 28(%edx)
    mov %eax, rin_cpp_exception_current_value
    mov 8(%esp), %eax
    mov %eax, 32(%edx)
    mov %eax, rin_cpp_exception_current_type
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
    movl $0, 28(%eax)
    movl $0, 32(%eax)
    movl $0, rin_cpp_exception_object_used
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
