#include "lib/in-out.h"
#include "lib/keyboard.h"
#include "lib/video.h"
#include "lib/disk.h"
#include "lib/string.h"
#include "lib/fs.h"

// В начало файла, после #include
// В начало файла, после #include
extern void idt_flush(u32);
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

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
  u32 gs, fs, es, ds;
  u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // pusha
  u32 int_no, err_code;
  u32 ip, cs, flags;
} interrupt_frame;

typedef struct {
    int (*func_ptr)(char*);
    char* name;
} run_cmd;

void execute_command(command comm);

char tab_len = 4;
int read_select = 0;
u8 sector_buffer[512];

static void delay(unsigned int ms);
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
    wsf("sss", "                                     Kernel panic\n\n\nerr: ", err, "\npress any key to reboot");
    wait_keypress();
    outb(0x64, 0xFE);
    while(1);
}

void rtc_wait() {
    outb(0x70, 0x0A);
    u8 val = inb(0x71);
    while(val & 0x80) {
        outb(0x70, 0x0A);
        val = inb(0x71);
    }
}

u8 utc = 4; // UTC+4 Tbilisi

u8 rtc_read(u8 reg) {
    outb(0x70, 0x0B);
    u8 regB = inb(0x71);

    outb(0x70, reg);
    u8 val = inb(0x71);

    if (regB & 0x04) {
        return val;               // уже бинарный формат — возвращаем как есть
    } else {
        return (val & 0x0F) + ((val >> 4) * 10);  // BCD -> обычное число
    }
}

void run_script(u32 sector) {
    //readFilePart(sector, 0, sector_buffer);               // читаем сектор в sector_buffer
    static u8 script_buffer[4096];
    memset(script_buffer, 0, 4096);

    char rnum[128];
    int rcount = 0;// счетчик символов в rnum
    int i = 0;
    u16 strings = 0;
    for (; i < 4096; i++) {
        if(!(i % 512)) readFilePart(sector, i / 512, &script_buffer[i]);
        char c = script_buffer[i];

        // Пропускаем пустые байты
        if (!c) continue;
        if ((u8)c == 0xFF) continue;

        if (c == 10) {                       // символ конца команды
            strings++;
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
            if (rcount < (int)sizeof(rnum)-1) {
                rnum[rcount++] = c;  // добавляем символ в rnum
            }
        }
    }


    // если последняя строка не закончилась \n, всё равно вызываем execute
    if (rcount > 0) {
        strings++;
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
    char buf[5];
    wsf("ss", "\nExecuting finished in string ", int_to_str(strings, buf, sizeof(buf)));
}

void split_command(char* input, char** cmd, char** args) {
    *cmd = input;
    *args = 0;
    for(int j = 0; input[j]; j++) {
        if(input[j] != ' ') continue;
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
            current_value = lsb | (msb << 8);
        }
    }
}

struct idt_entry {
    u16 offset_lo;
    u16 selector;
    u8  zero;
    u8  flags;
    u16 offset_hi;
} __attribute__((packed));
 
struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));
 
static struct idt_entry idt[256];
struct idt_ptr idtp;
 
static void idt_set(u8 num, u32 handler) {
    idt[num].offset_lo = handler & 0xFFFF;
    idt[num].offset_hi  = (handler >> 16) & 0xFFFF;
    idt[num].selector   = 0x08;   // 32-bit code segment (см. boot.asm)
    idt[num].zero       = 0;
    idt[num].flags      = 0x8E;  // present, ring0, 32-bit interrupt gate
}
 
static const char* exc_name(int n) {
    switch(n) {
        case 0:  return "Divide by zero";
        case 1:  return "Debug";
        case 2:  return "NMI";
        case 3:  return "Breakpoint";
        case 4:  return "Overflow";
        case 5:  return "Bound range exceeded";
        case 6:  return "Invalid opcode";
        case 7:  return "No FPU";
        case 8:  return "Double fault";
        case 9:  return "Coprocessor overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment not present";
        case 12: return "Stack fault";
        case 13: return "General protection fault";
        case 14: return "Page fault";
        case 16: return "FPU error";
        case 17: return "Alignment check";
        case 18: return "Machine check";
        case 19: return "SIMD error";
        default: return "Unknown exception";
    }
}
 
void isr_handler(interrupt_frame *frame) {
    char buf[8];
    int_to_str((int)frame->int_no, buf, sizeof(buf));
 
    char bbuf[64] = {0};
    stradd(bbuf, "Exception_", buf);
 
    char full[96] = {0};
    stradd(full, bbuf, ": ");
    char full2[96] = {0};
    stradd(full2, full, exc_name((int)frame->int_no));
 
    Cpanic(full2);
}
 
static void idt_init(void) {
    for (int i = 0; i < 256; i++) idt_set((u8)i, 0);
 
    void *handlers[32] = {
        isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,
        isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31
    };
    for (int i = 0; i < 32; i++) idt_set((u8)i, (u32)handlers[i]);
 
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (u32)&idt;
    idt_flush((u32)&idtp);

    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
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
            //u16 device_id = (vendor >> 16) & 0xFFFF;

            //char buf[8]; // f*ck buffers
            wsf("sdsdsdsdsdsd", "\nBUS:", bus,
                                " SLOT:", slot, 
                                " FUNC:", func,
                                " CLASS:", class, 
                                " SUB:", subclass, 
                                " VEND:", vendor_id);
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

/*
void read_sector(u64 lba) {
    disk_read_lba((u32)lba, sector_buffer);
}
*/

void write_sector(u32 lba) {
    disk_write_lba(lba, sector_buffer);
}

void execute_command(command comm) {
    if (strcmp(comm.cmd, "color") == 0 && comm.args) {
        VGA_COLOR = (str_to_hex(comm.args[0]) << 4) | str_to_hex(comm.args[1]);
    }
    else if (strcmp(comm.cmd, "cls") == 0) {
        clear_screen();
    }
    else if (strcmp(comm.cmd, "echo") == 0 && comm.args) {
        put_char('\n');
        write_string(comm.args);
    }
    else if (strcmp(comm.cmd, "delay") == 0 && comm.args) {
        int del = str_to_int(comm.args);
        if(del <= 0 || del >= 100000) write_string("\ninvalid delay");
        else delay(del);
    }
    else if (strcmp(comm.cmd, "run") == 0) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        run_script((u32)read_select);
    }
    else if (strcmp(comm.cmd, "dumb") == 0) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        put_char('\n');
        move_screen();
        readFilePart(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i += 16) {  // выводим по 16 байт на строку
            char line[80];
            wsf("dc ", i, ':');

            // HEX часть
            for (int j = 0; j < 16; j++) {
                u8 b = sector_buffer[i + j];
                char hex[3] = {0};
                hex[0] = (b >> 4) < 10 ? '0' + (b >> 4) : 'A' + ((b >> 4) - 10);
                hex[1] = (b & 0x0F) < 10 ? '0' + (b & 0x0F) : 'A' + ((b & 0x0F) - 10);
                hex[2] = '\0';
                wsf("s ", hex);
            }

            // ASCII часть
            write_string(" |");
            for (int j = 0; j < 16; j++) {
                u8 c = sector_buffer[i + j];
                if (c > 10) {  // печатаемые символы
                    put_char(c);
                } else {
                    put_char_color('.', 0x70);  // непечатаемые
                }
            }
            char u = read_key(0);
            if (u == 27) break;
            write_string("|\n");
        }

    }
    else if (strcmp(comm.cmd, "info") == 0) {
        file f = getFile(read_select);
        wsf("ssxsssd", "\nmode: 32bit", "\ncolor: ", VGA_COLOR, "\nselect: ", f.path, "\ndisk size: ", data_sectors_total);
    }
    else if (strcmp(comm.cmd, "strwrite") == 0 && comm.args) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

        put_char('\n');
        get_string((char*)sector_buffer, 27, 512, 0b011);

        setFile(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

    }
    else if (strcmp(comm.cmd, "write") == 0 && comm.args) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

        for (int i = 0; i < length(comm.args); i++) 
            sector_buffer[i] = comm.args[i + 2];

        setFile(read_select, str_to_int(comm.args), sector_buffer);

        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;

    }
    else if (strcmp(comm.cmd, "read") == 0 && comm.args) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        for (int i = 0; i < 512; i++)
            sector_buffer[i] = 0;
        readFilePart(read_select, str_to_int(comm.args), sector_buffer);
        put_char('\n');
        for(int i = 0; i < 512; i++) 
            if(sector_buffer[i]) put_char(sector_buffer[i]);
        
    }
    else if (strcmp(comm.cmd, "select") == 0 && comm.args) {
        read_select = findFile(comm.args);
    }
    else if (strcmp(comm.cmd, "tab") == 0 && comm.args) {
        tab_len = str_to_int(comm.args);
    }
    else if (strcmp(comm.cmd, "charmap") == 0 && comm.args) {
        for(int i = 10; i < 256; i++)  wsf("ccdc", i, '|', i, newline);

    } 
    // divnull was here
    else if (strcmp(comm.cmd, "scan_pci") == 0) {
        scan_pci();
    }
    else if (strcmp(comm.cmd, "panic") == 0) {
        Cpanic("err_generated_manually");
    }
    else if (strcmp(comm.cmd, "reboot") == 0) {
        outb(0x64, 0xFE);
    }
    else if (strcmp(comm.cmd, "touch") == 0 && comm.args) {
        createFile(comm.args);
    }
    else if (strcmp(comm.cmd, "del") == 0) {
        if (read_select == 0) { write_string("\nno file selected"); return; }
        deleteFile(read_select);
    }
    else if (strcmp(comm.cmd, "dir") == 0) {
        put_char(newline);
        if(comm.args[0]) listFiles(write_string, comm.args);
        else listFiles(write_string, "/");
    }
    else if (strcmp(comm.cmd, "in") == 0 && comm.args) {
        char mode = comm.args[0];
        u16 port = str_to_int(&comm.args[2]);
        //u64 val = str_to_int(&comm.args[8]);
        char buf[5];
        if(mode == 'b') write_string(int_to_str(inb(port), buf, 5));
        if(mode == 'w') write_string(int_to_str(inw(port), buf, 5));
        if(mode == 'l') write_string(int_to_str(inl(port), buf, 5));
    }
    else if (strcmp(comm.cmd, "out") == 0 && comm.args) {
        char mode = comm.args[0];
        u16 port = str_to_int(&comm.args[2]);
        u32 val = str_to_int(&comm.args[8]);
        if(mode == 'b') outb(port, val);
        if(mode == 'w') outw(port, val);
        if(mode == 'l') outl(port, val);
    }
    else if (strcmp(comm.cmd, "time") == 0) {
        rtc_wait();
        u8 sec  = rtc_read(0x00);
        u8 min  = rtc_read(0x02);
        u8 hour = rtc_read(0x04);
        wsf("cdcdcd", newline, (utc + hour) % 24, ':', min, ':', sec);
    }
    else if (strcmp(comm.cmd, "date") == 0) {
        rtc_wait();
        u8 day = rtc_read(0x07);
        u8 mon = rtc_read(0x08);
        u16 year = rtc_read(0x09);
        wsf("cdcdcd", newline, day, '.', mon, space, 2000 + year);
    }
    else if (strcmp(comm.cmd, "utc") == 0 && comm.args) {
        utc = str_to_int(comm.args);
    }
    else {
        put_char(newline);
        write_string("incorrect command");
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
            char name[17];
            write_string("drive label:");
            format(get_string(name, NO_TERM, 16, 0b101));
            clear_screen();
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
    idt_init();
    __asm__ __volatile__(
        ".intel_syntax noprefix\n"
        "mov eax, cr0\n"
        "and eax, ~(1 << 2)\n"
        "and eax, ~(1 << 3)\n"
        "mov cr0, eax\n"
        "fninit\n"
        ".att_syntax prefix\n"
        ::: "eax"
    );
    kmain();
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}