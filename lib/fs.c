#include "fs.h"
#include "disk.h"
#include "in-out.h"
#include "string.h"

#define MAX_CONTENT      8     // секторов данных на файл (пока без цепочек)
#define BITMAP_SECTOR    51    // где НАЧИНАЕТСЯ bitmap (фиксировано)
#define SLOTS_PER_SECTOR 127   // marks в одном list-секторе (508 байт) + 4 байта на "next"

u32 disk_s = 49;
u32 list_s = 50;    // корень единственного списка файлов (всегда фиксирован)
u8 inited = 0;
char disk_name[16];
u32 diskptr;
u32 bitmap_sectors = 1;
u32 data_start = 64;
u32 data_sectors_total = 512 * 8;

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

	u32 total = disk_get_total_sectors();
	if (total > diskptr) {
		u32 span = total - diskptr;
		bitmap_sectors = (span + 4096 - 1) / 4096;
		if (bitmap_sectors == 0) bitmap_sectors = 1;
		data_start = diskptr + bitmap_sectors;
		data_sectors_total = (total > data_start) ? (total - data_start) : 0;
	}
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

	// очищаем корень списка файлов (единственный фиксированный сектор)
	u8 zero[512] = {0};
	disk_write_lba(list_s, zero);

	u32 total = disk_get_total_sectors();
	if (total > diskptr) {
		u32 span = total - diskptr;
		bitmap_sectors = (span + 4096 - 1) / 4096;
		if (bitmap_sectors == 0) bitmap_sectors = 1;
	} else {
		bitmap_sectors = 1;
	}
	data_start = diskptr + bitmap_sectors;
	data_sectors_total = (total > data_start) ? (total - data_start) : 0;

	for (u32 i = 0; i < bitmap_sectors; i++)
		disk_write_lba(diskptr + i, zero);

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

u32 alloc_sector() {
	if (!inited) return 0;

	for (u32 sec = 0; sec < bitmap_sectors; sec++) {
		u8 bm[512];
		disk_read_lba(diskptr + sec, bm);

		u32 base = sec * 4096;
		for (u32 local = 0; local < 4096; local++) {
			u32 gidx = base + local;
			if (gidx >= data_sectors_total) break;

			if (!bit_get(bm, local)) {
				bit_set(bm, local, 1);
				disk_write_lba(diskptr + sec, bm);
				return data_start + gidx;
			}
		}
	}
	return 0;
}

void free_sector(u32 lba) {
	if (!inited || lba < data_start) return;
	u32 gidx = lba - data_start;
	if (gidx >= data_sectors_total) return;

	u32 sec = gidx / 4096;
	u32 local = gidx % 4096;

	u8 bm[512];
	disk_read_lba(diskptr + sec, bm);
	bit_set(bm, local, 0);
	disk_write_lba(diskptr + sec, bm);
}

/* ---------- список файлов: растёт цепочкой секторов по мере надобности ---------- */

// найти LBA сектора, где физически лежит слот с глобальным индексом num
// (переходя по цепочке "next"); если grow=1 и цепочка коротка - достраивает её
static u32 list_walk(u32 num, u8 grow) {
	u32 sector_idx = num / SLOTS_PER_SECTOR;
	u32 cur = list_s;

	for (u32 hop = 0; hop < sector_idx; hop++) {
		u8 buf[512];
		disk_read_lba(cur, buf);
		u32 next = buf[508] | (buf[509] << 8) | (buf[510] << 16) | ((u32)buf[511] << 24);

		if (next == 0) {
			if (!grow) return 0;
			next = alloc_sector();
			if (next == 0) return 0;   // диск полон

			u8 zero[512] = {0};
			disk_write_lba(next, zero);

			buf[508] = (u8)(next & 0xFF);
			buf[509] = (u8)((next >> 8) & 0xFF);
			buf[510] = (u8)((next >> 16) & 0xFF);
			buf[511] = (u8)((next >> 24) & 0xFF);
			disk_write_lba(cur, buf);
		}
		cur = next;
	}
	return cur;
}

u32 getMarkS(u32 num) {
	if (!inited) return 0;
	u32 sec = list_walk(num, 0);
	if (sec == 0) return 0;

	u8 buf[512];
	disk_read_lba(sec, buf);
	u32 off = (num % SLOTS_PER_SECTOR) * 4;
	return buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((u32)buf[off+3] << 24);
}

void setMarkS(u32 num, u32 mark) {
	if (!inited) return;
	u32 sec = list_walk(num, 1);
	if (sec == 0) return;

	u8 buf[512];
	disk_read_lba(sec, buf);
	u32 off = (num % SLOTS_PER_SECTOR) * 4;
	buf[off]   = (u8)(mark & 0xFF);
	buf[off+1] = (u8)((mark >> 8) & 0xFF);
	buf[off+2] = (u8)((mark >> 16) & 0xFF);
	buf[off+3] = (u8)((mark >> 24) & 0xFF);
	disk_write_lba(sec, buf);
}

// первый свободный слот (mark==0) в пределах уже существующей цепочки,
// либо следующий по порядку индекс сразу за концом цепочки (тогда setMarkS её достроит)
static u32 find_free_slot() {
	u32 cur = list_s;
	u32 base = 0;
	for (;;) {
		u8 buf[512];
		disk_read_lba(cur, buf);

		for (u32 i = 0; i < SLOTS_PER_SECTOR; i++) {
			u32 off = i * 4;
			u32 mark = buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((u32)buf[off+3] << 24);
			if (mark == 0) return base + i;
		}

		u32 next = buf[508] | (buf[509] << 8) | (buf[510] << 16) | ((u32)buf[511] << 24);
		if (next == 0) return base + SLOTS_PER_SECTOR;   // конец цепочки - расти сюда

		cur = next;
		base += SLOTS_PER_SECTOR;
	}
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
		if (f.content[part] == 0) return;
		writeFile(&f);
	}
	disk_write_lba(f.content[part], content);
}

u8 readFilePart(u32 mark, u8 part, u8 *out_buf) {
	if (!inited || part >= MAX_CONTENT) return 0;
	file f = getFile(mark);
	if (f.content[part] == 0) return 0;
	disk_read_lba(f.content[part], out_buf);
	return 1;
}

u32 createFile(const char *path) {
	if (!inited) return 0;
	if (findFile(path)) return 0;

	u32 slot = find_free_slot();

	u32 mark = alloc_sector();
	if (mark == 0) return 0;

	file f = {0};
	memcpy(f.path, path, (strlen(path) > 127) ? 127 : strlen(path));
	f.mark_s = mark;
	writeFile(&f);

	setMarkS(slot, mark);
	return mark;
}

void deleteFile(u32 mark) {
	if (!inited || mark == 0) return;

	file f = getFile(mark);
	for (u8 i = 0; i < MAX_CONTENT; i++)
		if (f.content[i]) free_sector(f.content[i]);

	free_sector(mark);

	u32 cur = list_s;
	while (cur) {
		u8 buf[512];
		disk_read_lba(cur, buf);
		u8 changed = 0;

		for (u32 i = 0; i < SLOTS_PER_SECTOR; i++) {
			u32 off = i * 4;
			u32 m = buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((u32)buf[off+3] << 24);
			if (m == mark) {
				buf[off] = buf[off+1] = buf[off+2] = buf[off+3] = 0;
				changed = 1;
				break;
			}
		}
		if (changed) { disk_write_lba(cur, buf); return; }

		cur = buf[508] | (buf[509] << 8) | (buf[510] << 16) | ((u32)buf[511] << 24);
	}
}

u32 findFile(const char *path) {
	if (!inited) return 0;

	u32 cur = list_s;
	while (cur) {
		u8 buf[512];
		disk_read_lba(cur, buf);

		for (u32 i = 0; i < SLOTS_PER_SECTOR; i++) {
			u32 off = i * 4;
			u32 mark = buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((u32)buf[off+3] << 24);
			if (mark == 0) continue;
			file f = getFile(mark);
			if (strcmp(f.path, path) == 0) return mark;
		}

		cur = buf[508] | (buf[509] << 8) | (buf[510] << 16) | ((u32)buf[511] << 24);
	}
	return 0;
}

void listFiles_(void (*print)(const char*)) {
	listFiles(print, "/");
}

void listFiles(void (*print)(const char*), char *filter) {
	if (!inited) return;
	u32 flen = strlen(filter);

	u32 cur = list_s;
	while (cur) {
		u8 buf[512];
		disk_read_lba(cur, buf);

		for (u32 i = 0; i < SLOTS_PER_SECTOR; i++) {
			u32 off = i * 4;
			u32 mark = buf[off] | (buf[off+1] << 8) | (buf[off+2] << 16) | ((u32)buf[off+3] << 24);
			if (mark == 0) continue;

			file f = getFile(mark);
			u32 j = 0;
			while (j < flen && f.path[j] == filter[j]) j++;
			if (j != flen) continue;

			print(f.path);
			print("\n");
		}

		cur = buf[508] | (buf[509] << 8) | (buf[510] << 16) | ((u32)buf[511] << 24);
	}
}
