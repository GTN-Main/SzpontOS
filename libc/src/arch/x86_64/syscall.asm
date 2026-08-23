[bits 64]
default rel

global __syscall
global __syscall0
global __syscall1
global __syscall2
global __syscall3
global __syscall4
global __syscall5
global __syscall6
global __clone

section .text

; int64_t __syscall(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6)
__syscall:
    mov rax, rdi        ; syscall number
    mov rdi, rsi        ; a1
    mov rsi, rdx        ; a2
    mov rdx, rcx        ; a3
    mov r10, r8         ; a4 (r10 used instead of rcx for syscall)
    mov r8,  r9         ; a5
    mov r9,  [rsp + 8]  ; a6 (from stack)

    syscall
    ret

__syscall0:
    mov rax, rdi
    syscall
    ret

__syscall1:
    mov rax, rdi
    mov rdi, rsi
    syscall
    ret

__syscall2:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    syscall
    ret

__syscall3:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    syscall
    ret

__syscall4:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    syscall
    ret

__syscall5:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8,  r9
    syscall
    ret

__syscall6:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8,  r9
    mov r9,  [rsp + 8]
    syscall
    ret

; int clone(int (*fn)(void *), void *child_stack, int flags, void *arg, int *ptid, void *tls, int *ctid)
; RDI: fn, RSI: child_stack, RDX: flags, RCX: arg, R8: ptid, R9: tls, [rsp+8]: ctid
__clone:
    test rdi, rdi
    jz .clone_err
    test rsi, rsi
    jz .clone_err

    ; Push fn and arg onto child stack
    sub rsi, 16
    mov [rsi], rdi       ; fn
    mov [rsi + 8], rcx   ; arg

    ; Syscall arguments:
    ;   RAX = SYS_clone (56)
    ;   RDI = flags (RDX)
    ;   RSI = child_stack (RSI)
    ;   RDX = ptid (R8)
    ;   R10 = ctid ([rsp + 8])
    ;   R8  = tls (R9)
    mov rdi, rdx         ; flags
    mov rdx, r8          ; ptid
    mov r10, [rsp + 8]   ; ctid
    mov r8, r9           ; tls
    mov eax, 56          ; SYS_clone
    syscall

    test rax, rax
    jl .clone_err
    jz .child_entry
    ret

.child_entry:
    ; In child: pop fn and arg
    pop rdi              ; fn
    pop rsi              ; arg
    mov rax, rdi
    mov rdi, rsi         ; arg in RDI (1st C arg)
    call rax             ; fn(arg)

    ; If fn returns, exit thread
    mov rdi, rax
    mov eax, 60          ; SYS_exit
    syscall
    hlt

.clone_err:
    neg eax
    ret
