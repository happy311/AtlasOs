#!/usr/bin/env python3
"""
=================================================================
 quantum_key_gen.py  -  Quantum Security Module (Research Piece)
=================================================================

This is the "Quantum Security Module (Qiskit Research Component)"
shown in the project diagram, sitting alongside the boot chain.

WHAT THIS IS:
    A simulation of the BB84 Quantum Key Distribution (QKD)
    protocol, run using Qiskit's Aer simulator. It produces a
    "quantum-derived" bitstream that is used, at BUILD time, to
    seed the checksum algorithm the bootloader uses to verify
    kernel integrity (see tools/gen_checksum.py and
    boot/stage2.asm : verify_checksum).

WHAT THIS IS NOT:
    This does NOT run on real quantum hardware, and it does NOT
    run during the actual boot sequence. Commodity x86 firmware
    cannot execute Python/Qiskit, and no real QKD hardware link
    exists on a normal PC. Framing this as happening "at boot" or
    "on real quantum hardware" would be inaccurate.

    This script is an offline, one-time, build-machine step. Its
    output is a 32-bit constant baked into the bootloader source
    before assembly. This is a deliberately honest scope for a
    portfolio project: it demonstrates understanding of QKD
    concepts and Qiskit, wired into a real system in a way that
    is technically defensible, rather than pretending quantum
    hardware is doing something it cannot currently do on a PC.

HOW BB84 IS SIMULATED HERE (simplified, no eavesdropper/Eve model,
no error-rate estimation - see docs/DOCUMENTATION.md for the full
explanation and its limitations):
    1. Alice picks random bits and random encoding bases (Z or X).
    2. Alice "sends" each bit as a qubit prepared in the chosen
       basis (a Qiskit circuit: X gate for bit=1, H gate if the
       basis is X).
    3. Bob picks his own random measurement bases and measures.
    4. Alice and Bob keep only the bits where their bases matched
       (the "sifted key") - this is the core BB84 idea.
"""

import os
import random
import struct

from qiskit import QuantumCircuit
from qiskit_aer import AerSimulator

NUM_BITS = 256          # raw qubits exchanged before sifting
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "build")


def bb84_generate_key(num_bits: int = NUM_BITS) -> list[int]:
    """Simulate one round of BB84 and return the sifted key bits."""
    simulator = AerSimulator()

    alice_bits = [random.randint(0, 1) for _ in range(num_bits)]
    alice_bases = [random.randint(0, 1) for _ in range(num_bits)]  # 0=Z, 1=X
    bob_bases = [random.randint(0, 1) for _ in range(num_bits)]

    bob_results = []
    for i in range(num_bits):
        qc = QuantumCircuit(1, 1)

        # Alice prepares the qubit according to her bit and basis
        if alice_bits[i] == 1:
            qc.x(0)
        if alice_bases[i] == 1:
            qc.h(0)

        # Bob measures in his own chosen basis
        if bob_bases[i] == 1:
            qc.h(0)
        qc.measure(0, 0)

        result = simulator.run(qc, shots=1, memory=True).result()
        bob_results.append(int(result.get_memory()[0]))

    # Sifting: keep only positions where Alice's and Bob's bases agree.
    sifted_key = [
        alice_bits[i] for i in range(num_bits) if alice_bases[i] == bob_bases[i]
    ]
    return sifted_key


def bits_to_bytes(bits: list[int]) -> bytes:
    usable_len = (len(bits) // 8) * 8
    bits = bits[:usable_len]
    out = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for b in bits[i : i + 8]:
            byte = (byte << 1) | b
        out.append(byte)
    return bytes(out)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    key_bits = bb84_generate_key()
    print(f"[quantum_key_gen] Raw qubits exchanged : {NUM_BITS}")
    print(f"[quantum_key_gen] Sifted key length     : {len(key_bits)} bits")

    key_bytes = bits_to_bytes(key_bits)
    if len(key_bytes) < 4:
        raise RuntimeError(
            "Sifted key too short (basis mismatch ran unlucky) - re-run this script."
        )

    key_path = os.path.join(OUTPUT_DIR, "quantum_key.bin")
    with open(key_path, "wb") as f:
        f.write(key_bytes)

    seed = struct.unpack_from("<I", key_bytes, 0)[0]
    seed_path = os.path.join(OUTPUT_DIR, "seed.txt")
    with open(seed_path, "w") as f:
        f.write(f"0x{seed:08X}")

    print(f"[quantum_key_gen] Full quantum key       : {len(key_bytes)} bytes -> {key_path}")
    print(f"[quantum_key_gen] Derived 32-bit seed     : 0x{seed:08X} -> {seed_path}")


if __name__ == "__main__":
    main()
