#ifndef COMMAND_H
#define COMMAND_H

#include "in-out.h"
#define RET_ERRCODE(code, str) \
    do { \
        ret.errorcode = (code); \
        strcat(ret.error, str); \
        return ret; \
    } while(0)

typedef struct {
    int errorcode;
    char error[64];
} cmd_ret;

typedef struct {
    char args[64][64];
    u8 amount;
} cmd_args;

typedef struct {
    cmd_ret (*func_ptr)(cmd_args);
    char name[32];
} run_cmd;

cmd_args get_args(char *str_args);

#endif
