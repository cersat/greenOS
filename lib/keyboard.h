#ifndef KEYBOARD_H
#define KEYBOARD_H

// Ждать нажатия клавиши
void wait_keypress(void);
// Ждать нажатия клавиши из массива вида "wasdWASD", причём не допускается использование массива без нулевого терминатора
char choice(char *choices);
// Читать клавишу с клавиатуры. Требует параметр, отвечающий за включение или не включение функциональных клавиш
char read_key(u8 functions);

#endif