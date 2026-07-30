#!/usr/bin/env python3
"""
=================================================================
 gen_seed.py  -  Build-time seed for the kernel integrity checksum
=================================================================
Produces a random 32-bit value, at build time, that seeds the
rotate/XOR checksum in gen_checksum.py (and is re-derived by
stage2.asm at boot time to verify the loaded kernel image hasn't
been corrupted or swapped - see boot/stage2.asm : verify_checksum).

There's nothing exotic here on purpose: os.urandom() is a properly
seeded CSPRNG, and a build-time integrity seed doesn't need to be
anything fancier than "unpredictable and different per build". An
earlier version of this project generated the seed via a simulated
BB84 quantum key distribution protocol (Qiskit) instead - which was
a fun thing to wire up, but added a heavyweight dependency and a
"quantum" framing for a spot in the pipeline that doesn't actually
need or benefit from it. This project is scoped as a bootloader +
kernel project, so the seed generation is scoped to match.

Output:
    build/seed.txt   - the seed, as a hex string (e.g. "0xDEADBEEF")
"""

import os
import struct


OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "build")


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    seed_bytes = os.urandom(4)
    seed = struct.unpack("<I", seed_bytes)[0]

    seed_path = os.path.join(OUTPUT_DIR, "seed.txt")
    with open(seed_path, "w") as f:
        f.write(f"0x{seed:08X}")

    print(f"[gen_seed] Derived 32-bit seed : 0x{seed:08X} -> {seed_path}")


if __name__ == "__main__":
    main()
