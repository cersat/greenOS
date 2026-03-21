#ifndef STRING_H
#define STRING_H

typedef struct {
    char* str;
} string;

int string_length(string s);
#endif