#!/usr/bin/env bash
export PATH="/c/msys64/i686-elf-tools/bin:$PATH"

echo "running in mingw32"

set -euo pipefail

# 1. Компиляция kernel.c
i686-elf-gcc \
  -ffreestanding -m32 \
  -fno-stack-protector -fno-pic \
  -c kernel.c -o kernel.o \
  -Wbuiltin-declaration-mismatch

# 2. Компиляция библиотек
i686-elf-gcc -c kernel.c -o kernel.o
i686-elf-gcc -c lib/in-out.c -o in-out.o
i686-elf-gcc -c lib/video.c -o video.o
i686-elf-gcc -c lib/keyboard.c -o keyboard.o
i686-elf-gcc -c lib/usb.c -o usb.o

# 3. Компиляция ISR (обработчики прерываний)
nasm -f elf32 isr.s -o isr.o

# 4. Линковка
i686-elf-ld -T linker.ld -o kernel.elf \
  kernel.o in-out.o video.o keyboard.o usb.o isr.o 

# 5. Остальное без изменений
i686-elf-objcopy -O binary kernel.elf kernel.bin

kernel_size=$(stat -c%s kernel.bin)
kernel_sectors=$(( (kernel_size + 511) / 512 ))

nasm -f bin -d KERNEL_SECTORS=${kernel_sectors} boot.asm -o boot.bin

echo "kernel.bin size: ${kernel_size} bytes (${kernel_sectors} sectors)"
echo "Created kernel.bin and boot.bin"

./os-image-create.bat
rm -f kernel.bin boot.bin kernel.elf *.o
echo "built os-image.bin"

echo "finished create.sh"
