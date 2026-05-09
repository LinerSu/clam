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

## 5) How Tags Propagate in Crab Region Domain

Tag propagation in the analyzer is implemented in:

- [crab/include/crab/domains/region_domain.hpp](../crab/include/crab/domains/region_domain.hpp)

and is guarded by:

- `crab_domain_params_man::get().region_tag_analysis()`

When enabled, the domain tracks tags in `m_tag_env`, a map from variables/regions to finite tag sets.

### 5.1 Core State and Lattice Behavior

- Tag state is stored in `m_tag_env` (`tag_env_t`).
- `is_top()` and `operator<=` include tag checks only when `region_tag_analysis()` is enabled.
- Join/widening use union on tags (`|`), and meet/narrowing use intersection (`&`).
- Forget/project/rename also update `m_tag_env` when enabled.

### 5.2 Propagation Through Transfer Functions

Representative propagation rules in `region_domain.hpp`:

- Arithmetic and assignment:
  - `apply(op, x, y, z)`: tags of `x` become tags(`y`) union tags(`z`).
  - `apply(op, x, y, k)`: tags of `x` become tags(`y`).
  - `assign(x, e)`: tags of `x` are merged from all variables in expression `e`.
- Cast-like/ref conversions:
  - `ref_to_int`: tags(`int_var`) := tags(`ref_var`).
  - `int_to_ref`: tags(`ref_var`) := tags(`int_var`).
- Memory/reference operations:
  - `ref_store` strong update: region tags become tags of stored value variable (or bottom for constants).
  - `ref_store` weak update: region tags are unioned with stored value tags.
  - `ref_gep(ref1,...,ref2,...)`: tags(`ref2`) include tags(`ref1`).
  - `ref_load(..., res)`: tags(`res`) are copied from region tags.

### 5.3 Intrinsic-Specific Semantics

In `intrinsic(...)`:

- `add_tag(rgn, ref, TAG)`:
  - adds `TAG` to tags(`rgn`).
  - current implementation ignores `ref` and updates at region granularity.
- `move_tag(rgn1, ref1, rgn2, ref2)`:
  - sets tags(`rgn2`) := tags(`rgn1`).
  - this is a copy-style update (not union with previous tags in `rgn2`).
- `check_does_not_have_tag(rgn, ref, TAG) -> b`:
  - if `TAG` is definitely absent from tags(`rgn`), sets `b = true`.
  - otherwise, keeps `b` unconstrained and emits sink-oriented debug output under log channel `region-tag-report`.

### 5.4 Path-Tag Note

`add_path_tags(...)` is disabled all many transfer functions. `m_ctrl_deps` is not used.

## 6) How to Extend Taint Instrumentation

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
