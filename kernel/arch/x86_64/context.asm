[bits 64]
default rel

global arch_switch_context
global arch_enter_user_mode
global arch_thread_trampoline
extern thread_exit

section .text

; void arch_switch_context(uintptr_t *old_rsp_ptr, uintptr_t new_rsp)
; RDI: old_rsp_ptr
; RSI: new_rsp
arch_switch_context:
    ; Save callee-saved registers of old thread
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq

    ; Save current RSP into *old_rsp_ptr
    mov [rdi], rsp

    ; Load new RSP
    mov rsp, rsi

    ; Restore callee-saved registers of new thread
    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret

; void arch_thread_trampoline(void (*entry_point)(void))
; RDI: entry_point (on initial thread stack)
arch_thread_trampoline:
    pop rdi          ; Pop function pointer
    call rdi         ; Call thread function

    ; If thread function returns, exit cleanly
    mov rdi, 0
    call thread_exit
.hang:
    hlt
    jmp .hang

; void arch_enter_user_mode(uintptr_t rip, uintptr_t rsp)
; RDI: User RIP
; RSI: User RSP
arch_enter_user_mode:
    cli

    ; Setup Segment Selectors for Ring 3
    ; 0x18 = User Data Selector (DPL = 3) -> 0x18 | 3 = 0x1B
    ; 0x20 = User Code Selector (DPL = 3) -> 0x20 | 3 = 0x23
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push iretq frame
    push qword 0x1B       ; SS (User Data)
    push rsi              ; RSP (User Stack)
    push qword 0x202      ; RFLAGS (Interrupts enabled, bit 1 always 1)
    push qword 0x23       ; CS (User Code)
    push rdi              ; RIP (User Entry Point)

    ; Clear general purpose registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    iretq
