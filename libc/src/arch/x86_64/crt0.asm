[bits 64]
default rel

global _start
extern main
extern exit
extern environ

section .text

_start:
    ; Terminate stack frame for debuggers
    xor rbp, rbp

    ; Extract argc, argv, envp from stack
    ; [rsp] = argc
    ; [rsp + 8] = argv
    mov rdi, [rsp]        ; 1st arg: argc
    lea rsi, [rsp + 8]    ; 2nd arg: argv

    ; envp is after argv array (argc + 1 pointers)
    mov rax, rdi
    inc rax
    shl rax, 3
    lea rdx, [rsi + rax]  ; 3rd arg: envp

    ; Set environ = envp
    mov [environ], rdx

    ; Align stack to 16 bytes before calling main (System V ABI requirement)
    and rsp, -16

    call main wrt ..plt

    ; Exit with main's return code
    mov rdi, rax
    call exit wrt ..plt

.hang:
    hlt
    jmp .hang
