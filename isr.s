[bits 32]
section .text

GLOBAL idt_flush
EXTERN isr_handler

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

%macro ISR_NOERR 1
GLOBAL isr%1
isr%1:
    cli
    push dword 0        ; фиктивный error code (CPU его не кладёт для этого вектора)
    push dword %1        ; номер исключения
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
GLOBAL isr%1
isr%1:
    cli
    push dword %1        ; error code уже положен CPU, добавляем только номер
    jmp isr_common_stub
%endmacro

ISR_NOERR 0    ; #DE  Divide by zero  (divnull её и ловит)
ISR_NOERR 1    ; #DB  Debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; #BP  Breakpoint
ISR_NOERR 4    ; #OF  Overflow
ISR_NOERR 5    ; #BR  Bound range
ISR_NOERR 6    ; #UD  Invalid opcode
ISR_NOERR 7    ; #NM  No FPU
ISR_ERR   8    ; #DF  Double fault
ISR_NOERR 9    ; Coproc overrun
ISR_ERR   10   ; #TS  Invalid TSS
ISR_ERR   11   ; #NP  Segment not present
ISR_ERR   12   ; #SS  Stack fault
ISR_ERR   13   ; #GP  General protection
ISR_ERR   14   ; #PF  Page fault
ISR_NOERR 15   ; Reserved
ISR_NOERR 16   ; #MF  FPU error
ISR_ERR   17   ; #AC  Alignment check
ISR_NOERR 18   ; #MC  Machine check
ISR_NOERR 19   ; #XM  SIMD error
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

isr_common_stub:
    pusha              ; edi,esi,ebp,esp,ebx,edx,ecx,eax
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10       ; data segment ядра
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp           ; передаём struct regs* в C
    call isr_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8         ; int_no + err_code
    iretd
