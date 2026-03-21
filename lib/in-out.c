#include "in-out.h"

inline void outb(u16 port, u8 value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline u8 inb(u16 port) {
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void outl(u16 port, u32 value) {
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

inline u32 inl(u16 port) {
    u32 value;
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline u32 pci_read(u16 bus, u8 slot, u8 func, u8 offset) {
    u32 address = 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

inline void pci_write(u16 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 address = 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

u32 pci_read_dword(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address =
        (1 << 31) |
        ((u32)bus  << 16) |
        ((u32)slot << 11) |
        ((u32)func << 8)  |
        (offset & 0xFC);

    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_dword(u8 bus, u8 slot, u8 func, u8 offset, u32 value)
{
    u32 address =
        (1 << 31) |
        ((u32)bus  << 16) |
        ((u32)slot << 11) |
        ((u32)func << 8)  |
        (offset & 0xFC);

    outl(0xCF8, address);
    outl(0xCFC, value);
}

inline u32 mmio_read32(u32 addr) {
    return *(volatile u32*)addr;
}

inline void mmio_write32(u32 addr, u32 val) {
    *(volatile u32*)addr = val;
}