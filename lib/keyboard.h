#ifndef KEYBOARD_H
#define KEYBOARD_H

char read_key(void);
static char shift_scancode_to_ascii(u8 scancode);
static char       scancode_to_ascii(u8 scancode);

#endif