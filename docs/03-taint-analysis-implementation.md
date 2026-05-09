# Taint Analysis Implementation Details

This document explains where taint analysis is implemented and how to extend it.

## 1) Pipeline Overview

The taint pipeline has two phases:

1. Instrument LLVM IR with taint intrinsics according to a YAML config.
2. Run CLAM/Crab to prove or refute taint safety checks.

Main control points:

- Taint instrumentation pass implementation:
  - [lib/Transforms/InsertTaintIntrinsic.cc](../lib/Transforms/InsertTaintIntrinsic.cc)
- Pass insertion in preprocessing pipeline:
  - [tools/clam-pp/clam-pp.cc](../tools/clam-pp/clam-pp.cc) (calls `createInsertTaintIntrinsicPass()`)

For a fully validated end-to-end run with exact command line and expected outputs, see:

- `docs/08-validated-full-pipeline.md`

## 2) YAML-Driven Instrumentation

The pass consumes YAML through option:

- `--clam-taint-config=<file.yaml>`

Command-line options defined in:

- [lib/Transforms/InsertTaintIntrinsic.cc](../lib/Transforms/InsertTaintIntrinsic.cc)

Notable options:

- `clam-taint-config`: path to YAML taint config
- `clam-print-taint-info`: print taint info at sink calls

## 3) YAML Schema (implemented fields)

Top-level sections:

- `Propagations`
- `Filters`
- `Sinks`

Supported fields include:

- `Name`
- `Args`
- `SrcArgs`
- `DstArgs`
- `VariadicType` (`None`, `Src`, `Dst`)
- `VariadicIndex`

Example structure:

```yaml
Propagations:
  - Name: scanf
    DstArgs: [-1]
    VariadicType: Dst
    VariadicIndex: 1

Sinks:
  - Name: printf
    Args: []
    VariadicIndex: 1
```

## 4) Intrinsics Inserted by the Pass

The instrumentation pass resolves and inserts calls to intrinsics (names in source):

- `add_tag`
- `move_tag`
- `check_has_tag`
- `check_does_not_have_tag`

These symbolic names map to `__CRAB_intrinsic_*` calls in lowered IR.

## 5) How to Extend Taint Instrumentation

### Add support for another source/sink library API

1. Add entries to your YAML config (`Propagations`, `Filters`, `Sinks`).
2. Run `clam-pp` with `--clam-taint-config=<yaml>`.
3. Re-run `clam` on the instrumented bitcode.

### Add new matching/translation logic in C++

1. Edit parsing or matching logic in `InsertTaintIntrinsic.cc`.
2. If you add new intrinsic kinds, extend declaration/discovery in the same file.
3. Ensure pass is still linked via [lib/Transforms/CMakeLists.txt](../lib/Transforms/CMakeLists.txt).

### Add regression tests

Use existing patterns in:

- `tests/tag-analysis/test1.c`
- `tests/tag-analysis/test2.c`

These tests demonstrate expected safe/warn check counts for taint properties.
