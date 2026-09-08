[bits 32]
section .text

extern idtp        ; символ из kernel.c

global bios_call
bios_call:
    pushad
    pushfd
    mov [0x5000], esp

    mov dword [0x5004], reentry
    mov word  [0x5008], 0x08

    jmp 0x18:0x2000

reentry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [0x5000]
    lidt [idtp]        ; восстановить protected-mode IDT после BIOS-вызова
    popfd
    popad
    ret