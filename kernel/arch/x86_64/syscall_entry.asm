[bits 64]
default rel

global syscall_entry
global arch_syscall_return
global g_user_temp_rsp
extern syscall_dispatcher
extern g_current_kernel_stack

section .bss
g_user_temp_rsp: resq 1

section .text

syscall_entry:
    ; RCX contains user RIP, R11 contains user RFLAGS
    ; Save user RSP temporarily
    mov [rel g_user_temp_rsp], rsp

    ; Switch to current thread's kernel stack
    mov rsp, [rel g_current_kernel_stack]

    ; Push user execution context onto the KERNEL stack
    push qword [rel g_user_temp_rsp] ; [rsp + 64]: User RSP
    push r11                         ; [rsp + 56]: User RFLAGS
    push rcx                         ; [rsp + 48]: User RIP
    push rbp                         ; [rsp + 40]
    push rbx                         ; [rsp + 32]
    push r12                         ; [rsp + 24]
    push r13                         ; [rsp + 16]
    push r14                         ; [rsp + 8]
    push r15                         ; [rsp + 0]

    ; Push 7th argument `a6` (passed in R9 by userland) onto the stack.
    ; 9 registers (72B) + 8B (a6) = 80B -> perfectly aligns RSP to 16 bytes before call!
    push r9

    ; Map userland syscall registers to System V AMD64 C ABI:
    ;   Userland: RAX=sys_no, RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5, R9=a6
    ;   Target C: RDI=sys_no, RSI=a1, RDX=a2, RCX=a3, R8=a4,  R9=a5, [rsp]=a6
    mov r9, r8          ; a5 in R9 (6th C arg)
    mov r8, r10         ; a4 in R8 (5th C arg)
    mov rcx, rdx        ; a3 in RCX (4th C arg)
    mov rdx, rsi        ; a2 in RDX (3rd C arg)
    mov rsi, rdi        ; a1 in RSI (2nd C arg)
    mov rdi, rax        ; sys_no in RDI (1st C arg)

    call syscall_dispatcher

    ; Clean up 7th argument from stack
    add rsp, 8

    ; Return value from dispatcher is in RAX

    ; Restore user state from kernel stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx             ; User RIP for sysret
    pop r11             ; User RFLAGS for sysret
    pop rsp             ; User RSP

    ; Return to Ring 3 (sysretq)
    o64 sysret

arch_syscall_return:
    ; Child thread return from fork():
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx             ; User RIP
    pop r11             ; User RFLAGS
    pop rsp             ; User RSP

    ; In child process, fork() returns 0 in RAX
    xor rax, rax
    o64 sysret
