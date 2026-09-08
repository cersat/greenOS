#!/usr/bin/env bash
export PATH="/c/msys64/i686-elf-tools/bin:$PATH"

echo "running in mingw64"

set -euo pipefail

# 0. Трамплин PM<->RM для BIOS INT13h (заменяет EHCI)
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

# 1. Компиляция kernel.c
i686-elf-gcc "${CFLAGS[@]}" -c kernel.c -o kernel.o

# 2. Компиляция ВСЕХ .c из lib/ автоматически
obj_files=(kernel.o)
for src in lib/*.c; do
    obj="${src%.c}.o"
    obj="${obj//\//_}"     # lib/foo.o -> lib_foo.o, чтобы не путались имена
    i686-elf-gcc "${CFLAGS[@]}" -c "$src" -o "$obj"
    obj_files+=("$obj")
done

# 3. Компиляция ISR + диск-трамплин
nasm -f elf32 isr.s -o isr.o
nasm -f elf32 diskcall.asm -o diskcall.o
obj_files+=(isr.o diskcall.o)

# 4. Линковка
i686-elf-ld -T linker.ld -o kernel.elf "${obj_files[@]}"

# 5. Остальное без изменений
i686-elf-objcopy -O binary kernel.elf kernel.bin

kernel_size=$(stat -c%s kernel.bin)
kernel_sectors=$(( (kernel_size + 511) / 512 ))

nasm -f bin -d KERNEL_SECTORS=${kernel_sectors} boot.asm -o boot.bin

echo "kernel.bin size: ${kernel_size} bytes (${kernel_sectors} sectors)"
echo "Created kernel.bin and boot.bin"

./os-image-create.bat
rm -f kernel.bin tramp.bin boot.bin kernel.elf *.o
echo "built os-image.bin"

echo "finished create.sh"
