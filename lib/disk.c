#include "in-out.h"
#include "disk.h"
#include "tramp_blob.h"

extern void bios_call(void);

#define BCB_CMD    (*(volatile u8*)0x500A)
#define BCB_DRIVE  (*(volatile u8*)0x500B)
#define BCB_AH     (*(volatile u8*)0x500C)
#define BCB_CF     (*(volatile u8*)0x500D)
#define BOOTDRV    (*(volatile u8*)0x0500)
#define BOUNCE     ((volatile u8*)0x6000)

extern void *memcpy(void *dest, const void *src, unsigned int n);

static void dap_set(u16 cnt, u32 lba) {
    volatile u8 *d = (volatile u8*)0x5010;
    d[0] = 0x10; d[1] = 0;
    *(volatile u16*)(d+2) = cnt;
    *(volatile u16*)(d+4) = 0x0000;   /* offset буфера (BOUNCE) */
    *(volatile u16*)(d+6) = 0x0600;   /* сегмент буфера: 0x0600:0x0000 = 0x6000 */
    *(volatile u32*)(d+8) = lba;
    *(volatile u32*)(d+12) = 0;
}

void disk_init(void) {
    /* копируем трамплин в фиксированный физический адрес 0x2000 */
    unsigned char *dst = (unsigned char*)0x2000;
    for (unsigned int i = 0; i < tramp_bin_len; i++)
        dst[i] = tramp_bin[i];
}

int disk_read_lba(u32 lba, void *buf) {
    dap_set(1, lba);
    BCB_CMD = 0x42;
    BCB_DRIVE = BOOTDRV;
    bios_call();
    if (BCB_CF) return BCB_AH ? BCB_AH : 0xFF;
    memcpy(buf, (void*)BOUNCE, 512);
    return 0;
}

int disk_write_lba(u32 lba, void *buf) {
    memcpy((void*)BOUNCE, buf, 512);
    dap_set(1, lba);
    BCB_CMD = 0x43;
    BCB_DRIVE = BOOTDRV;
    bios_call();
    return BCB_CF ? (BCB_AH ? BCB_AH : 0xFF) : 0;
}

u32 disk_get_total_sectors(void) {
    volatile u8 *d = (volatile u8*)0x5010;
    *(volatile u16*)(d+0) = 0x1E;   /* buffer size, BIOS ждёт это поле заполненным */
    for (int i = 2; i < 0x1E; i++) d[i] = 0;

    BCB_CMD = 0x48;
    BCB_DRIVE = BOOTDRV;
    bios_call();
    if (BCB_CF) return 0;   /* extensions не поддерживаются или ошибка */

    return *(volatile u32*)(d + 0x10);   /* младшие 32 бита total_sectors */
}
