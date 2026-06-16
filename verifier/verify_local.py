#!/usr/bin/env python3
"""Local crash verifier for the Slate fuzz harnesses.

Builds the requested libFuzzer+ASAN target offline and replays a single proof
input through it, reporting whether the input crashes the current source tree.

Usage:
    python3 verifier/verify_local.py --bundle-root . --poc <file> [--harness recalc_fuzzer]

Exit status:
    0  input replayed cleanly (no sanitizer error)  -> "CLEAN"
    1  input triggered a sanitizer error / abort     -> "CRASH"
    2  build or usage error

This is deliberately dependency-free (standard library only) and does no network
access, so it runs inside a `--network=none` sandbox. It reuses a cached build
directory (`build-verify/`) so repeated verifications are fast.
"""
import argparse
import os
import re
import subprocess
import sys

HARNESSES = {"workbook_parse_fuzzer", "recalc_fuzzer", "formula_fuzzer"}
SAN_MARKERS = re.compile(
    r"(AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|"
    r"ERROR: libFuzzer|SEGV|heap-buffer-overflow|heap-use-after-free|"
    r"double-free|stack-overflow)"
)


def run(cmd, **kw):
    return subprocess.run(cmd, **kw)


def build_harness(root, builddir, harness):
    env = dict(os.environ)
    # Force a hermetic, sanitizer-instrumented libFuzzer build.
    # Mirror ClusterFuzzLite's ASAN configuration, which is normally injected via
    # $CXXFLAGS. The fuzzer targets additionally link clang's libFuzzer driver.
    asan = "-fsanitize=address -fno-omit-frame-pointer -g -O1"
    cfg = [
        "cmake", "-S", root, "-B", builddir,
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
        "-DSLATE_BUILD_FUZZERS=ON",
        "-DCMAKE_CXX_FLAGS=" + asan,
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address",
    ]
    r = run(cfg, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode(errors="replace"))
        return None
    r = run(["cmake", "--build", builddir, "--target", harness, "-j",
             str(os.cpu_count() or 4)],
            cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode(errors="replace"))
        return None
    for cand in (os.path.join(builddir, harness),
                 os.path.join(builddir, "fuzz", harness)):
        if os.path.isfile(cand):
            return cand
    # Fall back to a recursive search.
    for dirpath, _dirs, files in os.walk(builddir):
        if harness in files:
            return os.path.join(dirpath, harness)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bundle-root", default=".")
    ap.add_argument("--poc", required=True)
    ap.add_argument("--harness", default="recalc_fuzzer", choices=sorted(HARNESSES))
    ap.add_argument("--build-dir", default="build-verify")
    args = ap.parse_args()

    root = os.path.abspath(args.bundle_root)
    poc = os.path.abspath(args.poc)
    if not os.path.isfile(poc):
        sys.stderr.write("poc not found: %s\n" % poc)
        return 2

    builddir = os.path.join(root, args.build_dir)
    binary = build_harness(root, builddir, args.harness)
    if not binary:
        sys.stderr.write("build failed for harness %s\n" % args.harness)
        return 2

    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "abort_on_error=1:handle_abort=1:detect_leaks=1"
    # libFuzzer replays a single input file when given a path argument.
    r = run([binary, "-runs=1", poc], cwd=root, env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    out = r.stdout.decode(errors="replace")
    crashed = r.returncode != 0 or bool(SAN_MARKERS.search(out))
    if crashed:
        # Surface the first sanitizer line for a quick read.
        for line in out.splitlines():
            if SAN_MARKERS.search(line):
                sys.stderr.write(line + "\n")
                break
        print("CRASH  harness=%s poc=%s rc=%d" % (args.harness,
              os.path.basename(poc), r.returncode))
        return 1
    print("CLEAN  harness=%s poc=%s" % (args.harness, os.path.basename(poc)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
