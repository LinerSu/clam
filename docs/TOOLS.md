# Clam/Seadsa/Crab Tool Guide

This folder documents the Clam/Crab tool in a step-by-step way.

## Reading order

1. [01-artifact-contents-manifest.md](01-artifact-contents-manifest.md)
2. [02-data-structure-analysis-implementation.md](02-data-structure-analysis-implementation.md)
3. [03-taint-analysis-implementation.md](03-taint-analysis-implementation.md)
4. [04-one-command-quick-run.md](04-one-command-quick-run.md)
5. [06-transfer-functions-and-crabir.md](06-transfer-functions-and-crabir.md)
6. [05-how-to-add-abstract-domain.md](05-how-to-add-abstract-domain.md)
7. [08-validated-full-pipeline.md](08-validated-full-pipeline.md)
8. [07-build-outside-docker.md](07-build-outside-docker.md)

## Scope

The docs cover the Clam/Crab tool capabilities including:

- compiling source C to LLVM bitcode,
- building SeaDSA local data-structure graphs (including interval-cell behavior),
- instrumenting IR with taint intrinsics from YAML,
- using SeaDSA for memory graph information,
- running Crab to check safety properties.

The executable path used in this guide is:

- `$CLAM_ROOT/bin/clam`
- `$CLAM_ROOT/bin/clam.py`
- `$CLAM_ROOT/bin/clam-pp`
- `$CLAM_ROOT/bin/seaopt`
- `$CLAM_ROOT/bin/read_results.py`

Assumption:

- `CLAM_ROOT` is an environment variable already set in the Docker image.
- To verify availability quickly, run `<tool_name> --help` (tools are on `PATH` already) or `$CLAM_ROOT/bin/<tool_name> --help`.

## Tool Quick Reference

Set path once before running commands (if needed):

```ssh
CLAM_ROOT=<path-to-clam-root>
```

| Tool | What it does | Input | Output | Example command |
|---|---|---|---|---|
| clam.py | Full pipeline driver (preprocess + optimize + analyze + report). | Source or .bc plus pipeline options. | Temp files, analysis logs, optional .crabir. | $CLAM_ROOT/bin/clam.py --help |
| clam-pp | LLVM preprocessing and taint instrumentation pass runner. | .bc and optional YAML taint config. | Preprocessed/instrumented .bc. | $CLAM_ROOT/bin/clam-pp -o out.pp.bc in.bc --clam-taint-config=taint.yaml |
| seaopt | Optimization pass before running clam. | Preprocessed .bc and -O flags. | Optimized .bc and optionally .ll. | $CLAM_ROOT/bin/seaopt -f -o out.pp.o.bc -O1 in.pp.bc |
| clam | Main abstract interpretation engine (Crab). | .bc and crab options. | Check counts, optional analyzed .ll, optional .crabir. | $CLAM_ROOT/bin/clam in.bc --crab-dom=int --crab-track=mem --crab-check |
| read_results.py | Summarize assertion outcomes from .crabir. | .crabir file. | Proven/warning/checked/unchecked summary. | $CLAM_ROOT/bin/read_results.py result.crabir |

## Uses beyond the paper

Clam/Crab is a general-purpose abstract-interpretation framework. The taint analysis shown in the paper is one use case; the tool supports many more:

| Use case | How |
|---|---|
| Numerical invariant inference | Run `clam --crab-dom=int` (intervals) or `--crab-dom=zones` / `--crab-dom=oct` on any LLVM bitcode. |
| Buffer-safety checking | Use `--crab-track=arr` to enable array-bounds tracking. |
| Inter-procedural analysis | Pass `--crab-inter` for a context-insensitive inter-procedural run. |
| Pointer/memory analysis | Combine with SeaDSA (`--crab-track=mem`) for heap-shape information. |
| Adding a new abstract domain | Follow [05-how-to-add-abstract-domain.md](05-how-to-add-abstract-domain.md) to register a custom Crab domain and expose it via `--crab-dom=<name>`. |
| Taint-config variation | Write a new YAML taint config and pass it via `--clam-taint-config=<file>` to instrument different source/sink pairs without recompiling. |
| Running on arbitrary C | Compile with `clang-14 -c -emit-llvm`, preprocess with `clam-pp`, then run `clam`. No special project setup is needed. |

## Extension interfaces and open-source status

Clam and Crab are open-source (MIT / Apache 2.0; see `LICENSE` in each sub-repository). The key documented extension points are:

| Extension point | Document |
|---|---|
| Extend SeaDSA data-structure analysis / interval cells | [02-data-structure-analysis-implementation.md](02-data-structure-analysis-implementation.md) |
| Add a new abstract domain | [05-how-to-add-abstract-domain.md](05-how-to-add-abstract-domain.md) |
| Understand/implement transfer functions | [06-transfer-functions-and-crabir.md](06-transfer-functions-and-crabir.md) |
| Add or modify taint sources/sinks | [03-taint-analysis-implementation.md](03-taint-analysis-implementation.md) |
| Source layout and registration hooks | [01-artifact-contents-manifest.md](01-artifact-contents-manifest.md) |

All source code is available in the repository. Crab domain API contracts are in `crab/include/crab/domains/abstract_domain.hpp`.

## Running outside Docker

The tool can be built natively on Ubuntu 22.04 without Docker. See [07-build-outside-docker.md](07-build-outside-docker.md) for the step-by-step native build instructions.
