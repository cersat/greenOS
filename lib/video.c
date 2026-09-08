#include "in-out.h"
#include "video.h"
#include "string.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

u8 automs = 0;
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

static inline void ser_putc(char c){
    while(!(inb(0x3F9-1+5) & 0x20));
    outb(0x3F8, (u8)c);
}
static u8 ser_ready = 0;

void set_char(u8 x, u8 y, char c) {
    u16 pos = y * VGA_WIDTH + x;
    VGA[pos * 2] = (u8)c;
    VGA[pos * 2 + 1] = VGA_COLOR;
}

void set_color(u8 x, u8 y, char c) {
    u16 pos = y * VGA_WIDTH + x;
    VGA[pos * 2 + 1] = (u8)c;
}

char get_char(u8 x, u8 y) {
    u16 pos = y * VGA_WIDTH + x;
    return (char)VGA[pos * 2];
}

char get_color(u8 x, u8 y) {
    u16 pos = y * VGA_WIDTH + x;
    return (char)VGA[(pos * 2) + 1];
}

void move_screen() {
    // 1. Сдвигаем все строки (начиная со второй, j = 1) на одну строку вверх
    for (u8 j = 1; j < VGA_HEIGHT; j++) {
        for (u8 i = 0; i < VGA_WIDTH; i++) {
            // Копируем символ из текущей строки j в строку выше (j - 1)
            int color = get_color(i, j);
            set_char(i, j - 1, get_char(i, j));
            set_color(i, j - 1, color);
        }
    }

    // 2. Очищаем самую нижнюю строку, чтобы она не дублировалась
    for (u8 i = 0; i < VGA_WIDTH; i++) {
        set_char(i, VGA_HEIGHT - 1, ' '); // Записываем пробел (или 0)
    }
}

void put_char_color(char c, int color) {
    int col = VGA_COLOR;
    VGA_COLOR = color;
    put_char(c);
    VGA_COLOR = col;
}

void put_char(char c) {
    if(!ser_ready){
        outb(0x3F8+1,0x00); outb(0x3F8+3,0x80); outb(0x3F8+0,0x03);
        outb(0x3F8+1,0x00); outb(0x3F8+3,0x03); outb(0x3F8+2,0xC7); outb(0x3F8+4,0x0B);
        ser_ready = 1;
    }
    ser_putc(c);

    if (c == '\n') {
        cursor += (VGA_WIDTH - (cursor % VGA_WIDTH));
    } else {
        VGA[cursor * 2] = (u8)c;
        VGA[cursor * 2 + 1] = VGA_COLOR;
        ++cursor;
    }

    // единая точка проверки переполнения - после ЛЮБОГО символа, не только '\n'
    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        if (automs) {
            move_screen();
            cursor -= VGA_WIDTH;
        } else {
            cursor = 0;
        }
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
        buffer[j] = hex_chars[value % 10];
        value /= 10;
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

void print_bit(u32 value, u8 bits) {
    for(int i = bits - 1; i >= 0; i--) {
        u32 bit = (value >> i) & 1;
        put_char(bit + '0');
    }
}

void write_string(const char* s) {
    while (*s) {
        put_char(*s++);
    }
}

void wsf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        switch (*fmt) {
            case 's': {
                char *s = va_arg(args, char*);
                write_string(s);
                break;
            }
            case 'd': {
                int v = va_arg(args, int);
                char buf[12];
                write_string(int_to_str(v, buf, sizeof(buf)));
                break;
            }
            case 'x': {
                u32 v = va_arg(args, u32);
                char buf[9];
                write_string(hex_to_str(v, buf, sizeof(buf)));
                break;
            }
            case 'b': {
                u32 v = va_arg(args, u32);
                print_bin(v);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                put_char(c);
                break;
            }
            case ' ': {
                put_char(' ');
                break;
            }
            case '\n': {
                put_char('\n');
                break;
            }
            default: {
                break;
            }
        }
        fmt++;
    }

    va_end(args);
}