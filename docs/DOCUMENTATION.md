# Quantum-Assisted Secure Bootloader — Full Documentation

## 1. What this project is

A hand-written x86 boot chain built in assembly and C, from BIOS
hand-off all the way to a running 32-bit protected-mode kernel,
with a kernel-integrity check that is seeded by a Qiskit simulation
of the BB84 Quantum Key Distribution (QKD) protocol.

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

It was scoped deliberately: it implements the handful of things
that make a boot chain "real" (BIOS disk I/O, real-mode-to-protected-mode
transition, a genuine pass/fail integrity gate) and does not attempt
to implement an entire operating system. Everything it leaves out
is listed explicitly in Section 6, so nobody — including you, in an
interview — is caught by surprise by what's *not* here.

Every claim in this document was verified by actually building and
booting the project in QEMU during development, including the
integrity check's failure path (Section 5.4).

---

## 2. Directory structure

```
quantum-secure-bootloader/
├── Makefile                  Orchestrates the entire build
├── README.md                 Quick start
├── boot/
│   ├── boot1.asm              Stage 1: MBR, loads Stage 2
│   ├── stage2.asm             Stage 2: loads kernel, verifies it,
│   │                           enters protected mode
│   └── gdt.inc                Global Descriptor Table (flat model)
├── kernel/
│   ├── entry.asm               32-bit entry stub, sets up stack, calls C
│   ├── kernel.c                 Kernel's main logic
│   ├── vga.c / vga.h             VGA text-mode driver (separate from
│   │                              kernel logic on purpose)
│   └── linker.ld                Links the kernel to run at 0x10000
├── tools/
│   ├── quantum_key_gen.py       Quantum Security Module (Qiskit / BB84)
│   ├── gen_checksum.py           Build-time integrity checksum generator
│   └── requirements.txt
├── docs/
│   └── DOCUMENTATION.md         This file
└── build/                     Created by `make`; not checked in
```

Nothing is dumped into a single file. The bootloader is two files
because two genuinely different execution contexts exist (16-bit
real mode with a small BIOS-loaded footprint vs. the "already
loaded, ready to switch modes" stage). The kernel splits hardware
access (`vga.c`) from logic (`kernel.c`) so each file has one job.

---

## 3. The boot chain, stage by stage

### 3.1 BIOS → `boot1.asm` (Stage 1 / MBR)

The BIOS loads sector 0 of the boot disk to physical address
`0x7C00` and jumps to it in 16-bit real mode — this part is fixed
by the PC platform, not something this project controls.

`boot1.asm`:

1. Zeroes segment registers, sets up a stack at `0x7C00`.
2. Saves the boot drive number (BIOS passes it in `DL`).
3. Uses **INT 13h, AH=42h** (BIOS "extended read", LBA-addressed —
   the modern replacement for the older CHS-addressed `AH=02h`) to
   read 8 sectors (Stage 2) from disk into memory at `0x0000:0x8000`.
4. Jumps to `0x0000:0x8000`.

It must be exactly 512 bytes and end in the signature `0x55AA`, or
BIOS will refuse to treat it as bootable — `boot1.asm` pads itself
to this size and appends the signature explicitly.

### 3.2 `stage2.asm` (Stage 2)

Now running at `0x8000`, still in real mode, with more room to work
with than Stage 1's 512-byte budget:

1. Loads `checksum.bin` (the expected integrity value, 1 sector)
   to `0x9000`.
2. Loads `kernel.bin` (up to 64 sectors / 32 KB) to physical
   `0x10000`.
3. **Recomputes the checksum over the freshly loaded kernel image**
   and compares it to the value loaded from `checksum.bin`. This is
   the "Kernel Integrity Verification" step in the project diagram —
   see Section 5 for exactly how it works and what it is/isn't
   protecting against.
4. If the checksums don't match: prints an error and halts. The
   kernel is never entered.
5. If they match: enables the A20 line (needed to address memory
   above 1 MB reliably), loads the GDT (`gdt.inc`), sets `CR0.PE`
   to enable protected mode, and does a **far jump** to a 32-bit
   code segment — the far jump is required here to flush the CPU's
   prefetch queue, which may still contain 16-bit-decoded
   instructions.
6. In 32-bit code: sets up the data/stack segment registers and
   jumps directly to `0x10000` — the kernel's entry point.

### 3.3 Protected mode

"Protected mode" here means: 32-bit addressing, a flat memory model
(one code segment and one data segment, each covering the full 4 GB
address space via the GDT in `gdt.inc`), and no more reliance on
BIOS interrupts. No paging is enabled — that's a deliberate scope
cut (Section 6).

### 3.4 The kernel

`entry.asm` sets up a dedicated 16 KB stack and calls `kernel_main()`
in `kernel.c`. The kernel:

- Clears the screen and prints a banner directly to VGA text video
  memory (`0xB8000`) via `vga.c`.
- Halts the CPU in a loop (`hlt` in a tight loop) rather than
  running off into undefined memory once there's nothing left to do.

It is intentionally *not* a general-purpose kernel — see Section 6.

---

## 4. Build pipeline

`make` runs these steps in dependency order:

1. **Compile the kernel.** `entry.asm` → ELF object; `kernel.c` and
   `vga.c` → ELF objects (`-m32 -ffreestanding -nostdlib`, no libc,
   no host OS assumptions); linked with `linker.ld` (places the
   kernel at `0x10000`); `objcopy -O binary` strips the ELF
   container down to a raw flat binary the bootloader can load
   directly.
2. **Run the Quantum Security Module** (`quantum_key_gen.py`) to
   produce a quantum-derived 32-bit seed. See Section 5.
3. **Generate the integrity checksum** (`gen_checksum.py`), seeded
   with the value from step 2, computed over the kernel binary from
   step 1. Emits `checksum.bin` (what Stage 2 compares against) and
   `stage2_seed.inc` (a NASM include defining `SEED_CONST`, so Stage
   2 recomputes the *same* checksum using the *same* seed at boot).
4. **Assemble Stage 2**, which `%include`s `stage2_seed.inc` — this
   is why Stage 2 must be assembled *after* step 3, not before.
5. **Assemble Stage 1.**
6. **Assemble the disk image** with `dd`, at these fixed offsets:

   | Sector(s) | Contents           | Size          |
   |-----------|--------------------|---------------|
   | 0         | `boot1.bin` (MBR)  | 512 B         |
   | 1–8       | `stage2.bin`       | 4096 B        |
   | 9         | `checksum.bin`     | 4 B (of 512 B)|
   | 10–73     | `kernel.bin`       | up to 32 KB   |

`make run` boots the resulting image in QEMU. `make run-debug` also
enables QEMU's debug-console port (`0xE9`) so the boot chain's
progress can be captured to a log file — see Section 5.4 for why
this mattered during development.

---

## 5. The Quantum Security Module — what it is and isn't

This is the part of the project most likely to raise an eyebrow, so
it gets the most detailed and the most honest treatment. **Read this
section before describing the project to anyone technical.**

### 5.1 What it actually does

`tools/quantum_key_gen.py` simulates one round of the BB84 QKD
protocol using Qiskit's `AerSimulator`:

1. "Alice" picks random bits and random encoding bases (rectilinear
   or diagonal).
2. Each bit is prepared as a single-qubit circuit (an `X` gate for a
   `1` bit, an `H` gate if the diagonal basis is used).
3. "Bob" picks his own random measurement bases and measures.
4. Bits are kept only where Alice's and Bob's bases happened to
   match — this basis-sifting step is the actual core idea of BB84,
   and it's genuinely implemented, not faked.
5. The sifted bits are packed into bytes; the first 4 bytes become a
   32-bit seed value.

This is a real quantum-circuit simulation producing a genuinely
random (simulator-random) bitstream via actual gate operations —
not a hardcoded constant dressed up to look quantum.

### 5.2 What it deliberately does NOT do

- **It does not run on real quantum hardware.** It runs on Qiskit's
  classical simulator. No quantum computer or QKD hardware link is
  involved.
- **It does not run during boot.** It runs once, offline, on the
  developer's build machine, before assembly. Commodity PC firmware
  cannot execute Python, and there is no quantum link between "Alice"
  and "Bob" here — both are simulated in the same process. Describing
  this as "quantum security at boot time" would be wrong, and the
  code comments in `quantum_key_gen.py` say so explicitly.
- **It does not model an eavesdropper or error-rate estimation** —
  real BB84 protocols include a step where Alice and Bob compare a
  subset of their sifted key over a public channel to detect
  eavesdropping (via the disturbance quantum measurement causes).
  That step is not implemented here; this is a simplified
  demonstration of the sifting mechanism, not a complete QKD
  security proof.
- **It is not a cryptographic guarantee for the bootloader.** The
  checksum algorithm the seed feeds into (Section 5.3) is a rotate/XOR
  checksum, not a cryptographic hash or signature. It will reliably
  catch accidental corruption, a truncated write, or a swapped
  kernel file — the kind of failure a real integrity check is meant
  to catch in a project like this. It will **not** stop a
  sophisticated attacker who controls the disk image, since they
  could recompute a matching checksum for a modified kernel using
  the same public algorithm. A production-grade version of this idea
  would sign the kernel with a private key (e.g. Ed25519) and verify
  the signature with a public key baked into the bootloader —
  intentionally out of scope here to keep the project buildable and
  auditable in a reasonable amount of code.

### 5.3 Why it's wired in this way

The honest way to connect "quantum" and "bootloader" without
overstating either one is: use the quantum simulation to produce a
seed value at build time, and have the bootloader's own integrity
check depend on that seed. This is genuinely wired end-to-end — change
the kernel by one byte, or run the checksum with the wrong seed, and
the bootloader's own recomputation (in `stage2.asm`, real x86
assembly, actually executed by the CPU at boot) will disagree with
the stored value and refuse to boot. That refusal is real and was
tested (Section 5.4) — it just isn't "quantum" in the sense of
happening on quantum hardware or at boot time.

If asked directly ("is this really quantum security, running on the
bootloader?") the accurate answer is: *the checksum enforcement is
real and happens at boot; the "quantum" part is a Qiskit simulation
that seeds it, run once at build time — a research/exploratory
component, not live quantum hardware in the boot path.*

### 5.4 Verification performed during development

Both the pass and fail paths of the integrity check were tested by
booting the built disk image in QEMU:

- **Valid kernel:** Stage 2 loads the kernel, recomputes the
  checksum, it matches, and the log shows
  `Integrity OK. Entering protected mode...` followed by
  `[kernel] kernel_main() reached.` — confirming control genuinely
  reached the compiled C kernel after a real mode switch.
- **Corrupted kernel:** a single byte was flipped inside the
  kernel's on-disk sectors (leaving `checksum.bin` untouched, as if
  the kernel had been tampered with or corrupted after the checksum
  was generated). Booting that image produced
  `INTEGRITY CHECK FAILED - kernel image rejected. Halting.` and the
  CPU never reached the kernel. This confirms the check is load-bearing,
  not decorative.

---

## 6. Explicit scope cuts

This project implements a boot chain and an integrity gate — not a
full operating system. Left out on purpose:

| Not implemented       | Why                                                            |
|------------------------|-----------------------------------------------------------------|
| Paging / virtual memory | Adds significant complexity for no benefit to a boot-chain demo |
| Interrupt handling (IDT) | Not needed for a kernel that just prints a banner and halts   |
| A scheduler / multitasking | Out of scope for "does the boot chain work" as a project goal |
| A filesystem            | The kernel is loaded as a raw flat binary at a fixed disk offset |
| Drivers beyond VGA text  | No keyboard, disk, or network drivers in the kernel itself     |
| User mode / syscalls    | No ring 3 code runs; everything here is ring 0                 |
| Real QKD hardware / live quantum boot-time checks | See Section 5.2 |
| Cryptographic signing of the kernel | Would need asymmetric crypto (e.g. Ed25519) — a natural "next step," noted but not built |

---

## 7. How to talk about this project (e.g. on a resume / in an interview)

Accurate framing that holds up under technical questioning:

> "Built a two-stage x86 bootloader in NASM assembly that transitions
> the CPU from 16-bit real mode to 32-bit protected mode (GDT, A20
> line, `CR0.PE`), loads a small freestanding C kernel, and enforces
> a kernel-integrity check before boot — tested against both valid
> and deliberately corrupted kernel images in QEMU. Paired it with a
> Qiskit simulation of the BB84 quantum key distribution protocol as
> a build-time research component that seeds the integrity
* checksum, with the scope and limitations of that simulation
  (offline, no eavesdropper model, not a cryptographic signature)
  explicitly documented."

This framing is defensible in an interview because every clause in
it is something you can point to in the code and something you
watched work (or fail correctly) in QEMU.

## 8. Extending this project

Natural next steps, roughly in order of effort:

1. Replace the rotate/XOR checksum with a real cryptographic hash
   (e.g. SHA-256, computed at build time and — since implementing
   SHA-256 in real-mode assembly is a project in itself — either
   embed a precomputed hash and do a simpler equality check, or move
   the check to a stage that can afford a heavier hash implementation).
2. Add Ed25519 (or similar) signing of the kernel, verifying the
   signature instead of a checksum, so a tampered kernel *cannot*
   be re-signed without the private key.
3. Add a serial port driver so kernel output doesn't depend on VGA
   text mode.
4. Implement an IDT and basic interrupt handling as a stepping stone
   toward a scheduler.
5. Research a genuine hybrid: use a real hardware QKD/entropy source
   (where available) to periodically refresh a *runtime*-verifiable
   key, rather than a build-time constant — this is a much larger
   undertaking and would need real quantum hardware access.
