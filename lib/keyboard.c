#include "in-out.h"
#include "keyboard.h"

static u8 shift = 0;
static u8 arrows = 0;

static char shift_scancode_to_ascii(u8 scancode) {
    static const char table[128] = {
        ']', 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        '\a', 131, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        129, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 133, 132,
        130, ' '
    };
    // 27 escape
    // 131 alt
    // 130 ctrl
    // 129 shift
    // 132 print screen
    // 133 pause

    return table[scancode];
}

static char scancode_to_ascii(u8 scancode) {
    static const char table[128] = {
        ']', 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        '\n', 131, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        129, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 133, 132,
        130, ' '
    };
    // 27 escape
    // 131 alt
    // 130 ctrl
    // 129 shift
    // 132 print screen
    // 133 pause

    return table[scancode];
}
void wait_keypress(void) {
    for(;;) {
        if((inb(0x64) & 1) == 0) continue;
        break;
    }
}

char choice(char *choices) {
    u8 length = 0;
    for(length; choices[length]; length++);
    for(;;) {
        char c = read_key(0);
        for(int i = 0; i < length; i++) if(choices[i] == c) return choices[i];
    }
}

char read_key(u8 functions) {
    for (;;) {
        if ((inb(0x64) & 1) == 0) {
            continue;
        }
        
        u8 scancode = inb(0x60);

        // Shift
        if (scancode == 0x2A || scancode == 0x36) {
            shift = 1;   // Shift нажата
            continue;
        }
        if (scancode == 0xAA || scancode == 0xB6) {
            shift = 0;   // Shift отпущен
            continue;
        }

        // arrows
        if(scancode == 0xE0) {
            arrows = 1;
            continue;
        }
        if(arrows) {
            arrows = 0;
            if(scancode == 0x48) return 24;
            if(scancode == 0x50) return 25;
            if(scancode == 0x4D) return 26;
            if(scancode == 0x4B) return 27;
            continue;
        }

        if(scancode & 0x80) continue;

        // F1
        if (scancode == 0x3B && functions) {
            int number = 0;
            char key;

            for (;;) {
                key = read_key(0);

                if (key >= '0' && key <= '9') {
                    number = number * 10 + (key - '0');
                }

                else if (key == 10) {  // Enter
                    return number;
                    break;
                }
            }
        }

        // F2
        if (scancode == 0x3C && functions) {
            
            continue;
        }

        // F5
        if (scancode == 0x3F && functions) {
            outb(0x64, 0xFE);
            continue;
        }

        char c;
        if (shift)
            c = shift_scancode_to_ascii(scancode);
        else
            c =       scancode_to_ascii(scancode);
        
        if (c) {
            return c;
        }
    }
}