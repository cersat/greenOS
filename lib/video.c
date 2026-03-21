#include "in-out.h"
#include "video.h"

static volatile u8* const VGA = (volatile u8*)0xB8000;

u16 cursor = 0;
int VGA_COLOR = 0x2;

char num[256];
int count = 0;

void move_cursor(void) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(cursor & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((cursor >> 8) & 0xFF));
}

void del_symbol(void) {
    if (cursor > 0) {
        cursor--;
        VGA[cursor * 2] = ' ';
        VGA[cursor * 2 + 1] = VGA_COLOR;
        move_cursor();
    }
}

void clear_screen(void) {
    for (u16 i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA[i * 2] = ' ';
        VGA[i * 2 + 1] = VGA_COLOR;
    }
    cursor = 0;
    move_cursor();
}

void put_char(char c) {
    if (c == '\n') {
        cursor += (VGA_WIDTH - (cursor % VGA_WIDTH));
    } else {
        VGA[cursor * 2] = (u8)c;
        VGA[cursor * 2 + 1] = VGA_COLOR;
        ++cursor;
    }

    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        cursor = 0;
    }

    move_cursor();
}

void print_dec(u32 value)
{
    char hex_chars[] = "0123456789";
    int i = 2;
    if(value >= 10) i = 3;
    if(value >= 100) i = 4;
    if(value >= 1000) i = 5;
    if(value >= 10000) i = 6;
    char buffer[i];

    buffer[i - 1] = 0;

    for(int j = i - 2; j >= 0; j--) {
        buffer[j] = hex_chars[value & 0x09];
        value >>= 4;
    }

    write_string(buffer);
}

void print_hex(u32 value)
{
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];

    buffer[8] = 0;

    for(int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    write_string(buffer);
}

void print_bin(u32 value) {
    for(int i = 31; i >= 0; i--) {
        u32 bit = (value >> i) & 1;
        put_char(bit + '0');
    }
}

void print_bin16(u16 value) {
    for(int i = 15; i >= 0; i--) {
        u32 bit = (value >> i) & 1;
        put_char(bit + '0');
    }
}

void print_bin8(u8 value) {
    for(int i = 7; i >= 0; i--) {
        u32 bit = (value >> i) & 1;
        put_char(bit + '0');
    }
}

void write_string(const char* s) {
    while (*s) {
        put_char(*s++);
    }
}