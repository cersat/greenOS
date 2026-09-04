#include "lib/in-out.h"
#include "lib/keyboard.h"
#include "lib/video.h"
#include "lib/disk.h"
#include "lib/string.h"
#include "lib/fs.h"

// В начало файла, после #include
extern void isr13(void);
extern void isr14(void);
extern void isr_common_stub(void);

extern char bss_start_marker;
extern char bss_end_marker;

static void clear_bss(void)
{
    char *p = &bss_start_marker;
    while (p < &bss_end_marker)
        *p++ = 0;
}

void *memset(void *dest, int val, unsigned int n);
void execute(void);

typedef struct {
    char* cmd;
    char* args;
    int runner;
} command;

typedef struct {
    int (*func_ptr)(char*);
    char* name;
} run_cmd;

typedef struct {
  u32 ip;
  u32 cs;
  u32 flags;
} interrupt_frame;

void execute_command(command comm);

// Глобальная переменная для номера исключения
static int exception_number = 0;

char tab_len = 4;
int read_select = 50;
u8 sector_buffer[512];

void int_to_str(int n, char* buf, int size);
static void delay(unsigned int ms);
static void read_sector(u64 lba);
extern void split_command(char* input, char** cmd, char** args);

char hex_to_char(int c) {
    if (c >= 0 && c <= 9) return c + '0';      // '0' это 48, '1' это 49 и т.д.
    return c + 'a' - 10;
}

int length(const char* str) {
    unsigned int len = 0;

    while (str[len] != '\0') {
        len++;
    }

    return len;
}

void Cpanic(char *err) __attribute__((noreturn));
void Cpanic(char *err) {
    VGA_COLOR = 0x4F;
    clear_screen();
    write_string("                                        Kernel panic");
    put_char('\n');
    put_char('\n');
    put_char('\n');
    write_string("err:");
    write_string(err);
    put_char('\n');
    write_string("press any key to reboot");
    wait_keypress();
    outb(0x64, 0xFE);

}

void run_script(u32 sector) {
    read_sector((u64)sector);               // читаем сектор в sector_buffer

    char rnum[128];
    int rcount = 0;// счетчик символов в rnum
    int i = 0;              
    for (i; i < 512; i++) {
        char c = sector_buffer[i];

        // Пропускаем пустые байты
        if (!c) continue;
        if (c == 0xFF) continue;

        if (c == 10) {                       // символ конца команды
            rnum[rcount] = '\0';             // завершение строки
            char *cmd;
            char *args;

            split_command(rnum, &cmd, &args);
            command comm;
            comm.cmd = cmd;
            comm.args = args;
            comm.runner = 2;
            execute_command(comm);      // выполняем команду
            rcount = 0;                      // готовимся к следующей команде
        } else {
            if (rcount < sizeof(num)-1) {
                rnum[rcount++] = c;  // добавляем символ в rnum
            }
        }
    }


    // если последняя строка не закончилась \n, всё равно вызываем execute
    if (rcount > 0) {
        rnum[rcount] = '\0';
        char *cmd;
        char *args;
        split_command(rnum, &cmd, &args);
        command comm;
        comm.cmd = cmd;
        comm.args = args;
        comm.runner = 2;
        execute_command(comm);
        rcount = 0;
    }
    write_string("Executing finished in string ");
    char *buf;
    int_to_str(i,buf,sizeof(buf));
    write_string(buf);
}

void split_command(char* input, char** cmd, char** args) {
    *cmd = input;
    *args = 0;
    u8 space = 0;
    int j = 0;
    for(j; input[j]; j++) {
        if(input[j] != ' ') continue;
        space = 1;
    }
    if(!space) {
        cmd = &input;
        *args = 0;
        return; // Команда без аргумента
    }


    for (int i = 0; input[i]; i++) {
        if (input[i] == ' ') {
            input[i] = '\0';     // разрезаем строку
            *args = &input[i+1]; // аргументы после пробела
            return;
        }
    }
}

static inline void delay(unsigned int ms) {
    if (ms == 0) return;
    u32 total_ticks = (1193182 / 1000) * ms;
    while (total_ticks > 0) {
        u16 count = (total_ticks > 65535) ? 65535 : (u16)total_ticks;
        total_ticks -= count;
        outb(0x43, 0x30);                 
        outb(0x40, count & 0xFF);
        outb(0x40, (count >> 8) & 0xFF);
        u16 current_value = count;
        while (current_value > 0) {
            outb(0x43, 0x00); 

            u8 lsb = inb(0x40);
            u8 msb = inb(0x40);
            u16 new_value = lsb | (msb << 8);

            if (new_value > current_value) {
                break; 
            }
            current_value = new_value;
        }
    }
}

void __attribute__((interrupt)) exception_handler_c(struct interrupt_frame *frame) {
    char buf[8];
    int_to_str(exception_number, buf, sizeof(buf));
    
    char bbuf[32] = {0};
    strcat(bbuf, "Exception_", buf);
    
    Cpanic(bbuf);
}

void scan_pci(void) {
    for(int bus=0; bus<256; bus++) 
    for(int slot=0; slot<32; slot++) 
    for(int func=0; func<8; func++) {

        u32 vendor = pci_read(bus,slot,func,0);

        if((vendor & 0xFFFF) != 0xFFFF) {

            u32 classreg = pci_read(bus,slot,func,0x08);

            u8 class = classreg >> 24;
            u8 subclass = (classreg >> 16) & 0xFF;

            u16 vendor_id = vendor & 0xFFFF;
            u16 device_id = (vendor >> 16) & 0xFFFF;

            char buf[8];

            write_string("\nBUS:");
            int_to_str(bus,buf,sizeof(buf));
            write_string(buf);

            write_string(" SLOT:");
            int_to_str(slot,buf,sizeof(buf));
            write_string(buf);

            write_string(" FUNC:");
            int_to_str(func,buf,sizeof(buf));
            write_string(buf);

            write_string(" CLASS:");
            int_to_str(class,buf,sizeof(buf));
            write_string(buf);

            write_string(" SUB:");
            int_to_str(subclass,buf,sizeof(buf));
            write_string(buf);

            write_string(" VEND:");
            int_to_str(vendor_id,buf,sizeof(buf));
            write_string(buf);
            wait_keypress();
        }
    }
}

void execute(void) {
    num[count - 1] = '\0';

    char* cmd;
    char* args;

    split_command(num, &cmd, &args);
    command comm;
    comm.cmd = cmd;
    comm.args = args;
    comm.runner = 1;
    execute_command(comm);
}

void read_sector(u64 lba) {
    disk_read_lba((u32)lba, sector_buffer);
}

void write_sector(u32 lba) {
    disk_write_lba(lba, sector_buffer);
}

void execute_command(command comm) {
    if (strcmp(comm.cmd, "color") == 0) {
        VGA_COLOR = (str_to_hex(comm.args[0]) << 4) | str_to_hex(comm.args[1]);
    }
    if (strcmp(comm.cmd, "cls") == 0) {
        clear_screen();
    }
    if (strcmp(comm.cmd, "echo") == 0) {
        if (comm.args) {
            put_char('\n');
            write_string(comm.args);
        }
    }
    if (strcmp(comm.cmd, "delay") == 0) {
        delay(str_to_int(comm.args) * 30000);
    }
    if (strcmp(comm.cmd, "run") == 0) {
        run_script((u32)read_select);
    }
    if (strcmp(comm.cmd, "dumb") == 0) {
        put_char('\n');
        move_screen();
        readFilePart(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i += 16) {  // выводим по 16 байт на строку
            char line[80];
            int pos = 0;

            // Выводим адрес в начале строки
            int_to_str(i, line, sizeof(line));
            write_string(line);
            write_string(": ");

            // HEX часть
            for (int j = 0; j < 16; j++) {
                u8 b = sector_buffer[i + j];
                char hex[3] = {0};
                hex[0] = (b >> 4) < 10 ? '0' + (b >> 4) : 'A' + ((b >> 4) - 10);
                hex[1] = (b & 0x0F) < 10 ? '0' + (b & 0x0F) : 'A' + ((b & 0x0F) - 10);
                hex[2] = '\0';
                write_string(hex);
                put_char(' ');
                
            }

            // ASCII часть
            write_string(" |");
            for (int j = 0; j < 16; j++) {
                char c = sector_buffer[i + j];
                if (c != 10) {  // печатаемые символы
                    put_char(c);
                } else {
                    put_char('-');  // непечатаемые
                }
            }
            char u = read_key(0);
            if (u == 27) break;
            write_string("|\n");
        }

    }
    if (strcmp(comm.cmd, "info") == 0) {
        write_string("\nmode: 32bit");
        
        write_string("\ncolor: ");
        char buf[4]; // временный массив для числа
        int_to_str(VGA_COLOR, buf, sizeof(buf));
        write_string(buf);
        
        write_string("\nselect: ");
        write_string(getFile(read_select).path);
    }
    if (strcmp(comm.cmd, "strwrite") == 0) {
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

        put_char('\n');
        int i = 0;
        for (char c; c != 27; c = read_key(1)) {
            sector_buffer[i] = c;
            put_char(c);
            i++;
        }

        setFile(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

    }
    if (strcmp(comm.cmd, "write") == 0) {
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

        for (int i = 0; i < length(comm.args); i++) 
            sector_buffer[i] = comm.args[i + 1];

        setFile(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

    }
    if (strcmp(comm.cmd, "read") == 0) {
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;
        readFilePart(read_select, str_to_int(comm.args), sector_buffer);
        put_char('\n');
        for(int i = 0; i < 512; i++) 
            if(sector_buffer[i]) put_char(sector_buffer[i]);
        
    }
    if (strcmp(comm.cmd, "select") == 0) {
        read_select = findFile(comm.args);
    }
    if (strcmp(comm.cmd, "tab") == 0) {
        tab_len = str_to_int(comm.args);
    }
    if (strcmp(comm.cmd, "charmap") == 0) {
        if(comm.args[1]) {put_char(comm.args[1]); return;}
        for(int i = 10; i < 256; i++) {
            put_char(i);
            put_char('|');
        }

    }
    if (strcmp(comm.cmd, "divnull") == 0) {
        char c = 1/0;
        put_char(c);
    }
    if (strcmp(comm.cmd, "scan_pci") == 0) {
        scan_pci();
    }
    if (strcmp(comm.cmd, "panic") == 0) {
        Cpanic("err_generated_manually");
    }
    if (strcmp(comm.cmd, "reboot") == 0) {
        outb(0x64, 0xFE);
    }
    if (strcmp(comm.cmd, "touch") == 0) {
        createFile(comm.args);
    }
    if (strcmp(comm.cmd, "dir") == 0) {
        if(comm.args[0]) listFiles(write_string, comm.args);
        else listFiles_(write_string);
    }
    count = 0;
    for(int i = 0; i < 15; i++) num[i] = 0;
}

void kmain(void) {
    VGA_COLOR = 0x0F;
    clear_screen();
    disk_init();
    init_fs();
    if(!inited) {
        write_string("MAT not formatted, format? (y/n)");
        char choi = choice("ny");
        if(choi == 'y' || choi == 'Y') {
            char name[10];
            for(int i = 0; i < 10; i++) name[i] = read_key(0);
            format(name);
        }
    }

    write_string("greenOS bootloader\npress any key to boot");
    wait_keypress();
    clear_screen();
    VGA_COLOR = 0x02;

    write_string("greenOS\n>");
    automs = 1;
    for (;;) {
        char c = read_key(1);
        if (c == '\b') {
            if (count > 0) {
                count--;
                num[count] = 0;
                del_symbol();
            }
            continue;
        }
        num[count] = c;
        count++;
        if (c == 9) {
            count--;
            for (int i = 0; i < tab_len; i++) {
                num[count] = ' ';
                count++;
                put_char(' ');
            }
            continue;
        }
        if (c == 10) execute();
        if (c != 8 && c != 9) put_char(c);
        if (c == 10) put_char('>');
    }
}

__attribute__((section(".text.start")))
void _start(void) {
    clear_bss();
    kmain();
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}