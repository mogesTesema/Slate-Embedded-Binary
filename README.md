# Slate Embedded Binary

A compact, embeddable C++ library for spreadsheet formula parsing, evaluation, and workbook recalculation. Designed for low-latency environments where pulling in a full spreadsheet runtime is not an option.

---

## Features

- **Formula compiler** — recursive-descent parser + name binder + type verifier
- **Evaluator** — stack-machine executor with 150+ built-in functions (math, text, lookup, date, financial, statistical)
- **Dependency graph** — directed acyclic dependency tracking with topological sort for correct recalc ordering
- **Recalculation driver** — dirty-cell propagation, epoch tracking, incremental recalc, and graph compaction
- **Format decoder** — lightweight reader for the Slate workbook binary format (section headers, RLE decompression, shared string table)
- **CSV renderer** — zero-allocation export path for evaluated grids
- **Fuzz-tested** — ClusterFuzzLite integration with three dedicated fuzz targets

---

## Building

Requires CMake ≥ 3.20 and a C++17-capable compiler (Clang or GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run the unit test suite:

```bash
cmake --build build --target slate_tests
./build/tests/slate_tests
```

---

## Repository Layout

```
slate/
├── include/
├── src/                 
├── tests/               

```

---



To run locally against the provided seed corpus:

```bash
# Build with sanitizers
cmake -S . -B build-fuzz \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,fuzzer-no-link" \
  -DSLATE_BUILD_FUZZ=ON
cmake --build build-fuzz

# Run one target
./build-fuzz/fuzz/formula_fuzzer fuzz/corpus/formula_fuzzer -max_len=4096
```

---

## Tools

### `mkworkbook`

Generates synthetic `.slw` workbook binaries for use in tests and as fuzzer seeds.

```bash
./build/tools/mkworkbook/mkworkbook --sheets 3 --rows 100 -o fixture.slw
```

### `gen_seeds`

Generates a structured seed corpus for the fuzz targets.

```bash
./build/tools/gen_seeds/gen_seeds --out fuzz/corpus/
```

---

## License

MIT
