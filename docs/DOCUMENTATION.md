# AtlasOS - Technical Documentation

This document exists for two reasons: to record *why* things are
built the way they are, and to give you a script for explaining
this project in an interview. Each section below follows the same
shape: **what it does**, **why it's built that way**, **how to
walk through the code**, and **questions this invites**.

---

## 1. Big picture

```
BIOS (real mode, 16-bit)
   |
   v
boot1.asm  - MBR, 512 bytes, loads Stage 2 via INT 13h
   |
   v
stage2.asm - loads the kernel + checksum, verifies kernel
             integrity, enables A20, loads the GDT, switches to
             32-bit protected mode, jumps to the kernel
   |
   v
entry.asm  - sets up a stack, calls kernel_main()
   |
   v
kernel.c   - brings up every subsystem in dependency order, then
             hands off to the shell
```

Everything runs in a single privilege ring (ring 0), with a flat,
identity-mapped address space. There's exactly one "process" - the
kernel itself - and the shell is just its main loop, not a
separate task. That's a deliberate simplification: it lets every
other piece (interrupts, drivers, the allocator, paging) be
implemented and explained on its own, without also having to
explain a scheduler or address-space isolation at the same time.

### Physical memory map

| Address        | What lives there                                  |
|-----------------|-----------------------------------------------------|
| `0x00000` - `0x003FF` | Real-mode IVT (unused once we're in protected mode) |
| `0x07C00`         | Stage 1 (MBR), loaded here by the BIOS             |
| `0x08000`         | Stage 2, loaded here by Stage 1                    |
| `0x09000`         | The 4-byte expected checksum, loaded by Stage 2    |
| `0x10000`         | The kernel, loaded by Stage 2 and where it's linked |
| `0x90000`         | Stack pointer set just before the jump to the kernel; the kernel then switches to its own 16 KB `.bss` stack (`entry.asm`) |
| `0x400000` (4 MB) | Start of the `kmalloc` heap arena (1 MB)           |
| `0xB8000`          | VGA text-mode video memory                         |

---

## 2. Boot chain (`boot/boot1.asm`, `boot/stage2.asm`, `boot/gdt.inc`)

**What it does.** The BIOS loads whatever is in the disk's first
512 bytes to `0x7C00` and jumps to it - that's Stage 1. Stage 1
does the absolute minimum: set up a stack, use the INT 13h
extended-read BIOS call to load Stage 2 (8 sectors) to `0x8000`,
and jump there. Stage 2 does the real work: it loads the kernel
image and a pre-computed checksum from disk, recomputes the
checksum over the loaded kernel and compares it, enables the A20
line (so 32-bit code can address memory past the first 1 MB),
loads a GDT, sets `CR0.PE` to enter protected mode, and far-jumps
into 32-bit code that sets up the kernel's segment registers and
jumps to `0x10000`.

**Why two stages.** A boot sector is exactly 512 bytes, minus the
16-byte partition-table-adjacent boot signature - there is nowhere
near enough room in Stage 1 to also hold protected-mode setup and
a checksum verifier. Splitting into a tiny, fixed-size Stage 1 and
a larger Stage 2 (whose size is only bounded by how many sectors
you tell Stage 1 to load) is the standard way around that.

**Why a checksum instead of nothing.** It's a cheap way for the
bootloader to refuse to run a kernel image that's been truncated
or corrupted, rather than jumping into garbage and getting an
undebuggable triple fault. The algorithm (`tools/gen_checksum.py`,
mirrored in `verify_checksum` in `stage2.asm`) is a rotate/XOR
checksum, seeded by a random value picked at build time
(`tools/gen_seed.py`). **Be upfront about what this is not**: it's
not a cryptographic hash and it's not a signature. Anyone who can
rewrite the disk image can recompute a matching checksum for their
own payload - this defends against accidental corruption, not a
deliberate attacker. A real secure-boot chain would sign the image
with an asymmetric key and verify the signature, which is out of
scope here.

**Interview questions this invites:** *"Walk me through what
happens between power-on and your kernel running."* *"Why do you
need to enable A20?"* (real-mode address wraparound at the 1 MB
boundary - some old software depended on it, so it ships disabled
by default even on modern hardware). *"What's actually in a GDT
entry and why do you need one before enabling protected mode?"*
(`boot/gdt.inc` - segment descriptors are mandatory in protected
mode even for a flat memory model; there's no way to have "no
segmentation" on x86, only base=0/limit=4GB segments that behave
like there isn't one).

---

## 3. Interrupts: IDT, ISR stubs, PIC (`kernel/idt.c/h`, `kernel/isr.asm/c/h`, `kernel/pic.c/h`)

**What it does.** The IDT (`idt.c`) is a 256-entry table the CPU
consults on every interrupt/exception; each entry points at a
handler. `isr.asm` generates one small assembly stub per vector
(NASM macros, not 48 hand-written trampolines) that normalizes the
stack - pushing a dummy error code for the vectors that don't get
one from the CPU - then jumps into a shared "common stub" that
saves every general-purpose register, calls into C, restores them,
and `iret`s back. `isr.c` is where CPU exceptions (vectors 0-31)
and hardware IRQs (remapped to vectors 32-47) actually get
dispatched: exceptions print what happened and halt (except `int3`,
used by the shell's `trap` command as a safe, resumable demo);
IRQs look up whichever driver registered itself for that line and
call it. `pic.c` remaps the two legacy 8259 PICs so hardware IRQs
land on 32-47 instead of their BIOS default of 8-15, which would
otherwise collide with CPU exceptions like `#DF` (8) and `#GP`
(13).

**Why the asm/C split.** The CPU jumps to interrupt handlers with
no calling convention - no argument register, no promise that any
C-visible state survives. The stub's whole job is to capture that
raw CPU state into something a C function can safely receive as a
struct (`registers_t` in `isr.h`), and to restore it exactly before
`iret`. This boundary - "the CPU hands you a mess, and asm's job is
to turn it into something you can reason about" - is worth being
able to draw on a whiteboard.

**Why remap the PIC at all.** Without remapping, an unmasked
hardware IRQ vectors straight into whatever CPU exception occupies
that slot by BIOS default. That's the single most common reason a
hobby kernel triple-faults the instant it enables interrupts, and
it's worth knowing *why* rather than just knowing "you have to call
pic_remap first."

**Interview questions this invites:** *"What's the difference
between an interrupt gate and a trap gate?"* (interrupt gates clear
`IF` on entry - we use these, and re-enable interrupts explicitly
with `STI` in the common stub, right before `IRET`, once it's safe
to do so). *"Why does your `registers_t` struct not have `esp` and
`ss` in it?"* (we never change privilege level, so the CPU doesn't
push them - see the comment at the top of `isr.h`). *"What happens
if two IRQs fire while you're already in a handler?"* (the second
one waits - our stub clears `IF` on entry and doesn't re-enable it
until right before `iret`, so we don't support nested interrupts).

---

## 4. PIT timer driver (`kernel/timer.c/h`)

**What it does.** Programs PIT (Programmable Interval Timer)
channel 0 to fire IRQ0 at a chosen frequency (100 Hz by default),
by writing a 16-bit divisor derived from the PIT's fixed 1.193182
MHz input clock. The handler just increments a tick counter;
everything else (`timer_get_ticks`, `timer_sleep`) is built on top
of that one counter.

**Why `hlt` instead of a busy loop.** `timer_sleep` doesn't spin -
it executes `sti; hlt` in a loop, which parks the CPU until the
*next* interrupt of any kind, then re-checks the tick count. On
real hardware this is the difference between a core pegged at
100% and one sitting idle until there's actual work; it's the same
principle behind `keyboard_getchar`'s blocking read.

**Interview questions this invites:** *"How would you build a
scheduler on top of what you have?"* (IRQ0 is exactly where a
preemptive scheduler's timer tick would live - you'd save the
interrupted task's context in the handler instead of just
incrementing a counter, and it's the same `registers_t` you already
have). *"Why 100 Hz and not 1000?"* (tradeoff between timer
resolution and IRQ overhead - fine to reason about out loud, there's
no single right answer).

---

## 5. PS/2 keyboard driver (`kernel/keyboard.c/h`)

**What it does.** IRQ1 fires on every key make/break event. The
handler reads a raw scancode from port `0x60`, tracks shift
key state (make/break codes `0x2A`/`0x36` and their `+0x80`
counterparts), translates the scancode to ASCII via a lookup table
for scancode set 1, and pushes the result into a small ring buffer.
`keyboard_getchar()` blocks (via `hlt`, not spinning) until that
buffer has something in it.

**Why a ring buffer instead of handing characters straight to the
shell.** The interrupt handler and the shell's `read_line` run at
different times and different "speeds" - a burst of fast typing
(or a pasted string, in an emulator) can generate several
keystrokes before the shell asks for the next one. The ring buffer
decouples "when a key was pressed" from "when the shell got around
to reading it," which is the same producer/consumer problem every
real OS input path solves the same way.

**What's intentionally missing.** No extended (`0xE0`-prefixed)
scancodes (arrow keys, etc.), no caps lock, no key-repeat handling
beyond what the hardware itself does. None of these are difficult -
they just aren't needed to drive a line-oriented shell, and adding
them would pad the driver without adding a new concept.

**Interview questions this invites:** *"Why do you check for the
break bit before shift, instead of after?"* (walk through
`keyboard_callback` - shift's own break code needs to be handled as
shift *specifically*, before the generic "ignore all breaks" check
would otherwise swallow it). *"What happens if the ring buffer
fills up?"* (`buffer_push` drops the keystroke rather than
overwriting an unread one or blocking inside an interrupt handler -
blocking in an ISR is never correct).

---

## 6. Serial (COM1) driver (`kernel/serial.c/h`)

**What it does.** A polled (not interrupt-driven) 16550 UART
driver: `serial_init` programs the baud rate divisor and line
control registers, `serial_write_char`/`serial_read_char` spin on
the line-status register's transmit-empty / data-ready bits.

**Why it's polled, not interrupt-driven, when the timer and
keyboard aren't.** It's an intentional contrast worth pointing out
in an interview: not every driver needs an IRQ. Polling is simpler
and perfectly fine when nothing else needs to run concurrently with
"wait for a byte" - which is true here, since the shell's `serial`
command is the only caller. Interrupt-driven I/O earns its
complexity when the CPU has other useful work to do while waiting;
demonstrating you know *when not to* reach for interrupts is as
useful a signal as demonstrating you can wire one up.

**Interview questions this invites:** *"Why 38400 baud?"* (an
arbitrary but conservative choice for compatibility - the divisor
math is in `serial_init`'s comments, and it's easy to change to
justify any other rate). *"What would you need to add to make this
interrupt-driven instead?"* (an IRQ4 handler like `keyboard.c`'s,
plus a transmit/receive ring buffer, plus enabling the UART's
interrupt-enable register instead of leaving it at `0x00`).

---

## 7. Heap allocator (`kernel/kmalloc.c/h`)

**What it does.** A first-fit allocator over a fixed 1 MB arena
starting at physical `0x400000`. Each block has a small header
(`size`, `free`, `next`) immediately before the memory handed back
to the caller. `kmalloc` walks the linked list looking for the
first free block big enough, splitting off the remainder as a new
free block if there's enough left over to be worth it. `kfree`
marks a block free and does a single pass merging any run of
adjacent free blocks back together.

**Why the heap isn't a static C array.** It would work, but it
would also bloat `kernel.bin`: `objcopy -O binary` has to emit
zero-filled bytes for every address in a `.bss` array's range
before the final flat binary is produced, and disk sectors 10+ (the
kernel's on-disk footprint) would grow to match. Instead,
`kmalloc.c` just treats a raw physical address well above the
kernel and its stack as "memory it owns" - that memory already
exists (QEMU's default RAM is 128 MB), it just isn't part of the
kernel image. `paging.c`'s identity map makes this address behave
identically whether paging is on or off.

**Why first-fit, not something fancier.** Best-fit or a buddy
allocator would reduce fragmentation better, but first-fit is the
smallest design that still makes every core allocator concept -
headers, splitting, coalescing, external fragmentation - visible
and explainable, rather than hidden behind a more "production"
design's extra bookkeeping. `MIN_SPLIT_REMAINDER` in `kmalloc.c` is
exactly the kind of tuning knob that's easy to talk through: split
too eagerly and you thrash on tiny slivers, split too conservatively
and you waste memory.

**Interview questions this invites:** *"What happens if you call
`kfree` on a pointer twice?"* (undefined - there's no guard against
a double-free here, same as most textbook allocators; a real one
would need a magic number/canary in the header to detect it).
*"How would you make this thread-safe?"* (there's no locking - fine
today because we're single-threaded, but the first thing to break
the moment a second execution context exists).

---

## 8. Paging (`kernel/paging.c/h`)

**What it does.** Builds a single, statically-allocated 1024-entry
page directory and identity-maps the entire 4 GB address space
using 4 MB pages (the `PSE`/Page Size Extension feature - `CR4`
bit 4), then loads `CR3` with the directory's physical address and
sets `CR0.PG` to turn paging on.

**Why 4 MB pages instead of the usual 4 KB + page tables.** The
textbook page-table walk (a page directory *and* a set of page
tables, 4 KB pages) is the "real" design, but it needs the
allocator (or a dedicated static pool) to actually build all those
page tables. Using PSE's 4 MB pages needs exactly one 4 KB-aligned
array and nothing else, which is enough to demonstrate the actual
mechanism - `CR4.PSE`, `CR3`, `CR0.PG`, and what a page-directory
entry's flag bits mean - without also having to stand up a page
table allocator first. Being able to say *"I know this
demonstrates paging without being a memory-protection system yet,
and here's specifically what's missing"* is a stronger interview
answer than quietly hoping nobody asks.

**What this does *not* give you**, and would need a real per-4KB
page-table design to add: memory protection (every page is
present+writable, so there's no fault on a bad access), no
non-identity virtual addresses (so no per-process address spaces),
no demand paging / swapping.

**Interview questions this invites:** *"Walk me through what each
bit in a page-directory entry means."* (`paging.c`: bit 0 present,
bit 1 writable, bit 7 page-size - the Intel SDM has the full 32-bit
layout, worth having glanced at once). *"Why does the page
directory need to be 4 KB aligned?"* (the CPU reuses the low 12
bits of `CR3` for flags, ignoring whatever's actually there - an
unaligned directory would silently corrupt those bits).

---

## 9. Shell (`kernel/shell.c/h`)

**What it does.** A line-oriented command loop: read a line via
`keyboard_getchar` with backspace handling and VGA echo, split it
into a command word and the rest of the line, dispatch to a
`strcmp` chain. Commands: `help`, `clear`, `echo`, `mem` (heap
stats), `uptime` (from the PIT tick counter), `trap` (raises
`int3` to demo the IDT/ISR path safely), `serial` (writes to
COM1), `color`, `reboot` (the standard 8042 keyboard-controller
reset-line pulse).

**Why this is the right "top" of the project.** Every other
subsystem in this kernel is proven by whether the shell can
actually use it: typing at all proves the keyboard driver and IRQ
dispatch, `uptime` proves the timer, `mem` proves the allocator,
`trap` proves the IDT, `reboot` proves you can still talk to
hardware after everything else is running. It's also the natural
place to demonstrate the project live in an interview instead of
just describing it.

**Interview questions this invites:** *"What would `cd`/a real
filesystem-backed shell need that this doesn't have?"* (a
filesystem driver and a notion of a current working directory or
process - neither exists here, on purpose). *"Why `strcmp` instead
of a table of function pointers?"* (a table would scale better past
a handful of commands - `dispatch` in `shell.c` is the obvious spot
to swap it in, and saying so shows you know the tradeoff exists
even where the current code doesn't need it yet).

---

## 10. Build system (`Makefile`, `tools/gen_seed.py`, `tools/gen_checksum.py`)

The kernel is built as a set of freestanding `.o` files (`-m32
-ffreestanding -nostdlib`, no libc - `kernel/string.c` exists
because there is no `<string.h>` implementation to link against),
linked with `kernel/linker.ld` at a fixed load address (`0x10000`),
then `objcopy -O binary`'d into a flat binary with no ELF headers -
the bootloader has no ELF loader, so this has to be something it
can just read into memory and jump straight to. `tools/gen_seed.py`
picks a random build-time seed; `tools/gen_checksum.py` computes
the expected checksum against the freshly-built kernel binary and
emits both the binary checksum (baked onto the disk image) and a
NASM `.inc` file the bootloader assembles against. `Makefile`'s
comment block over the disk-image target documents the exact
sector layout, which is worth keeping in sync any time a stage's
size changes.

An earlier version of this project generated the build-time seed
via a Qiskit simulation of the BB84 quantum key distribution
protocol instead of `os.urandom()`. That was a fun thing to wire
up, but it added a heavyweight dependency for a spot in the
pipeline - "produce one unpredictable 32-bit value, once, on the
build machine" - that doesn't need or benefit from it, and it
invited the project to be read as claiming more than it did. This
version is scoped to match what it actually is: a bootloader +
freestanding kernel project with a straightforward integrity check,
not a cryptography or quantum-computing project.

---

## 11. Testing

`make run-debug` writes `build/debug.log`, capturing everything
written to QEMU's debug-console port `0xE9` - every boot-stage
progress message (`boot1.asm`/`stage2.asm`) plus the kernel's own
`debug_str` calls in `kernel.c`, in order, with no VGA/keyboard
needed. This is what makes the boot chain scriptable/headless:
build, boot with `-display none`, and grep the log for the
expected sequence of `[boot1]` / `[stage2]` / `[kernel]` lines,
which is exactly how this project was verified end-to-end
(including a temporary self-test that exercised `timer_sleep` and
`kmalloc`/`kfree` from `kernel_main` before the shell takes over,
confirming IRQ0 delivery and the allocator both work under QEMU).

---

## 12. What's deliberately out of scope, and what each would take

- **Scheduler / multitasking.** IRQ0 (`timer.c`) is exactly where a
  preemptive scheduler's tick would go; you'd need a task struct, a
  ready queue, and to save/restore full context (build on
  `registers_t`) instead of just counting ticks.
- **Filesystem.** Would need a disk driver beyond the boot-time INT
  13h reads (a real driver, e.g. ATA PIO), plus a filesystem format
  to parse - FAT is the usual first choice for how well-documented
  it is.
- **User mode / syscalls.** Needs TSS setup, ring 3 segments in the
  GDT, and an `int 0x80`-style (or `sysenter`) syscall entry point
  that validates arguments coming from a less-trusted context.
- **Real virtual memory.** Needs per-process page directories (4 KB
  pages + page tables, not the PSE identity map here), a page-fault
  handler that can actually do something useful (demand paging,
  copy-on-write), and non-identity address spaces.
- **A real integrity/secure-boot chain.** Needs asymmetric signing
  at build time and signature verification (not just a checksum) at
  boot time - see the caveat in section 2.

None of these are hard to *reason about* given what's already
here - the point of this section is to have a ready answer for
"what would you add next," not to pretend the list doesn't exist.
