#!/usr/bin/env bash

echo "running in mingw64"

set -euo pipefail

export PATH="/c/msys64/i686-elf-tools/bin:$PATH"

nasm -f bin tramp.asm -o tramp.bin
python3 - <<'PY'
data = open("tramp.bin","rb").read()
with open("lib/tramp_blob.h","w") as f:
    f.write("#ifndef TRAMP_BLOB_H\n#define TRAMP_BLOB_H\n\n")
    f.write("static const unsigned char tramp_bin[] = {\n")
    for i in range(0, len(data), 16):
        f.write("  " + ",".join(str(b) for b in data[i:i+16]) + ",\n")
    f.write("};\n")
    f.write(f"static const unsigned int tramp_bin_len = {len(data)};\n\n")
    f.write("#endif\n")
PY

CFLAGS=(
  -ffreestanding -m32
  -fno-stack-protector -fno-pic
  -Wbuiltin-declaration-mismatch
  -mgeneral-regs-only -mno-mmx 
  -mno-sse -mno-sse2 -Wall -Wextra
  -Wunused-but-set-variable
)

i686-elf-gcc "${CFLAGS[@]}" -c kernel.c -o kernel.o

obj_files=(kernel.o)
for src in lib/*.c; do
    obj="${src%.c}.o"
    obj="${obj//\//_}"
    i686-elf-gcc "${CFLAGS[@]}" -c "$src" -o "$obj"
    obj_files+=("$obj")
done

nasm -f elf32 isr.s -o isr.o
nasm -f elf32 diskcall.asm -o diskcall.o
obj_files+=(isr.o diskcall.o)

i686-elf-ld -T linker.ld -o kernel.elf "${obj_files[@]}"
i686-elf-objcopy -O binary kernel.elf kernel.bin

kernel_size=$(stat -c%s kernel.bin)
kernel_sectors=$(( (kernel_size + 511) / 512 ))

nasm -f bin -d KERNEL_SECTORS=${kernel_sectors} boot.asm -o boot.bin

# disk.img мог ещё не существовать (первый запуск) - создаём с запасом,
# если файла нет; если есть - не трогаем размер, просто перезаписываем начало.
if [ ! -f disk.img ]; then
    dd if=/dev/zero of=disk.img bs=1M count=16 status=none
fi

dd if=boot.bin of=disk.img conv=notrunc
dd if=kernel.bin of=disk.img bs=512 seek=1 conv=notrunc

echo "kernel.bin size: ${kernel_size} bytes (${kernel_sectors} sectors)"
echo "Updated disk.img"
qemu-system-i386 \
  -drive format=raw,file=disk.img \
  -D qemu.log -monitor stdio -d int

rm -f *.bin *.o *.elf

echo "emulation ended"
