#ifndef VIDEO_H
#define VIDEO_H

#include "in-out.h"

enum {
  space = ' ',
  newline = '\n',
};

extern u16 cursor;

extern int VGA_COLOR;

extern char num[256];
extern int count;

void move_cursor(void);
void del_symbol(void);
void clear_screen(void);
void print_dec(u32 value);
void print_bin(u32 value);
void print_bit(u32 value, u8 bits);
void print_hex(u32 value);
void write_string(const char*);
void put_char(char c);

#endif