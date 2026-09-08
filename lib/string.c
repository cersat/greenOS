#include "string.h"

char *strcat(char *buffer, const char *str) {
    int i = 0;
    
    // Копируем первую строку
    while (*str != '\0') {
        buffer[i++] = *str++;
    }
    
    // Завершающий ноль
    buffer[i] = '\0';
    return buffer;
}

char *hex_to_str(u8 n, char* buf, int size) {
    if (size < 3) { // нужно как минимум 2 символа + '\0'
        if (size > 0) buf[0] = '\0';
        return buf;
    }

    const char hex_chars[] = "0123456789ABCDEF";

    buf[0] = hex_chars[(n >> 4) & 0xF]; // старший 4 бита
    buf[1] = hex_chars[n & 0xF];        // младший 4 бита
    buf[2] = '\0';
    return buf;
}

char *int_to_str(int n, char* buf, int size) {
    int is_negative = 0;

    if (n == 0) {
        if (size > 1) {
            buf[0] = '0';
            buf[1] = '\0';
        }
        return buf;
    }

    if (n < 0) {
        is_negative = 1;
        n = -n;
    }

    // временный буфер для цифр в обратном порядке
    char tmp[12]; // достаточно для 32-битного числа
    int j = 0;

    while (n > 0 && j < (int)sizeof(tmp)-1) {
        tmp[j++] = '0' + (n % 10);
        n /= 10;
    }

    if (is_negative) {
        tmp[j++] = '-';
    }

    // копируем цифры в buf в правильном порядке
    int k = 0;
    while (j > 0 && k < size-1) {
        buf[k++] = tmp[--j];
    }
    buf[k] = '\0';
    return buf;
}

int str_to_int(const char* str) {
    int result = 0;
    int i = 0;
    int sign = 1;

    // Проверка на знак
    if (str[0] == '-') {
        sign = -1;
        i++;
    }

    while (str[i] != '\0') {
        char c = str[i];

        if (c >= '0' && c <= '9') {
            result = result * 10 + (c - '0');
        } else {
            // встречен недопустимый символ — прекращаем
            break;
        }
        i++;
    }

    return result * sign;
}

u8 str_to_hex(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';      // '0' это 48, '1' это 49 и т.д.
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10; // 'a' становится 10
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10; // 'A' становится 10
    }
    return 0; // Если символ не hex-цифра
}

void *memset(void *dest, int val, unsigned int n) {
    unsigned char *ptr = (unsigned char*)dest;

    while(n--)
        *ptr++ = (unsigned char)val;

    return dest;
}

void *memcpy(void *dest, const void *src, unsigned int n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    while(n--) {
        *d++ = *s++;
    }

    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(*s1) - (int)(*s2);
}

// удаляет из str первое вхождение needle, сдвигая остаток строки влево
char *cut(char *str, const char *needle) {
	u32 nlen = strlen(needle);
	if (nlen == 0) return str;

	u32 pos = 0;
	u8 found = 0;

	for (u32 i = 0; str[i]; i++) {
		u32 j = 0;
		while (needle[j] && str[i + j] == needle[j]) j++;
		if (!needle[j]) {
			pos = i;
			found = 1;
			break;
		}
	}

	if (!found) return str;

	char *src = &str[pos + nlen];
	char *dst = &str[pos];
	while (*src) {
		*dst++ = *src++;
	}
	*dst = 0;
    return str;
}

u32 strlen(const char *s) {
	u32 len = 0;
	while (s[len]) len++;
	return len;
}

char *stradd(char *buffer, const char *str, const char *str2) {
	u32 i = 0;
	while (str[i]) {
		buffer[i] = str[i];
		i++;
	}
	u32 j = 0;
	while (str2[j]) {
		buffer[i + j] = str2[j];
		j++;
	}
	buffer[i + j] = 0;
    return buffer;
}