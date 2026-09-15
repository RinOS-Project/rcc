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

rin_cpp_exception_install:
    mov rin_cpp_exception_top(%rip), %rax
    mov %rax, 64(%rdi)
    mov %rdi, rin_cpp_exception_top(%rip)
    ret

rin_cpp_exception_leave:
    mov rin_cpp_exception_top(%rip), %rax
    cmp %rdi, %rax
    jne 2f
    mov 64(%rdi), %rax
    mov %rax, rin_cpp_exception_top(%rip)
2:
    ret

rin_cpp_exception_throw:
    mov rin_cpp_exception_top(%rip), %rdx
    test %rdx, %rdx
    jz 3f
    mov %rdi, 72(%rdx)
    mov %rdi, rin_cpp_exception_current_value(%rip)
    mov %rsi, 80(%rdx)
    mov %rsi, rin_cpp_exception_current_type(%rip)
    mov 64(%rdx), %rax
    mov %rax, rin_cpp_exception_top(%rip)
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
