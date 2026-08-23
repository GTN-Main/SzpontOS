[bits 64]
default rel

global idt_load
global isr_stub_table
extern isr_handler

section .text

idt_load:
    lidt [rdi]
    ret

; ISR Common Stub
isr_common_stub:
    ; Save general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to interrupt_frame_t (current RSP) in RDI (1st argument in System V AMD64 ABI)
    mov rdi, rsp

    ; Call C handler
    call isr_handler

    ; Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Drop int_no and err_code from stack
    add rsp, 16

    ; Return from interrupt
    iretq

; Macro for ISR without CPU error code (pushes dummy 0)
%macro ISR_NOERRCODE 1
isr_stub_%1:
    push qword 0      ; Dummy error code
    push qword %1     ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISR with CPU error code
%macro ISR_ERRCODE 1
isr_stub_%1:
    push qword %1     ; Interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Generate 256 ISR entries
%assign i 0
%rep 256
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21 || i == 29 || i == 30
        ISR_ERRCODE i
    %else
        ISR_NOERRCODE i
    %endif
    %assign i i+1
%endrep

section .data
align 8
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
    %assign i i+1
%endrep
