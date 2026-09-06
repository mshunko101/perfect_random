#!/usr/bin/env python3
"""NIST SP 800-22 statistical tests for Rand++ generator."""
import sp80022suite
import sys
import os

def main():
    test_file = sys.argv[1] if len(sys.argv) > 1 else "test_data.bin"

    if not os.path.exists(test_file):
        print(f"ERROR: File not found: {test_file}")
        sys.exit(2)

    with open(test_file, "rb") as f:
        data = f.read()

    print(f"Read {len(data)} bytes from {test_file}")

    # Convert to bit sequence as bytes (each byte is 0 or 1)
    bits = bytes([(byte >> (7 - bit_pos)) & 1 for byte in data for bit_pos in range(8)])
    bits = bits[:2000000]  # 2 million bits for better coverage
    print(f"Extracted {len(bits)} bits\n")

    results = []

    # ── 1-arg tests: fn(bits) ──
    one_arg_tests = [
        ("Frequency",              sp80022suite.frequency),
        ("Runs",                   sp80022suite.runs),
        ("Longest Run of Ones",    sp80022suite.longest_run_of_ones),
        ("Rank",                   sp80022suite.rank),
        ("Universal",              sp80022suite.universal),
        ("Cumulative Sums",        sp80022suite.cumulative_sums),
        ("Discrete Fourier",       sp80022suite.discrete_fourier_transform),
        ("Random Excursions",      sp80022suite.random_excursions),
        ("Random Excursions Var",  sp80022suite.random_excursions_variant),
    ]

    for name, fn in one_arg_tests:
        try:
            p = fn(bits)
            if isinstance(p, (tuple, list)):
                p = p[0]
            p = float(p)
            status = "PASS" if p >= 0.01 else "FAIL"
            print(f"  {name:30s}  p={p:.6f}  {status}")
            results.append((name, p, status))
        except Exception as e:
            print(f"  {name:30s}  ERROR: {e}")
            results.append((name, None, "ERROR"))

    # ── 2-arg tests: fn(int_param, bits) — parameter FIRST, data SECOND ──
    two_arg_tests = [
        ("Block Frequency",          sp80022suite.block_frequency,                    128),
        ("Non-Overlapping Template",  sp80022suite.non_overlapping_template_matchings, 9),
        ("Overlapping Template",     sp80022suite.overlapping_template_matchings,    9),
        ("Linear Complexity",         sp80022suite.linear_complexity,                  500),
        ("Serial",                    sp80022suite.serial,                              16),
        ("Approximate Entropy",       sp80022suite.approximate_entropy,                10),
    ]

    for name, fn, param in two_arg_tests:
        try:
            p = fn(param, bits)
            if isinstance(p, (tuple, list)):
                p = p[0]
            p = float(p)
            status = "PASS" if p >= 0.01 else "FAIL"
            print(f"  {name:30s}  p={p:.6f}  {status}")
            results.append((name, p, status))
        except Exception as e:
            print(f"  {name:30s}  ERROR: {e}")
            results.append((name, None, "ERROR"))

    # ── Summary ──
    print()
    failed = [r for r in results if r[2] != "PASS"]
    passed = [r for r in results if r[2] == "PASS"]
    errored = [r for r in results if r[2] == "ERROR"]

    print(f"Passed: {len(passed)} / {len(results)}")
    if errored:
        print(f"Errors: {len(errored)}")
    if failed:
        print(f"Failed: {len(failed)}")
        for name, p, status in failed:
            print(f"  {name}: p={p:.6f} ({status})")
        sys.exit(1)
    elif errored:
        print("Some tests had errors (see above)")
        sys.exit(1)
    else:
        print("All tests PASSED")


if __name__ == "__main__":
    main()
