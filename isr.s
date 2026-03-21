[bits 32]
GLOBAL isr13
GLOBAL isr14
GLOBAL isr_common_stub
EXTERN exception_handler_c

isr13:          ; #GPF
    cli
    push 0x0D        ; номер исключения
    jmp isr_common_stub

isr14:          ; #Page Fault  
    cli
    push 0x0E
    jmp isr_common_stub

isr_common_stub:
    pusha             ; все регистры
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10      ; data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp          ; передаём стек в C
    call exception_handler_c
    add esp, 4        ; чистим параметр
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8        ; error code
    iretd
