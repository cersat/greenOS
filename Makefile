# Makefile для greenOS - заменяет create.sh, getFiles.sh, run.sh
#
#   make create   - собрать и сформировать os-image.bin, запустить os-image-create.bat (Rufus)
#   make get      - собрать, обновить disk.img и os-image.bin (без запуска чего-либо)
#   make run      - собрать, обновить disk.img, запустить QEMU
#
# Make сам вызывает /bin/sh для каждого рецепта, независимо от того, какой
# интерактивный шелл (fish, bash, что угодно) используется в терминале -
# SHELL ниже явно фиксирует bash, так что поведение гарантированно одинаковое
# у всех, а не зависит от личной оболочки того, кто это запускает.

SHELL := bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c
.SILENT:

TOOLCHAIN := /c/msys64/i686-elf-tools/bin
CC := $(TOOLCHAIN)/i686-elf-gcc
LD := $(TOOLCHAIN)/i686-elf-ld
OBJCOPY := $(TOOLCHAIN)/i686-elf-objcopy
export PATH := $(TOOLCHAIN):$(PATH)

CACHE_DIR := .build_cache

CFLAGS := -ffreestanding -m32 \
          -fno-stack-protector -fno-pic \
          -Wbuiltin-declaration-mismatch \
          -mgeneral-regs-only -mno-mmx \
          -mno-sse -mno-sse2 -Wall -Wextra

.PHONY: all create get run build update-disk clean

all: build

# ---------------------------------------------------------------------------
# Общая сборка: kernel.elf -> kernel.bin, boot.bin.
# Кеширование по хешу ПОСЛЕ препроцессинга (все #include уже развёрнуты) -
# любое изменение в подключаемом .h/.inc/commands.inc само меняет хеш,
# отдельно отслеживать такие зависимости не нужно.
# ---------------------------------------------------------------------------
build:
	mkdir -p "$(CACHE_DIR)"

	hash_of() {
	    printf '%s' "$$1" | md5sum | cut -d' ' -f1
	}

	preprocessed_hash() {
	    local src="$$1"
	    $(CC) $(CFLAGS) -E "$$src" 2>/dev/null | md5sum | cut -d' ' -f1
	}

	should_compile() {
	    local src="$$1"
	    local obj="$$2"
	    if [ ! -f "$$obj" ]; then
	        return 0
	    fi
	    local hash_file="$(CACHE_DIR)/$$(hash_of "$$src").md5"
	    local current_hash
	    current_hash=$$(preprocessed_hash "$$src")
	    if [ -f "$$hash_file" ] && [ "$$(cat "$$hash_file")" == "$$current_hash" ]; then
	        return 1
	    else
	        return 0
	    fi
	}

	mark_compiled() {
	    local src="$$1"
	    local hash_file="$(CACHE_DIR)/$$(hash_of "$$src").md5"
	    preprocessed_hash "$$src" > "$$hash_file"
	}

	# 1. Трамплин PM<->RM для BIOS INT13h - ассемблер, хешируем как есть
	TRAMP_HASH_FILE="$(CACHE_DIR)/$$(hash_of tramp.asm).asm.md5"
	TRAMP_CURRENT=$$(md5sum tramp.asm | cut -d' ' -f1)
	if [ ! -f lib/tramp_blob.h ] || [ ! -f "$$TRAMP_HASH_FILE" ] || [ "$$(cat "$$TRAMP_HASH_FILE")" != "$$TRAMP_CURRENT" ]; then
	    nasm -f bin tramp.asm -o tramp.bin
	    python3 scripts/gen_tramp_blob.py
	    rm -f tramp.bin
	    echo "$$TRAMP_CURRENT" > "$$TRAMP_HASH_FILE"
	    echo "tramp.asm compiled -> lib/tramp_blob.h regenerated"
	fi

	obj_files=()

	# 2. Компиляция kernel.c (commands.inc уже учтён хешем после препроцессинга)
	KERNEL_OBJ="$(CACHE_DIR)/kernel.o"
	if should_compile "kernel.c" "$$KERNEL_OBJ"; then
	    $(CC) $(CFLAGS) -c kernel.c -o "$$KERNEL_OBJ"
	    mark_compiled "kernel.c"
	    echo "kernel.c compiled"
	fi
	obj_files+=("$$KERNEL_OBJ")

	# 3. Компиляция файлов из lib/*.c (tramp_blob.h тоже учтён хешем disk.c)
	for src in lib/*.c; do
	    flat_name="$${src//\//_}"
	    obj="$(CACHE_DIR)/$${flat_name%.c}.o"
	    if should_compile "$$src" "$$obj"; then
	        $(CC) $(CFLAGS) -c "$$src" -o "$$obj"
	        mark_compiled "$$src"
	        echo "$$src compiled"
	    fi
	    obj_files+=("$$obj")
	done

	# 4. Ассемблирование isr и diskcall
	ISR_OBJ="$(CACHE_DIR)/isr.o"
	if should_compile "isr.s" "$$ISR_OBJ"; then
	    nasm -f elf32 isr.s -o "$$ISR_OBJ"
	    mark_compiled "isr.s"
	    echo "isr.s compiled"
	fi

	DC_OBJ="$(CACHE_DIR)/diskcall.o"
	if should_compile "diskcall.asm" "$$DC_OBJ"; then
	    nasm -f elf32 diskcall.asm -o "$$DC_OBJ"
	    mark_compiled "diskcall.asm"
	    echo "diskcall.asm compiled"
	fi
	obj_files+=("$$ISR_OBJ" "$$DC_OBJ")

	# 5. Линковка (делаем всегда - дёшево по сравнению с компиляцией)
	$(LD) -T linker.ld -o kernel.elf "$${obj_files[@]}"
	$(OBJCOPY) -O binary kernel.elf kernel.bin

	kernel_size=$$(stat -c%s kernel.bin)
	kernel_sectors=$$(( (kernel_size + 511) / 512 ))

	# 6. Сборка boot.asm - зависит от размера kernel.bin, кеш тут не подходит
	nasm -f bin -d KERNEL_SECTORS=$${kernel_sectors} boot.asm -o boot.bin

	echo "kernel.bin size: $${kernel_size} bytes ($${kernel_sectors} sectors)"

# ---------------------------------------------------------------------------
# Общий шаг: записать boot.bin+kernel.bin в disk.img (создать, если его нет)
# ---------------------------------------------------------------------------
update-disk: build
	if [ ! -f disk.img ]; then
	    dd if=/dev/zero of=disk.img bs=1M count=16 status=none
	fi
	dd if=boot.bin of=disk.img conv=notrunc status=none
	dd if=kernel.bin of=disk.img bs=512 seek=1 conv=notrunc status=none
	echo "Updated disk.img"

# ---------------------------------------------------------------------------
# make create - аналог create.sh: os-image.bin + запуск os-image-create.bat (Rufus)
# ---------------------------------------------------------------------------
create: build
	cat boot.bin kernel.bin > os-image.bin
	./os-image-create.bat
	rm -f kernel.bin boot.bin kernel.elf
	echo "built os-image.bin"
	echo "finished create"

# ---------------------------------------------------------------------------
# make get - аналог getFiles.sh: обновить disk.img и os-image.bin
# ---------------------------------------------------------------------------
get: update-disk
	cat boot.bin kernel.bin > os-image.bin
	echo "Updated os-image.bin"
	rm -f boot.bin kernel.bin kernel.elf

# ---------------------------------------------------------------------------
# make run - аналог run.sh: обновить disk.img, запустить QEMU
# QEMU -monitor stdio требует полноценный интерактивный терминал - </dev/tty
# привязывает stdin напрямую к реальному tty в обход того, как make сам
# передаёт stdin рецептам.
# ---------------------------------------------------------------------------
run: update-disk
	qemu-system-i386 \
	  -drive format=raw,file=disk.img \
	  -D qemu.log -monitor stdio -d int < /dev/tty
	rm -f *.bin kernel.elf
	echo "emulation ended"

clean:
	rm -rf "$(CACHE_DIR)" *.bin *.elf lib/tramp_blob.h
