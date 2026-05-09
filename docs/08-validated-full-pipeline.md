# Validated Full Pipeline (Yama)

This page captures a fully validated taint-analysis run using the exact command line and output profile provided by the experiment harness.

## Binary Path Convention

For portability across machines, define:

```bash
CLAM_ROOT="<path-to-clam-root>"
```

In Docker, `CLAM_ROOT` is assumed to be already configured.

Quick availability checks:

```bash
clam --help
clam.py --help
clam-pp --help
seaopt --help
```

If tools are not on `PATH`, run the same checks as `$CLAM_ROOT/bin/<tool_name> --help`.

All stage commands below are shown in portable form using `$CLAM_ROOT/bin/<tool>`.

## 1) End-to-End Command (exact)

```bash
"$CLAM_ROOT/bin/clam.py" -g -S -O1 --save-temps --temp-dir=/tmp/crab_taint --lower-unsigned-icmp --lower-memcpy --llvm-peel-loops=1 --devirt-functions=sea-dsa --crab-name-values=false --dsa-lazy-mem-transfer --crab-dom=int --crab-inter --crab-inter-recursive-functions=true --crab-inter-exact-summary-reuse=true --crab-inter-max-summaries=9999999 --crab-inter-entry-main=true --crab-track=mem --crab-singleton-aliases --crab-heap-analysis=ci-sea-dsa-types --crab-narrowing-iterations=1 --crab-widening-delay=2 --crab-widening-jump-set=0 --crab-preserve-invariants=false --crab-dom-param=region.is_dereferenceable=true:region.tag_analysis=true --crab-check=assert --crab-disable-warnings --crab-print-invariants=false --taint-config=/home/ljgy/Works/seatools/taint-benchs/expr_build/yaml/clam.taint.yaml --count-asserts --ocrab=/home/ljgy/Works/seatools/taint-benchs/expr_build/dump/small_complex_8/small_complex_8.crabir --inline=false /home/ljgy/Works/seatools/taint-benchs/expr_build/benchmarks/small/complex/small_complex_8/llvm-ir/small_complex_8.ir/small_complex_8.ir.bc
```

## 2) Stage-by-Stage Commands Emitted by clam.py

The wrapper expands into the following stages:

### Stage A: clang

- Skipped in this run because input is already bitcode.

Observed message:

```text
--- Clang skipped: input file is already bitecode
```

### Stage B: clam-pp (instrumentation + preprocessing)

```bash
"$CLAM_ROOT/bin/clam-pp" -o /tmp/crab_taint/small_complex_8.ir.pp.bc /tmp/crab_taint/small_complex_8.ir.bc -S --simplifycfg-sink-common=false --clam-promote-malloc=true --clam-peel-loops=1 --clam-lower-memcpy --clam-taint-config=/home/ljgy/Works/seatools/taint-benchs/expr_build/yaml/clam.taint.yaml --scalarize-load-store=true --clam-devirt --devirt-resolver=sea-dsa --sea-dsa-type-aware=true
```

### Stage C: seaopt

```bash
"$CLAM_ROOT/bin/seaopt" -f -o /tmp/crab_taint/small_complex_8.ir.pp.o.bc -O1 -S --simplifycfg-sink-common=false --vectorize-slp=false /tmp/crab_taint/small_complex_8.ir.pp.bc
```

### Stage D: clam (analysis)

```bash
"$CLAM_ROOT/bin/clam" /tmp/crab_taint/small_complex_8.ir.pp.o.bc -oll /tmp/crab_taint/small_complex_8.ir.pp.o.ll -S -crab-dom-param region.is_dereferenceable=true -crab-dom-param region.tag_analysis=true --simplifycfg-sink-common=false --clam-lower-unsigned-icmp --crab-dom=int --crab-widening-delay=2 --crab-widening-jump-set=0 --crab-narrowing-iterations=1 --crab-relational-threshold=10000 --crab-track=mem --crab-heap-analysis=ci-sea-dsa --sea-dsa-type-aware=true --sea-dsa-lazy-mem-transfer --crab-singleton-aliases --crab-inter --crab-inter-max-summaries=9999999 --crab-inter-recursive=true --crab-inter-exact-summary-reuse=true --crab-inter-entry-main=true --crab-check --crab-print-cfg=false --crab-enable-warnings=false --crab-print-invariants=none --crab-store-invariants=false --crab-dot-cfg=false --ocrab=/home/ljgy/Works/seatools/taint-benchs/expr_build/dump/small_complex_8/small_complex_8.crabir --use-crab-name-values=false --crab-enable-bignums=false
```

### Stage E: read_results.py

```bash
"$CLAM_ROOT/bin/read_results.py" /home/ljgy/Works/seatools/taint-benchs/expr_build/dump/small_complex_8/small_complex_8.crabir
```

## 3) Key Observed Runtime Messages

```text
Sea-Dsa type aware!
Omnipotent char: omni_i8*
[2026-05-06.14:51:27] Started sea-dsa analysis
[2026-05-06.14:51:27] Finished sea-dsa analysis
[2026-05-06.14:51:27] Started clam
[2026-05-06.14:51:27] Total number of analyzed functions:6
[2026-05-06.14:51:27] Running top-down inter-procedural analysis with domain:"Region"  ...
[2026-05-06.14:51:28] Finished inter-procedural analysis
Created file with CrabIR and assertion results
  path: /home/ljgy/Works/seatools/taint-benchs/expr_build/dump/small_complex_8/small_complex_8.crabir
```

## 4) Expected Analysis Results for this Run

```text
************** ANALYSIS RESULTS ****************
0  Number of total safe checks
0  Number of total error checks
1  Number of total warning checks
0.01 SEADSA running times
************** ANALYSIS RESULTS END*************
```

```text
************** ASSERTION RESULTS ****************
   0  Number of total proven assertions
   1  Number of total warning assertions
   1  Number of total checked assertions
   0  Number of total unchecked assertions (These assertions are in functions that have not been analyzed)
0.0%  Ratio of proven assertions
************** ASSERTION RESULTS END*************
```

## 5) Expected Artifacts Produced

- Preprocessed bitcode:
  - `/tmp/crab_taint/small_complex_8.ir.pp.bc`
- Optimized preprocessed bitcode:
  - `/tmp/crab_taint/small_complex_8.ir.pp.o.bc`
- Optimized LLVM textual IR:
  - `/tmp/crab_taint/small_complex_8.ir.pp.o.ll`
- Final CrabIR with assertion outcomes:
  - `/home/ljgy/Works/seatools/taint-benchs/expr_build/dump/small_complex_8/small_complex_8.crabir`

## 6) Reproduction Checklist

- Use LLVM 14-compatible toolchain.
- Use matching runtime binaries under `CLAM_ROOT`.
- Keep all crab-dom and inter-procedural options identical.
- Keep taint YAML path identical or equivalent in content.
- Confirm that final result counters are exactly: safe=0, error=0, warning=1.
