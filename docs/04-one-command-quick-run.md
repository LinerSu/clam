# One-Command Quick Run

This document shows how to run the full Clam pipeline from a C file with a single command.

## Goal

Validate that the shipped toolchain can compile a C file, preprocess it, optimize it, and run Clam analysis in one step.

## Binary Path Convention

Use a configurable binary directory so the same commands work across environments:

```bash
CLAM_ROOT="<path-to-clam-root>"
```

In Docker, `CLAM_ROOT` is assumed to be already configured.

Availability check:

```bash
clam.py --help
# or, if not on PATH
"$CLAM_ROOT/bin/clam.py" --help
```

## One Command From C Source

Run from the build directory used by the artifact (`build/` or `build_idsa/`), using the same command line exercised on the development machine:

```bash
"$CLAM_ROOT/bin/clam.py" -O0 \
  --crab-dom=int \
  --crab-track=mem \
  --crab-heap-analysis=cs-sea-dsa \
  --crab-check=assert \
  --crab-sanity-checks \
  ../tests/tag-analysis/test1.c
```

Pipeline stages executed by `clam.py`:

- Compiles the C file to LLVM bitcode.
- Runs the standard preprocessing and optimization pipeline.
- Invokes the Clam analyzer.
- Prints the final assertion summary.

Expected behavior:

- The command exits successfully.
- The analysis summary is printed in the terminal.
- For this specific test, the expected summary counters are:
  - `3  Number of total safe checks`
  - `1  Number of total warning checks`

## Taint Pipeline One-Command Run

For the taint-analysis artifact, use the same `clam.py` entry point on the C input.

```bash
"$CLAM_ROOT/bin/clam.py" -O0 INPUT.c \
  --crab-dom=int \
  --crab-inter \
  --crab-inter-recursive-functions=true \
  --crab-inter-exact-summary-reuse=true \
  --crab-inter-max-summaries=9999999 \
  --crab-inter-entry-main=true \
  --crab-track=mem \
  --crab-heap-analysis=cs-sea-dsa \
  --dsa-lazy-mem-transfer \
  --crab-singleton-aliases=true \
  --crab-widening-delay=2 \
  --crab-widening-jump-set=0 \
  --crab-narrowing-iterations=1 \
  --crab-check=assert \
  --crab-disable-warnings \
  --crab-print-invariants=false \
  --no-crab-preserve-invariants
```

Notes:

- Replace `INPUT.c` with the benchmark C file.
- If the benchmark already contains the taint intrinsics, no extra LLVM-level preprocessing command is needed.
- The command starts from C source and runs the pipeline through analysis.

## Optional: Save CrabIR For Inspection

To inspect the generated CrabIR, keep the same `clam.py` entry point and request a CrabIR output file:

```bash
"$CLAM_ROOT/bin/clam.py" -O0 INPUT.c \
  --crab-dom=int \
  --crab-track=mem \
  --crab-heap-analysis=cs-sea-dsa \
  --crab-check=assert \
  --ocrab=/tmp/taint.crabir
```

If your benchmark uses YAML-based taint instrumentation, add `--taint-config=<file>` to the same command.

This is useful for checking inserted `crab_intrinsic(add_tag|move_tags|has_tag|does_not_have_tag)` operations.
