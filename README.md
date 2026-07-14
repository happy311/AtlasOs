# Quantum-Assisted Secure Bootloader

A from-scratch x86 boot chain — BIOS → custom two-stage bootloader
→ kernel-integrity verification → 32-bit protected mode → a tiny
kernel — paired with a **Quantum Security Module**: a Qiskit
simulation of the BB84 Quantum Key Distribution protocol that
seeds the bootloader's integrity checksum at build time.

```
BIOS
 |
 v
Custom Bootloader (boot1.asm -> stage2.asm)
 |
 v
Kernel Integrity Verification   <-- checksum seeded by
 |                                   Quantum Security Module
 v
Load Kernel (32-bit protected mode)
 |
 v
Tiny Operating System (kernel.c)
```

Full write-up of every design decision, why it's built this way,
and the exact limitations of the "quantum" part is in
[`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md). Read that before
using this on a resume — it explains what's real and what's a
clearly-scoped simulation.

## Project layout

```
boot/       x86 assembly: MBR stage 1 + stage 2 (protected mode transition)
kernel/     Tiny C kernel + VGA driver + linker script
tools/      Quantum Security Module (Qiskit) + checksum build tool
build/      Generated at build time (binaries, disk image) - not committed
docs/       Full documentation
Makefile    Orchestrates the whole build
```

## Requirements

- `nasm`
- `gcc` + `ld` + `objcopy` (any recent GCC toolchain with 32-bit support,
  i.e. `gcc-multilib` on some distros)
- `python3` with `qiskit` and `qiskit-aer` (`pip install -r tools/requirements.txt`)
- `qemu-system-i386` (to actually boot and test it)

On Ubuntu/Debian:

```bash
sudo apt-get install nasm gcc qemu-system-x86
pip install -r tools/requirements.txt
```

## Build & run

```bash
make            # builds build/disk.img
make run        # builds (if needed) and boots it in QEMU
make run-debug  # same, plus writes build/debug.log with boot-stage output
                # (see "Testing" in DOCUMENTATION.md for why this exists)
make clean      # remove all generated files
```

You should see the bootloader's messages, then a `Tiny OS` banner
printed directly to VGA text memory confirming the kernel is running
in 32-bit protected mode.

## What this project demonstrates

- Real mode → protected mode transition (GDT, `CR0.PE`, A20 line, far jump)
- A minimal, from-scratch two-stage bootloader using BIOS INT 13h (LBA)
- A build-time kernel integrity check that a bootloader actually
  enforces before jumping to the kernel (tested against both a valid
  and a deliberately corrupted kernel image, see docs)
- A simulated BB84 QKD protocol in Qiskit, wired into a real build
  pipeline in a way that is honest about being a build-time, offline
  step rather than a fictional "quantum boot" feature
- A tiny freestanding C kernel with its own VGA driver, linked to a
  fixed physical address and entered from an asm stub

## What this project deliberately does *not* implement

No paging, no interrupt/IDT handling, no scheduler, no filesystem, no
drivers beyond VGA text mode, no user mode. This is scoped as a
boot-chain + integrity-verification project, not a full OS.
