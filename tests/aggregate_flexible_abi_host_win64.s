.intel_syntax noprefix
.text
.globl main
main:
    push rbp
    mov rbp, rsp
    push rdi
    sub rsp, 24

    mov DWORD PTR [rsp], 73
    call flexible_prefix_value
    cmp eax, 73
    jne .Lfailed

    lea rdi, [rbp - 16]
    mov DWORD PTR [rsp], 73
    call flexible_prefix_round_trip
    cmp DWORD PTR [rbp - 16], 73
    jne .Lfailed

    xor eax, eax
    jmp .Lreturn
.Lfailed:
    mov eax, 1
.Lreturn:
    add rsp, 24
    pop rdi
    pop rbp
    ret
