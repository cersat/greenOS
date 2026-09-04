#ifndef FS_H
#define FS_H

#include "in-out.h"

typedef struct {
	char path[128];
	u32 mark_s;
	u32 content[8];
} file;

extern u8 inited;

void init_fs(void);
void format(char *name);

u32  alloc_sector(void);
void free_sector(u32 lba);

u32  getMarkS(u16 num);
void setMarkS(u16 num, u32 mark);

file getFile(u32 mark);
void writeFile(file *f);
void setFile(u32 mark, u8 part, u8 *content);
u8   readFilePart(u32 mark, u8 part, u8 *out_buf);

u32  createFile(const char *path);
void deleteFile(u32 mark);
u32  findFile(const char *path);
void listFiles_(void (*print)(const char*));
void listFiles(void (*print)(const char*), char *filter);

#endif