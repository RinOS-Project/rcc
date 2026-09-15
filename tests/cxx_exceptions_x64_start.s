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
.globl rin_malloc
.globl rin_free
.extern _rcc_entry

.bss
.align 8
rin_cpp_exception_top:
    .quad 0
rin_cpp_exception_current_value:
    .quad 0
rin_cpp_exception_current_type:
    .quad 0
rin_cpp_exception_object_used:
    .quad 0
rin_cpp_exception_object_storage:
    .space 4096
rin_cpp_exception_cleanup_used:
    .quad 0
rin_cpp_exception_cleanup_storage:
    .space 1536
.align 8
rin_exception_test_heap:
    .space 128

.text
setjmp:
    mov %rdi, %rax
    mov %rbx, 0(%rax)
    mov %rbp, 8(%rax)
    mov %r12, 16(%rax)
    mov %r13, 24(%rax)
    mov %r14, 32(%rax)
    mov %r15, 40(%rax)
    lea 8(%rsp), %rdx
    mov %rdx, 48(%rax)
    mov (%rsp), %rdx
    mov %rdx, 56(%rax)
    xor %eax, %eax
    ret

longjmp:
    mov %rdi, %rdx
    mov %esi, %eax
    test %eax, %eax
    jnz 1f
    mov $1, %eax
1:
    mov 0(%rdx), %rbx
    mov 8(%rdx), %rbp
    mov 16(%rdx), %r12
    mov 24(%rdx), %r13
    mov 32(%rdx), %r14
    mov 40(%rdx), %r15
    mov 48(%rdx), %rsp
    mov 56(%rdx), %rcx
    jmp *%rcx

rin_cpp_exception_register_cleanup:
    test %rdi, %rdi
    jz 5f
    test %rsi, %rsi
    jz 5f
    test %rdx, %rdx
    jz 5f
    mov rin_cpp_exception_cleanup_used(%rip), %rax
    mov %rax, %r10
    add $24, %r10
    jc 5f
    cmp $1536, %r10
    jae 5f
    lea rin_cpp_exception_cleanup_storage(%rip), %r10
    add %rax, %r10
    mov %rsi, 0(%r10)
    mov %rdx, 8(%r10)
    mov 88(%rdi), %rax
    mov %rax, 16(%r10)
    mov %r10, 88(%rdi)
    mov rin_cpp_exception_cleanup_used(%rip), %rax
    add $24, %rax
    mov %rax, rin_cpp_exception_cleanup_used(%rip)
    ret

rin_cpp_exception_unregister_cleanup:
    test %rdi, %rdi
    jz 5f
    test %rsi, %rsi
    jz 5f
    test %rdx, %rdx
    jz 5f
    mov 88(%rdi), %rax
    xor %r8d, %r8d
7:
    test %rax, %rax
    jz 5f
    cmp %rsi, 0(%rax)
    jne 8f
    cmp %rdx, 8(%rax)
    je 9f
8:
    mov %rax, %r8
    mov 16(%rax), %rax
    jmp 7b
9:
    mov 16(%rax), %rcx
    test %r8, %r8
    jnz 10f
    mov %rcx, 88(%rdi)
    ret
10:
    mov %rcx, 16(%r8)
    ret

rin_cpp_exception_unwind_cleanups:
    test %rdi, %rdi
    jz 5f
    mov 88(%rdi), %rax
    movq $0, 88(%rdi)
11:
    test %rax, %rax
    jz 12f
    mov 16(%rax), %rcx
    mov 8(%rax), %rdx
    mov 0(%rax), %r8
    push %rcx
    mov %rdx, %rdi
    call *%r8
    pop %rax
    jmp 11b
12:
    ret

rin_cpp_exception_install:
    mov rin_cpp_exception_top(%rip), %rax
    mov %rax, 64(%rdi)
    movq $0, 88(%rdi)
    mov %rdi, rin_cpp_exception_top(%rip)
    ret

rin_cpp_exception_leave:
    mov rin_cpp_exception_top(%rip), %rax
    cmp %rdi, %rax
    jne 2f
    mov 64(%rdi), %rax
    mov %rax, rin_cpp_exception_top(%rip)
    call rin_cpp_exception_unwind_cleanups
2:
    ret

rin_cpp_exception_throw:
    mov rin_cpp_exception_top(%rip), %rdx
    test %rdx, %rdx
    jz 3f
    mov 64(%rdx), %rax
    mov %rax, rin_cpp_exception_top(%rip)
    push %rsi
    push %rdi
    push %rdx
    mov %rdx, %rdi
    call rin_cpp_exception_unwind_cleanups
    pop %rdx
    pop %rdi
    pop %rsi
    mov %rdi, 72(%rdx)
    mov %rdi, rin_cpp_exception_current_value(%rip)
    mov %rsi, 80(%rdx)
    mov %rsi, rin_cpp_exception_current_type(%rip)
    mov %rdx, %rdi
    mov $1, %esi
    jmp longjmp
3:
    mov $1, %edi
    mov $60, %eax
    syscall
    ud2

rin_cpp_exception_rethrow:
    mov rin_cpp_exception_current_value(%rip), %rdi
    mov rin_cpp_exception_current_type(%rip), %rsi
    jmp rin_cpp_exception_throw

rin_cpp_exception_throw_object:
    test %rsi, %rsi
    jz 5f
    mov rin_cpp_exception_object_used(%rip), %rax
    mov %rax, %r10
    add %rsi, %r10
    jc 5f
    cmp $4096, %r10
    ja 5f
    lea rin_cpp_exception_object_storage(%rip), %r8
    add %rax, %r8
    mov %rdi, %r9
    test %r9, %r9
    jz 5f
    mov %rsi, %rcx
    mov %r9, %rsi
    mov %r8, %rdi
    cld
    rep movsb
    mov %r10, rin_cpp_exception_object_used(%rip)
    mov %r8, %rdi
    mov %rdx, %rsi
    call rin_cpp_exception_throw
    ud2

rin_cpp_exception_rethrow_frame:
    mov 72(%rdi), %rax
    mov 80(%rdi), %rsi
    mov %rax, %rdi
    jmp rin_cpp_exception_throw

rin_cpp_exception_release_frame:
    test %rdi, %rdi
    jz 6f
    mov 80(%rdi), %edx
    test $0x80000000, %edx
    jz 6f
    movq $0, 72(%rdi)
    movq $0, 80(%rdi)
    movq $0, rin_cpp_exception_object_used(%rip)
6:
    ret

5:
    mov $134, %edi
    mov $60, %eax
    syscall
    ud2

rin_malloc:
    lea rin_exception_test_heap(%rip), %rax
    ret

rin_free:
    ret

_start:
    call _rcc_entry
    test %eax, %eax
    jne 4f
    xor %edi, %edi
    mov $60, %eax
    syscall
4:
    mov $1, %edi
    mov $60, %eax
    syscall
