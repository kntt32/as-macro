# Indexing regression tests

Run the C regression tests (hash collisions, rehashing, key ownership, overload
order, independent generators, forward labels, ELF symbol order and relocations):

```sh
make -C assembler CC=gcc check
```

The default build enables AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer. In environments where LeakSanitizer cannot run under ptrace, use
`ASAN_OPTIONS=detect_leaks=0` to run the other sanitizers.

To compare object files and diagnostics against an older assembler and measure
scaling, first build both versions with identical flags, then run:

```sh
make -C assembler clean
make -C assembler CC=gcc FSANITIZE=0 check
python3 test/check_indexing.py --baseline /path/to/old/asm.elf --benchmark
```

The benchmark defines 1,000, 2,000, 4,000 and 8,000 functions, each calling the
next one. It compares the complete ELF output byte for byte. Timing is printed
without a pass/fail threshold because it depends on the machine. The regular
fixtures also cover imports, templates, overloaded macros and invalid input.

The generator keeps its ordered vectors for ownership and deterministic output.
An internal string hash map indexes vector positions, so vector reallocations
do not invalidate the index. Probing uses FNV-1a and linear probing, with a
maximum load of 75% and geometric growth. Lookup is expected O(1) plus the
number of candidates with the same name; overload/signature and path resolution
retain their original order and behavior. Hash-table worst-case lookup remains
O(n). Pending relocations are processed once, and ELF symbol indices are built
once per label-table version. Vector growth now respects allocated capacity,
making repeated append amortized O(1).
