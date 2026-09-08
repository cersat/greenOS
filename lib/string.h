#ifndef STRING_H
#define STRING_H
#include "in-out.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

// str - исходные строки
// buffer - пустой массив с достаточным размером
char *strcat(char *buffer, const char *str1);

// n - число, buf - массив для результата, size - размер буфера
// Функция пишет строку вида "0A", "FF" и т.д.
char *hex_to_str(u8 n, char* buf, int size);

// n - число, buf - массив символов, size - размер массива
char *int_to_str(int n, char* buf, int size);

// Преобразует строку вида "1234" в число 1234
int str_to_int(const char* str);

// Преобразует символ 0123456789abcdef в число
u8 str_to_hex(char c);

void *memset(void *dest, int val, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
int strcmp(const char* s1, const char* s2);
char *cut(char *str, const char *needle);
u32 strlen(const char *s);
char *stradd(char *buffer, const char *str, const char *str2);

#endif