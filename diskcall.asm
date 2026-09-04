; diskcall.asm — вызов трамплина из 32-bit C кода
[bits 32]
section .text

global bios_call
bios_call:
    pushad
    pushfd
    mov [0x5000], esp        ; сохранить стек PM

    mov dword [0x5004], reentry   ; куда вернуться (offset, известен линкеру)
    mov word  [0x5008], 0x08      ; в каком селекторе (32-bit code)

    jmp 0x18:0x2000           ; в трамплин (16-bit code selector, offset тела трамплина)

reentry:
    ; --- ВАЖНО: после real mode в DS/ES/SS лежит null-селектор ---
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [0x5000]
    popfd
    popad
    ret
