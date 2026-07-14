# =================================================================
#  Makefile - Quantum-Assisted Secure Bootloader
# =================================================================
#  make            builds build/disk.img
#  make run        builds (if needed) and boots it in QEMU
#  make run-debug  same, but also writes build/debug.log (QEMU
#                  debug-console port 0xE9 output - see boot/*.asm)
#  make clean      removes the build/ directory
#
#  Requires: nasm, gcc, ld, objcopy, python3 (+ qiskit, qiskit-aer),
#            qemu-system-i386 (only for `make run*`)
# =================================================================

ASM      := nasm
CC       := gcc
LD       := ld
OBJCOPY  := objcopy
PYTHON   := python3
QEMU     := qemu-system-i386

BUILD    := build
BOOT     := boot
KERNEL   := kernel
TOOLS    := tools

KERNEL_MAX_BYTES := 32768   # must match KERNEL_SECTORS(64)*512 in stage2.asm

CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra

.PHONY: all run run-debug clean

all: $(BUILD)/disk.img

$(BUILD):
	mkdir -p $(BUILD)

# ---------------------------------------------------------------
# 1. Tiny kernel (C + asm entry stub -> flat binary)
# ---------------------------------------------------------------
$(BUILD)/entry.o: $(KERNEL)/entry.asm | $(BUILD)
	$(ASM) -f elf32 $(KERNEL)/entry.asm -o $@

$(BUILD)/kernel_main.o: $(KERNEL)/kernel.c $(KERNEL)/vga.h | $(BUILD)
	$(CC) $(CFLAGS) -c $(KERNEL)/kernel.c -o $@

$(BUILD)/vga.o: $(KERNEL)/vga.c $(KERNEL)/vga.h | $(BUILD)
	$(CC) $(CFLAGS) -c $(KERNEL)/vga.c -o $@

$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/kernel_main.o $(BUILD)/vga.o $(KERNEL)/linker.ld
	$(LD) -m elf_i386 -T $(KERNEL)/linker.ld -o $@ $(BUILD)/entry.o $(BUILD)/kernel_main.o $(BUILD)/vga.o

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# ---------------------------------------------------------------
# 2. Quantum Security Module (build-time only, see docs)
# ---------------------------------------------------------------
$(BUILD)/seed.txt $(BUILD)/quantum_key.bin: $(TOOLS)/quantum_key_gen.py | $(BUILD)
	$(PYTHON) $(TOOLS)/quantum_key_gen.py

# ---------------------------------------------------------------
# 3. Checksum + NASM seed include, derived from the kernel binary
#    and the quantum-derived seed
# ---------------------------------------------------------------
$(BUILD)/checksum.bin $(BUILD)/stage2_seed.inc: $(BUILD)/kernel.bin $(BUILD)/seed.txt $(TOOLS)/gen_checksum.py
	$(PYTHON) $(TOOLS)/gen_checksum.py $(BUILD)/kernel.bin $(BUILD)/seed.txt $(KERNEL_MAX_BYTES)

# ---------------------------------------------------------------
# 4. Bootloader stages
# ---------------------------------------------------------------
$(BUILD)/stage2.bin: $(BOOT)/stage2.asm $(BOOT)/gdt.inc $(BUILD)/stage2_seed.inc
	$(ASM) -f bin -I $(BOOT)/ -I $(BUILD)/ $(BOOT)/stage2.asm -o $@

$(BUILD)/boot1.bin: $(BOOT)/boot1.asm
	$(ASM) -f bin $(BOOT)/boot1.asm -o $@

# ---------------------------------------------------------------
# 5. Final disk image
#    Layout (512-byte sectors):
#      sector 0        -> boot1.bin   (MBR, 1 sector)
#      sectors 1-8     -> stage2.bin  (8 sectors / 4096 bytes)
#      sector 9        -> checksum.bin (4 bytes used, 1 sector reserved)
#      sectors 10-73   -> kernel.bin  (up to 64 sectors / 32 KB)
# ---------------------------------------------------------------
$(BUILD)/disk.img: $(BUILD)/boot1.bin $(BUILD)/stage2.bin $(BUILD)/checksum.bin $(BUILD)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=100 status=none
	dd if=$(BUILD)/boot1.bin    of=$@ conv=notrunc bs=512 seek=0  status=none
	dd if=$(BUILD)/stage2.bin   of=$@ conv=notrunc bs=512 seek=1  status=none
	dd if=$(BUILD)/checksum.bin of=$@ conv=notrunc bs=512 seek=9  status=none
	dd if=$(BUILD)/kernel.bin   of=$@ conv=notrunc bs=512 seek=10 status=none
	@echo "Built $@"

run: $(BUILD)/disk.img
	$(QEMU) -drive file=$(BUILD)/disk.img,format=raw

run-debug: $(BUILD)/disk.img
	$(QEMU) -drive file=$(BUILD)/disk.img,format=raw \
	        -debugcon file:$(BUILD)/debug.log -global isa-debugcon.iobase=0xe9

clean:
	rm -rf $(BUILD)
