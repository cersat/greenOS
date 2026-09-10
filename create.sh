#!/usr/bin/env bash
export PATH="/c/msys64/i686-elf-tools/bin:$PATH"

echo "running in mingw64"

set -euo pipefail

CACHE_DIR=".build_cache"
mkdir -p "$CACHE_DIR"

hash_of() {
    printf '%s' "$1" | md5sum | cut -d' ' -f1
}

# Возвращает 0 (true), если файл изменился или объектного файла нет.
# НЕ обновляет хеш сама - это делает mark_compiled после успешной сборки.
should_compile() {
    local src="$1"
    local obj="$2"

    if [ ! -f "$obj" ]; then
        return 0
    fi

    local hash_file="${CACHE_DIR}/$(hash_of "$src").md5"
    local current_hash
    current_hash=$(md5sum "$src" | cut -d' ' -f1)

    if [ -f "$hash_file" ] && [ "$(cat "$hash_file")" == "$current_hash" ]; then
        return 1
    else
        return 0
    fi
}

# Фиксирует текущий хеш файла ТОЛЬКО после того, как компиляция реально прошла успешно.
mark_compiled() {
    local src="$1"
    local hash_file="${CACHE_DIR}/$(hash_of "$src").md5"
    md5sum "$src" | cut -d' ' -f1 > "$hash_file"
}

# 0. Трамплин PM<->RM для BIOS INT13h (заменяет EHCI)
TRAMP_CHANGED=0
if should_compile "tramp.asm" "lib/tramp_blob.h"; then
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
    rm -f tramp.bin
    mark_compiled "tramp.asm"
    TRAMP_CHANGED=1
    echo "tramp.asm compiled -> lib/tramp_blob.h regenerated"
fi

CFLAGS=(
  -ffreestanding -m32
  -fno-stack-protector -fno-pic
  -Wbuiltin-declaration-mismatch
  -mgeneral-regs-only -mno-mmx
  -mno-sse -mno-sse2 -Wall -Wextra
)

obj_files=()

# 1. Компиляция kernel.c
KERNEL_OBJ="${CACHE_DIR}/kernel.o"
if should_compile "kernel.c" "$KERNEL_OBJ" || should_compile "commands.inc" "$KERNEL_OBJ"; then
    i686-elf-gcc "${CFLAGS[@]}" -c kernel.c -o "$KERNEL_OBJ"
    mark_compiled "kernel.c"
    echo "kernel.c compiled"
fi
obj_files+=("$KERNEL_OBJ")

# 2. Компиляция ВСЕХ .c из lib/ автоматически
# disk.c инклюдит lib/tramp_blob.h - если tramp.asm поменялся, нужно
# принудительно пересобрать disk.c, даже если сам disk.c не менялся.
for src in lib/*.c; do
    flat_name="${src//\//_}"
    obj="${CACHE_DIR}/${flat_name%.c}.o"

    force=0
    if [ "$src" = "lib/disk.c" ] && [ "$TRAMP_CHANGED" -eq 1 ]; then
        force=1
    fi

    if [ "$force" -eq 1 ] || should_compile "$src" "$obj"; then
        i686-elf-gcc "${CFLAGS[@]}" -c "$src" -o "$obj"
        mark_compiled "$src"
        echo "$src compiled"
    fi
    obj_files+=("$obj")
done

# 3. Компиляция ISR + диск-трамплин
ISR_OBJ="${CACHE_DIR}/isr.o"
if should_compile "isr.s" "$ISR_OBJ"; then
    nasm -f elf32 isr.s -o "$ISR_OBJ"
    mark_compiled "isr.s"
    echo "isr.s compiled"
fi

DC_OBJ="${CACHE_DIR}/diskcall.o"
if should_compile "diskcall.asm" "$DC_OBJ"; then
    nasm -f elf32 diskcall.asm -o "$DC_OBJ"
    mark_compiled "diskcall.asm"
    echo "diskcall.asm compiled"
fi
obj_files+=("$ISR_OBJ" "$DC_OBJ")

# 4. Линковка (делаем всегда - дёшево по сравнению с компиляцией)
i686-elf-ld -T linker.ld -o kernel.elf "${obj_files[@]}"

# 5. Остальное без изменений
i686-elf-objcopy -O binary kernel.elf kernel.bin

kernel_size=$(stat -c%s kernel.bin)
kernel_sectors=$(( (kernel_size + 511) / 512 ))

nasm -f bin -d KERNEL_SECTORS=${kernel_sectors} boot.asm -o boot.bin

echo "kernel.bin size: ${kernel_size} bytes (${kernel_sectors} sectors)"
echo "Created kernel.bin and boot.bin"

cat boot.bin kernel.bin > os-image.bin
./os-image-create.bat
rm -f kernel.bin boot.bin kernel.elf
echo "built os-image.bin"

echo "finished create.sh"
