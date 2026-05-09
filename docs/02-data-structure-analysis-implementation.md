# Data Structure Analysis Implementation Details (SeaDSA Interval Graph)

This document explains the SeaDSA data-structure analysis extension centered on interval cells.

It is intended as a developer-facing note for:

- where the implementation lives,
- what changed in the local (intraprocedural) phase,
- how to run and compare baseline vs interval-enabled behavior,
- how to extend pointer analysis further by making cell offsets more flexible.

## 1) Goal and Scope

The goal of this extension is to improve local SeaDSA precision by representing pointer targets as intervals (not only singleton offsets), then propagating that information through local transfer functions.

Primary implementation files:

- `sea-dsa/lib/seadsa/Interval_Graph.cc`
- `sea-dsa/lib/seadsa/DsaLocal.cc`
- `sea-dsa/include/seadsa/Interval_Graph.hh`

Related changes include graph printing and mapping behavior to preserve interval semantics.

## 2) Main Implementation Summary

### 2.1 Interval graph core (`Interval_Graph.cc`)

Key changes include:

- Interval-aware `Cell` behavior in graph operations.
- Support for partial offset collapse with explicit collapsed-cell tracking.
- Canonicalization during partial collapse so links in collapsed ranges map to a stable representative offset while preserving field type.
- Simulation and caller/callee mapping updates to keep graph correspondence stable under interval semantics.

Useful runtime flags introduced/used here:

- `--sea-dsa-type-aware`
- `--sea-dsa-partial-collapse`

The partial collapse flag is now connected to a global toggle (`seadsa::g_IsPartialCollapseEnabled`) so behavior can be switched consistently.

### 2.2 Local phase updates (`DsaLocal.cc`)

The local phase is still intraprocedural, but now computes richer GEP information and uses it when building/updating graph cells. The goal for this change is to improve approximation of interval offsets for pointers accessing an array.

Highlights:

- `GepOffset` now carries:
  - fixed offset (`noffset`),
  - variable stride (`stride`),
  - conservative lower/upper range bounds,
  - first known array extent (`arraySize`).
- `computeGepOffset(...)` computes both numeric and interval/range metadata.
- `visitGep(...)` uses the richer result to decide whether to:
  - reuse base-node offsets,
  - create/merge sequence nodes,
  - perform partial collapse-aware joins.

This is the core path where intraprocedural transfer functions become interval-aware for pointer arithmetic.

### 2.3 Transfer-function impact in local analysis

Local transfer handlers (load/store/gep/mem-transfer) continue to define graph updates, now with interval-sensitive behavior through the updated graph/cell abstractions.

In particular:

- GEP transfer uses interval-aware offset computation.
- Link creation and unification can preserve more structure instead of eagerly collapsing in some cases.
- Memory-transfer handling (eager/lazy) benefits from richer node/cell structure during propagation.

## 3) Build Modes: Baseline vs Interval-Enabled
You can toggle the interval-enabled behavior using the `--sea-dsa-partial-collapse` flag when running `clam`. This allows you to compare the baseline (non-interval) behavior with the new interval-enabled behavior. This is very end flag, we recommend to follow taint analsysis benchmark script to run at the front end.
### Toggle partial collapse (optional A/B inside IDSA build)

```bash
"$IDSA_CLAM_ROOT/bin/clam" INPUT_EXAMPLE.bc \
  --crab-dom=int \
  --crab-track=mem \
  --crab-heap-analysis=cs-sea-dsa \
  --crab-dsa-dot \
  --sea-dsa-partial-collapse=false \
  --sea-dsa-dot-outdir=./out-baseline

"$IDSA_CLAM_ROOT/bin/clam" INPUT_EXAMPLE.bc \
  --crab-dom=int \
  --crab-track=mem \
  --crab-heap-analysis=cs-sea-dsa \
  --crab-dsa-dot \
  --sea-dsa-partial-collapse=true \
  --sea-dsa-dot-outdir=./out-idsa
```

## 4) Generate and Compare Graphs

You can compare graph shape using dot outputs from both runs.

Typical workflow:

```bash
# 1) Run baseline and IDSA commands (Section 3)

# 2) Inspect generated dot files
ls out-baseline/*.dot
ls out-idsa/*.dot

# 3) (Optional) Render selected graphs
# dot -Tpng out-idsa/<function>.mem.dot -o out-idsa/<function>.mem.png
```

Comparison checklist:

- Node count and merge/split behavior.
- Link offsets (singleton vs interval-induced canonicalization).
- Sequence-node creation for GEP-heavy code.
- Collapsed-cell patterns when partial collapse is enabled.

## 6) How to Extend Pointer Analysis Further (High-Level)

To make offsets more flexible beyond the current implementation, focus on extending `Cell` semantics and all transfer points that consume it.

Recommended direction:

1. Extend `Cell` representation
- Create a new abstract domain for cell offset
- Refactor cell class to use this domain.
- Be aware of domain operations required (join, widen, meet) and ensure they are implemented soundly.

2. Understand how dsa works.

3. Update transfer functions consistently
- `visitGep(...)`, `visitLoadInst(...)`, `visitStoreInst(...)`, and memory transfer handlers must interpret and propagate the new offset model in the same way.

4. Preserve sound fallback behavior
- When precision is uncertain, fall back to conservative collapse/merge.
- Keep explicit toggles for new precision features (as done with partial collapse).

5. Validate with focused regression tests