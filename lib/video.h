#ifndef VIDEO_H
#define VIDEO_H

#include "in-out.h"

enum {
  space = ' ',
  newline = '\n',
};

extern u16 cursor;
extern u8 automs;

extern u8 VGA_COLOR;

extern char num[256];
extern int count;

void move_cursor(void);
void del_symbol(void);

void set_char(u8 x, u8 y, char c);
void set_color(u8 x, u8 y, char c);
char get_char(u8 x, u8 y);
char get_color(u8 x, u8 y);

void copy_screen(char *buf);
void paste_screen(char *buf);
void clear_screen(void);
void move_screen(void);

void print_dec(u32 value);
void print_bin(u32 value);
void print_bit(u32 value, u8 bits);
void print_hex(u32 value);

void write_string(const char*);
void write_string_point(const char* s, int *x, int y);

void put_char_color(char c, int color);
void put_char(char c);

// s - строка, d - число, x - число в HEX, b - число в битах, c - символ
void wsf_point(int x, int y, const char *fmt, ...);

// s - строка, d - число, x - число в HEX, b - число в битах, c - символ
void wsf(const char *fmt, ...);

#endif