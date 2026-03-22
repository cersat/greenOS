#!/usr/bin/env bash

echo "running in mingw32"

set -euo pipefail

export PATH="/c/msys64/i686-elf-tools/bin:$PATH"

i686-elf-gcc \
  -ffreestanding -m32 \
  -fno-stack-protector -fno-pic \
  -c kernel.c -o kernel.o

i686-elf-gcc -c kernel.c -o kernel.o
i686-elf-gcc -c lib/in-out.c -o in-out.o
i686-elf-gcc -c lib/video.c -o video.o
i686-elf-gcc -c lib/keyboard.c -o keyboard.o
i686-elf-gcc -c lib/usb.c -o usb.o

i686-elf-ld -T linker.ld -o kernel.elf \
  kernel.o in-out.o video.o keyboard.o usb.o
i686-elf-objcopy -O binary kernel.elf kernel.bin

kernel_size=$(stat -c%s kernel.bin)
kernel_sectors=$(( (kernel_size + 511) / 512 ))

nasm -f bin -d KERNEL_SECTORS=${kernel_sectors} boot.asm -o boot.bin
dd if=boot.bin of=disk.img conv=notrunc
dd if=kernel.bin of=disk.img bs=512 seek=1 conv=notrunc

echo "kernel.bin size: ${kernel_size} bytes (${kernel_sectors} sectors)"
echo "Updated disk.img"
qemu-system-i386 \
  -drive format=raw,file=disk.img \
  -device usb-ehci,id=ehci \
  -device usb-storage,bus=ehci.0,drive=stick \
  -drive if=none,id=stick,format=raw,file=disk.img

rm -f *.bin *.o *.elf

echo "emulation ended"