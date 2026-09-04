#ifndef STRING_H
#define STRING_H
#include "in-out.h"

// str1, str2 - исходные строки
// buffer - пустой массив с достаточным размером
void strcat(char *buffer, const char *str1, const char *str2);

// n - число, buf - массив для результата, size - размер буфера
// Функция пишет строку вида "0A", "FF" и т.д.
void hex_to_str(u8 n, char* buf, int size);

// n - число, buf - массив символов, size - размер массива
void int_to_str(int n, char* buf, int size);

// Преобразует строку вида "1234" в число 1234
int str_to_int(const char* str);

// Преобразует символ 0123456789abcdef в число
u8 str_to_hex(char c);

void *memset(void *dest, int val, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
int strcmp(const char* s1, const char* s2);
void cut(char *str, const char *needle);
u32 strlen(const char *s);
void stradd(char *buffer, const char *str, const char *str2);

#endif