#ifndef IN_OUT_H
#define IN_OUT_H

typedef unsigned char       u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

extern u64 osc;

void outb(u16 port, u8 value);
u8 inb(u16 port);

void outw(u16 port, u16 val);
u16 inw(u16 port);

void outl(u16 port, u32 value);
u32 inl(u16 port);

u32 pci_read(u16 bus, u8 slot, u8 func, u8 offset);
void pci_write(u16 bus, u8 slot, u8 func, u8 offset, u32 value);
u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset);
void pci_write_dword(u8 bus, u8 slot, u8 func, u8 offset, u32 val);

u32 mmio_read32(u32 addr);
void mmio_write32(u32 addr, u32 val);

#endif