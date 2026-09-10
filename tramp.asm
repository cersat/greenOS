; tramp.asm — real-mode трамплин для BIOS INT 13h (greenOS)
; BIOS (SeaBIOS) внутри int 13h делает свой lgdt и не
; восстанавливает наш — поэтому перед возвратом в protected mode мы
; перезагружаем СВОЮ копию GDT, которая едет прямо здесь (org 0x2000 известен).
[bits 16]
[org 0x2000]

tramp:
    ; --- сюда прыгаем из 32-bit PM, CS=0x18 (16-bit code) ---
    mov ax, 0x20            ; DATA16_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7000          ; стек внутри 64K лимита 16-битного сегмента

    mov eax, cr0
    and al, 0xFE             ; PE=0 -> real mode
    mov cr0, eax
    jmp 0x0000:real

real:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7000

    lidt [rm_idtr]           ; вернуть реальную IVT (BIOS её ждёт)
    sti

    mov ah, [0x500A]          ; команда: 0x42 read / 0x43 write
    xor al, al
    mov dl, [0x500B]          ; boot drive
    mov si, 0x5010            ; DAP
    int 0x13

    mov [0x500C], ah          ; результат BIOS в AH
    setc al
    mov [0x500D], al          ; флаг carry (ошибка)
    cli

    ; --- восстановить НАШУ GDT (BIOS её затоптала) ---
    xor ax, ax
    mov ds, ax
    lgdt [my_gdt_desc]

    mov eax, cr0
    or  al, 1                 ; PE=1
    mov cr0, eax

    ; jmp far dword [0x5004] -> селектор:офсет заполнены bios_call() перед вызовом
    db 0x66, 0xFF, 0x2E
    dw 0x5004

align 8
my_gdt:
    dq 0x0000000000000000
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00   ; 0x08 32-bit flat code
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00   ; 0x10 32-bit flat data
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 00000000b, 0x00   ; 0x18 16-bit code
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00   ; 0x20 16-bit data
my_gdt_end:

my_gdt_desc:
    dw my_gdt_end - my_gdt - 1
    dd my_gdt                 ; org 0x2000 -> абсолютный линейный адрес верный

align 4
rm_idtr:
    dw 0x03FF
    dd 0x00000000
