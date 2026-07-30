# AtlasOS

A from-scratch x86 boot chain and tiny operating system, built to
learn (and be able to explain) the fundamentals of OS and firmware
development:

```
BIOS
 |
 v
Custom two-stage bootloader (boot1.asm -> stage2.asm)
 |
 v
Kernel integrity check (build-time checksum, verified before load)
 |
 v
32-bit protected mode (GDT, A20, CR0.PE)
 |
 v
Kernel bring-up:
  IDT + PIC remap -> PIT timer (IRQ0) -> PS/2 keyboard (IRQ1)
  -> serial (COM1) -> heap allocator (kmalloc) -> paging
 |
 v
Interactive shell
```

Full write-up of every subsystem - what it does, why it's built
that way, and how to talk through it in an interview - is in
[`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md).

## Project layout

```
boot/       x86 assembly: MBR stage 1 + stage 2 (protected-mode transition)
kernel/     C kernel: drivers, IDT/ISR plumbing, allocator, paging, shell
tools/      Build-time checksum + seed generation (kernel integrity check)
build/      Generated at build time (binaries, disk image) - not committed
docs/       Full documentation
Makefile    Orchestrates the whole build
```

### Kernel source map

| File(s)                  | Subsystem                                    |
|---------------------------|-----------------------------------------------|
| `entry.asm`, `kernel.c`   | Entry stub + bring-up sequence                 |
| `vga.c/h`                 | VGA text-mode driver (with real scrolling)     |
| `idt.c/h`, `isr.asm/c/h`  | Interrupt Descriptor Table + ISR/IRQ dispatch  |
| `pic.c/h`                 | 8259 PIC remap + EOI                           |
| `timer.c/h`                | PIT timer driver (IRQ0)                        |
| `keyboard.c/h`             | PS/2 keyboard driver (IRQ1)                    |
| `serial.c/h`                | COM1 UART driver                               |
| `kmalloc.c/h`               | Heap allocator (first-fit, split/coalesce)     |
| `paging.c/h`                 | Identity-mapped paging (4 MB pages, PSE)       |
| `shell.c/h`                   | Command shell                                  |
| `string.c/h`                    | Freestanding memset/memcpy/strcmp/itoa/etc.  |

## Requirements

- `nasm`
- `gcc` + `ld` + `objcopy` (any recent GCC toolchain with 32-bit
  support, i.e. `gcc-multilib` on some distros)
- `python3` (standard library only - no extra packages needed)
- `qemu-system-i386` (to actually boot and test it)

On Ubuntu/Debian:

```bash
sudo apt-get install nasm gcc qemu-system-x86
```

## Build & run

```bash
make            # builds build/disk.img
make run        # builds (if needed) and boots it in QEMU
make run-debug  # same, plus writes build/debug.log with boot-stage
                 # and kernel debug output (see "Testing" in
                 # DOCUMENTATION.md)
make clean      # remove all generated files
```

You'll see the bootloader's progress messages, then the `AtlasOS`
banner as each kernel subsystem comes online, then a shell prompt.
Type `help` for the list of commands.

## What this project demonstrates

- Real mode -> protected mode transition (GDT, `CR0.PE`, A20 line, far jump)
- A minimal, from-scratch two-stage bootloader using BIOS INT 13h (LBA)
- A build-time kernel integrity check that the bootloader actually
  enforces before jumping to the kernel
- Interrupt handling from the ground up: IDT, 8259 PIC remap,
  CPU-exception vs. hardware-IRQ dispatch, and two real IRQ-driven
  drivers (PIT timer, PS/2 keyboard)
- A polled UART (serial) driver
- A hand-written heap allocator (`kmalloc`/`kfree`) with block
  splitting and coalescing
- Enabling paging correctly (`CR4.PSE`, `CR3`, `CR0.PG`) with a
  flat identity map
- An interactive shell built entirely on the drivers above (no
  libc, no OS underneath it)

## What this project deliberately does *not* implement

No scheduler/multitasking, no filesystem, no user mode or
syscalls, no dynamic (non-identity) virtual memory, no drivers
beyond VGA/keyboard/serial. This is scoped as a boot chain +
freestanding kernel project - see `docs/DOCUMENTATION.md` for the
reasoning behind that scope and what each omission would take to
add.
