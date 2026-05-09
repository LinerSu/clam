# Artifact Contents Manifest

This manifest states what is prebuilt, what is source, and which analyzer/toolchain requirements are needed.

## Required Analyzer/Toolchain Version

- LLVM major version expected: 14
- Use the provided binaries via `CLAM_ROOT` for reproducibility.
- In Docker, `CLAM_ROOT` is assumed to be preconfigured to point to the installation root.

## Prebuilt Binaries

The following binaries are already available and can be used directly (no build required):

- `$CLAM_ROOT/bin/clam`
- `$CLAM_ROOT/bin/clam-pp`
- `$CLAM_ROOT/bin/clam-diff`
- `$CLAM_ROOT/bin/seadsa`
- `$CLAM_ROOT/bin/seaopt`
- `$CLAM_ROOT/bin/clam.py`

## Source Files for Taint Pipeline

- IR taint instrumentation pass:
  - [lib/Transforms/InsertTaintIntrinsic.cc](../lib/Transforms/InsertTaintIntrinsic.cc)
- Taint pass registration in preprocessing pipeline:
  - [tools/clam-pp/clam-pp.cc](../tools/clam-pp/clam-pp.cc)
- Taint-related intrinsic examples used by unit tests:
  - `tests/tag-analysis/test1.c`
  - `tests/tag-analysis/test2.c`

## Source Files for Domain Registration

- Domain option definitions and user-facing names:
  - [include/clam/CrabDomain.hh](../include/clam/CrabDomain.hh)
- Domain parser:
  - [lib/Clam/CrabDomainParser.cc](../lib/Clam/CrabDomainParser.cc)
- CLAM-level domain registration entry point:
  - [lib/Clam/RegisterAnalysis.cc](../lib/Clam/RegisterAnalysis.cc)
- Domain registration declarations:
  - [lib/Clam/crab/domains/register_domains.hh](../lib/Clam/crab/domains/register_domains.hh)
- Registration macro used by domain implementations:
  - [lib/Clam/crab/domains/crab_defs.hh](../lib/Clam/crab/domains/crab_defs.hh)

## Minimal Runtime Assumption

`$CLAM_ROOT/bin/clam` must be executable.

Quick check:

- If tools are on `PATH`: `<tool_name> --help`
- Otherwise: `$CLAM_ROOT/bin/<tool_name> --help`
