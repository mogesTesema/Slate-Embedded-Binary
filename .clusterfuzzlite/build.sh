#!/bin/bash -eu
#
# ClusterFuzzLite build entry point. Runs at the repository root with $SRC set to
# the checkout and $OUT set to the artifact directory. Builds every fuzz target
# into $OUT and packages each target's seed corpus. Fully offline: no package
# managers, no downloads, no vendored dependencies — the engine uses only the C++
# standard library and the base image's fuzzing engine.

# Preserve the sanitizer/coverage flags the base image injects via $CXXFLAGS; add
# only the language standard.
export CXXFLAGS="${CXXFLAGS:-} -std=c++20"

cd "$SRC"

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSLATE_BUILD_FUZZERS=ON \
  -DSLATE_BUILD_TOOLS=OFF \
  -DSLATE_BUILD_TESTS=OFF
cmake --build build --parallel "$(nproc)"

for target in workbook_parse_fuzzer recalc_fuzzer formula_fuzzer; do
  cp "build/${target}" "$OUT/"
  # Package the target's seed corpus using the OSS-Fuzz naming convention so the
  # runner picks it up automatically.
  corpus_dir="fuzz/corpus/${target}"
  if [ -d "${corpus_dir}" ] && [ -n "$(ls -A "${corpus_dir}" 2>/dev/null)" ]; then
    (cd "${corpus_dir}" && zip -q -r "$OUT/${target}_seed_corpus.zip" .)
  fi
done
