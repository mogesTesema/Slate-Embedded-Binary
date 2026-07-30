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
├── include/slate/       # Public API headers
│   ├── slate.hh         # Top-level include
│   ├── compile/         # Formula compiler headers
│   ├── eval/            # Evaluator headers
│   ├── graph/           # Dependency graph headers
│   ├── model/           # Workbook / sheet / cell model
│   ├── recalc/          # Recalculation driver headers
│   ├── render/          # Output renderer headers
│   └── util/            # Internal utilities (arena, status, crc32, …)
├── src/                 # Implementation files
│   ├── compile/         # Parser, binder, verifier
│   ├── eval/            # Evaluator + function library
│   ├── format/          # Binary format decoder
│   ├── graph/           # Dep-graph and topo-sort
│   ├── model/           # Grid, sheet, workbook, reference
│   ├── recalc/          # Driver, epoch, teardown, compaction
│   ├── render/          # CSV export
│   └── util/            # Arena, status, crc32, intern table
├── tests/               # Unit tests
├── fuzz/                # Fuzz targets + seed corpus
│   ├── fuzz_targets/
│   └── corpus/
├── tools/               # Developer utilities
│   ├── mkworkbook/      # Workbook fixture builder
│   └── gen_seeds/       # Fuzz seed corpus generator
├── verifier/            # Local build verification script
└── .clusterfuzzlite/    # OSS-Fuzz / ClusterFuzzLite config
```

---

## Fuzzing

The project ships three LibFuzzer targets and integrates with [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/).

| Target | Entry point |
|--------|------------|
| `formula_fuzzer` | Formula parser + compiler pipeline |
| `recalc_fuzzer` | Recalculation driver with synthetic workbook |
| `workbook_parse_fuzzer` | Binary workbook format decoder |

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
