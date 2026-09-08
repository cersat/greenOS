#ifndef DISK_H
#define DISK_H

#include "in-out.h"

void disk_init(void);
int disk_read_lba(u32 lba, void *buf);
int disk_write_lba(u32 lba, void *buf);
u32 disk_get_total_sectors(void);

#endif
