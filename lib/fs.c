#include "fs.h"
#include "disk.h"
#include "in-out.h"
#include "string.h"

#define MAX_FILES     128   // 512/4 записей в list-секторе
#define MAX_CONTENT   8     // секторов данных на файл (пока без цепочек)
#define DATA_START    64    // где начинается область данных (после bitmap)
#define BITMAP_SECTOR 51    // отдельно от diskptr, для простоты фиксирован

u32 disk_s = 49;
u32 list_s = 50;
u8 inited = 0;
char disk_name[16];
u32 diskptr;

/* ---------- init / format ---------- */

void init_fs() {
	u8 buf[512];
	disk_read_lba(disk_s, buf);
	inited = (buf[0] == 'M' && buf[1] == 'A' && buf[2] == 'T');
	if (!inited) return;

	memcpy(disk_name, &buf[3], 16);

	u32 p3 = buf[19];
	u32 p2 = buf[19 + 1] << 8;
	u32 p1 = buf[19 + 2] << 16;
	u32 p0 = (u32)buf[19 + 3] << 24;
	diskptr = p3 + p2 + p1 + p0;
}

void format(char *name) {
	u8 buf[512] = {0};
	buf[0] = 'M'; buf[1] = 'A'; buf[2] = 'T';
	memcpy(&buf[3], name, 16);

	diskptr = BITMAP_SECTOR;
	buf[19]     = (u8)(diskptr & 0xFF);
	buf[19 + 1] = (u8)((diskptr >> 8) & 0xFF);
	buf[19 + 2] = (u8)((diskptr >> 16) & 0xFF);
	buf[19 + 3] = (u8)((diskptr >> 24) & 0xFF);
	disk_write_lba(disk_s, buf);

	// очищаем list (все mark-указатели = 0, т.е. "слот свободен")
	u8 zero[512] = {0};
	disk_write_lba(list_s, zero);

	// очищаем bitmap (все секторы свободны)
	disk_write_lba(diskptr, zero);

	inited = 1;
	memcpy(disk_name, name, 16);
}

/* ---------- bitmap-аллокатор ---------- */

static u8 bit_get(u8 *bm, u32 idx) {
	return (bm[idx / 8] >> (idx % 8)) & 1;
}
static void bit_set(u8 *bm, u32 idx, u8 val) {
	if (val) bm[idx / 8] |= (1 << (idx % 8));
	else     bm[idx / 8] &= ~(1 << (idx % 8));
}

// найти свободный сектор данных, пометить занятым, вернуть его LBA (0 = нет места)
u32 alloc_sector() {
	if (!inited) return 0;
	u8 bm[512];
	disk_read_lba(diskptr, bm);

	for (u32 i = 0; i < 512 * 8; i++) {
		if (!bit_get(bm, i)) {
			bit_set(bm, i, 1);
			disk_write_lba(diskptr, bm);
			return DATA_START + i;
		}
	}
	return 0; // диск полон
}

void free_sector(u32 lba) {
	if (!inited || lba < DATA_START) return;
	u8 bm[512];
	disk_read_lba(diskptr, bm);
	bit_set(bm, lba - DATA_START, 0);
	disk_write_lba(diskptr, bm);
}

/* ---------- работа со списком файлов ---------- */

u32 getMarkS(u16 num) {
	if (!inited || num >= MAX_FILES) return 0;
	u8 buf[512];
	disk_read_lba(list_s, buf);
	u16 dest = num * 4;
	u32 p3 = buf[dest];
	u32 p2 = buf[dest + 1] << 8;
	u32 p1 = buf[dest + 2] << 16;
	u32 p0 = (u32)buf[dest + 3] << 24;
	return p3 + p2 + p1 + p0;
}

void setMarkS(u16 num, u32 mark) {
	if (!inited || num >= MAX_FILES) return;
	u8 buf[512];
	disk_read_lba(list_s, buf);
	u16 dest = num * 4;
	buf[dest]     = (u8)(mark & 0xFF);
	buf[dest + 1] = (u8)((mark >> 8) & 0xFF);
	buf[dest + 2] = (u8)((mark >> 16) & 0xFF);
	buf[dest + 3] = (u8)((mark >> 24) & 0xFF);
	disk_write_lba(list_s, buf);
}

// первый свободный слот в list (mark == 0 значит пусто), -1 если нет места
static s32 find_free_slot() {
	for (u16 i = 0; i < MAX_FILES; i++)
		if (getMarkS(i) == 0) return i;
	return -1;
}

/* ---------- файлы ---------- */

file getFile(u32 mark) {
	file ret = {0};
	if (!inited || mark == 0) return ret;
	u8 buf[512];
	disk_read_lba(mark, buf);
	memcpy(ret.path, buf, 128);
	ret.mark_s = mark;
	memcpy(ret.content, &buf[128], MAX_CONTENT * 4);
	return ret;
}

void writeFile(file *f) {
	if (!inited || f->mark_s == 0) return;
	u8 buf[512] = {0};
	memcpy(buf, f->path, 128);
	memcpy(&buf[128], f->content, MAX_CONTENT * 4);
	disk_write_lba(f->mark_s, buf);
}

void setFile(u32 mark, u8 part, u8 *content) {
	if (!inited || part >= MAX_CONTENT) return;
	file f = getFile(mark);
	if (f.content[part] == 0) {
		f.content[part] = alloc_sector();
		if (f.content[part] == 0) return; // диск полон
		writeFile(&f);
	}
	disk_write_lba(f.content[part], content);
}

u8 readFilePart(u32 mark, u8 part, u8 *out_buf) {
	if (!inited || part >= MAX_CONTENT) return 0;
	file f = getFile(mark);
	if (f.content[part] == 0) return 0; // не аллоцирован
	disk_read_lba(f.content[part], out_buf);
	return 1;
}

// создать новый файл, вернуть mark-сектор (0 = не удалось: нет места в list или на диске)
u32 createFile(const char *path) {
	if (!inited) return 0;
	s32 slot = find_free_slot();
	if (slot < 0) return 0;

	u32 mark = alloc_sector();
	if (mark == 0) return 0;

	file f = {0};
	memcpy(f.path, path, (strlen(path) > 127) ? 127 : strlen(path));
	f.mark_s = mark;
	writeFile(&f);

	setMarkS((u16)slot, mark);
	return mark;
}

void deleteFile(u32 mark) {
	if (!inited || mark == 0) return;

	// освободить все занятые секторы данных
	file f = getFile(mark);
	for (u8 i = 0; i < MAX_CONTENT; i++)
		if (f.content[i]) free_sector(f.content[i]);

	// убрать сам mark-сектор
	free_sector(mark);

	// убрать из list
	for (u16 i = 0; i < MAX_FILES; i++) {
		if (getMarkS(i) == mark) {
			setMarkS(i, 0);
			break;
		}
	}
}

// найти файл по пути, вернуть mark (0 = не найден)
u32 findFile(const char *path) {
	if (!inited) return 0;
	for (u16 i = 0; i < MAX_FILES; i++) {
		u32 mark = getMarkS(i);
		if (mark == 0) continue;
		file f = getFile(mark);
		if (strcmp(f.path, path) == 0) return mark;
	}
	return 0;
}

// напечатать список файлов (для команды ls/list в шелле)
void listFiles_(void (*print)(const char*)) {
	listFiles(print, "/");
}

// напечатать список файлов (для команды ls/list в шелле) но с фильтром по префиксу
void listFiles(void (*print)(const char*), char *filter) {
	if (!inited) return;
	u32 flen = strlen(filter);

	for (u16 i = 0; i < MAX_FILES; i++) {
		u32 mark = getMarkS(i);
		if (mark == 0) continue;
		file f = getFile(mark);

		u32 j = 0;
		while (j < flen && f.path[j] == filter[j]) j++;
		if (j != flen) continue; // filter - не префикс f.path

		print(f.path);
		print("\n");
	}
}