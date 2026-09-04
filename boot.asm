bits 16
org 0x7C00

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 50
%endif

KERNEL_LOAD_SEG equ 0x1000
KERNEL_ENTRY    equ 0x10000

jmp short real_start
nop
times 33 db 0

real_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    
    mov [boot_drive], dl
    mov [0x0500], dl           ; boot drive для kernel (INT13h трамплин)
    mov si, loading_msg
    call print

    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, loaded_msg
    call print

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEL:protected_mode

print:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp print
.done: ret

disk_error:
    mov si, error_msg
    call print
.hang: cli
    hlt
    jmp .hang

boot_drive db 0
loading_msg db 'Loading C kernel...', 0
loaded_msg db 'Kernel loaded!', 0
error_msg   db 'Disk read error', 0

align 8
gdt_start:
    dq 0x0000000000000000                        ; 0x00 null
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00           ; 0x08 32-bit flat code
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00           ; 0x10 32-bit flat data
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 00000000b, 0x00           ; 0x18 16-bit code, limit 64K
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00           ; 0x20 16-bit data, limit 64K
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEL equ 0x08
DATA_SEL equ 0x10
CODE16_SEL equ 0x18
DATA16_SEL equ 0x20

[bits 32]
protected_mode:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp KERNEL_ENTRY

times 510 - ($ - $$) db 0
dw 0xAA55