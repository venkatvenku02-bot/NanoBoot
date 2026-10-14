CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc

CFLAGS = -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany \
         -ffreestanding -fno-builtin -fno-stack-protector \
         -nostdlib -Wall -Wextra -O2

LDFLAGS = -T kernel/linker.ld -nostdlib -static

all: nanoboot.elf

nanoboot.elf: kernel/boot.S kernel/trap.S kernel/kernel.c kernel/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o nanoboot.elf kernel/boot.S kernel/trap.S kernel/kernel.c

clean:
	rm -f nanoboot.elf

.PHONY: all clean
