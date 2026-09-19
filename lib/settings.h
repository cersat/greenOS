#ifndef SETTINGS_H
#define SETTINGS_H
#include "in-out.h"

s16 settings_read(u16 set);
s8 settings_write(u16 set, u8 val);

void settings_create();

#endif