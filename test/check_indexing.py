#!/usr/bin/env python3
"""Compare assembler output and measure scaling, using only Python's stdlib.

Build both revisions with the same compiler and flags, then run:
  python3 test/check_indexing.py --baseline /path/to/old/asm.elf --benchmark
Without --baseline, checks that the working examples assemble successfully.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile
from time import perf_counter


ROOT = Path(__file__).resolve().parents[1]


def assemble(executable, source, output):
    start = perf_counter()
    result = subprocess.run(
        [str(executable), str(source), "-i", str(ROOT / "stdlibs"), "-o", str(output)],
        capture_output=True, timeout=120,
    )
    elapsed = perf_counter() - start
    binary = output.read_bytes() if result.returncode == 0 else None
    return result.returncode, result.stdout, result.stderr, binary, elapsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assembler", type=Path, default=ROOT / "assembler/asm.elf")
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="as-macro-index-") as directory:
        temporary = Path(directory)

        def check(source, expect_success=True):
            current = assemble(args.assembler.resolve(), source, temporary / "current.o")
            if expect_success:
                assert current[0] == 0, current[2].decode(errors="replace")
            if args.baseline:
                baseline = assemble(args.baseline.resolve(), source, temporary / "baseline.o")
                assert baseline[:4] == current[:4], f"output changed: {source}"
                print(f"{source.name}: {baseline[4]:.3f}s -> {current[4]:.3f}s")
            else:
                print(f"{source.name}: {current[4]:.3f}s")

        for name in ["stdlibs/core.amc", "stdlibs/std.amc", "stdlibs/main.amc",
                     "test/main.amc", "examples/find_prime_numbers.amc"]:
            check(ROOT / name)

        # This upstream example imports the missing std.parser module. Compare
        # its diagnostics too, without requiring that unrelated example to work.
        if args.baseline:
            check(ROOT / "stdlibs/hello.amc", expect_success=False)

        invalid = temporary / "invalid.amc"
        invalid.write_text("fn main() { unknown_function(); };\n")
        check(invalid, expect_success=False)

        if args.benchmark:
            source = temporary / "scale.amc"
            for count in [1000, 2000, 4000, 8000]:
                source.write_text("import core;\n" + "".join(
                    f"pub fn f{i}() {{ f{(i + 1) % count}(); }};\n"
                    for i in range(count)
                ))
                print(f"{count} functions: ", end="", flush=True)
                check(source)


if __name__ == "__main__":
    main()
