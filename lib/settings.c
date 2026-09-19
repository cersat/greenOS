#include "settings.h"
#include "fs.h"

u8 settings[4096];

s16 settings_read(u16 set) {
	if(set >= 4096) return -1;
	u32 sector = findFile("/settings.bin");
	if(sector) {
        for(int i = 0; i < 8; i++) readFilePart(sector, i, &settings[i * 512]);
        return (s16)settings[set];
    }
	return -1;
}

s8 settings_write(u16 set, u8 write) {
	if(set >= 4096) return -1;
	u32 sector = findFile("/settings.bin");
	if(sector) {
        for(int i = 0; i < 8; i++) readFilePart(sector, i, &settings[i * 512]);
        settings[set] = write;
        for(int i = 0; i < 8; i++) setFile(sector, i, &settings[i * 512]);
		return 0;
    }
	return -1;
}

void settings_create() {
	createFile("/settings.bin");
}