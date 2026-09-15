.text
.globl _start
.globl setjmp
.globl longjmp
.globl rin_cpp_exception_install
.globl rin_cpp_exception_leave
.globl rin_cpp_exception_throw
.extern _rcc_entry

.bss
.align 8
rin_cpp_exception_top:
    .quad 0

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
    mov %rsi, 80(%rdx)
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
