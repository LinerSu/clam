# Transfer Functions and CrabIR Primer

This page is a prerequisite for [How to Add a New Abstract Domain](05-how-to-add-abstract-domain.md).

It explains:

- what transfer functions are in Crab/Clam,
- where operation syntax is defined,
- where transfer-function interfaces are defined,
- how to use default no-op handlers when your domain does not support some statement classes.

## 1) Where Transfer Functions Are Defined

Core abstract-domain interface (forward/backward transformers):

- [crab/include/crab/domains/abstract_domain.hpp](../crab/include/crab/domains/abstract_domain.hpp)

Operator enums and printed syntax tokens:

- [crab/include/crab/domains/abstract_domain_operators.hpp](../crab/include/crab/domains/abstract_domain_operators.hpp)

Default helper macros for unsupported operations:

- [crab/include/crab/domains/abstract_domain_macros.def](../crab/include/crab/domains/abstract_domain_macros.def)

## 2) What a Transfer Function Is

A transfer function updates abstract state for one CrabIR statement.

Examples from [crab/include/crab/domains/abstract_domain.hpp](../crab/include/crab/domains/abstract_domain.hpp):

- Numerical forward:
  - `apply(arith_operation_t, x, y, z)`
  - `assign(x, e)`
  - `operator+=(constraints)`
- Boolean forward:
  - `assign_bool_cst`, `assign_bool_var`, `assume_bool`
- Reference/region forward:
  - `ref_make`, `ref_load`, `ref_store`, `ref_gep`, `ref_assume`
- Array forward:
  - `array_init`, `array_load`, `array_store`, `array_store_range`
- Backward variants:
  - `backward_apply`, `backward_assign`, `backward_array_*`, `backward_assign_bool_*`

The same file also defines call-related transformers:

- `callee_entry(...)`
- `caller_continuation(...)`

## 2.1) Transfer Functions on Common CrabIR Statements (Examples)

Below are representative CrabIR statements and the corresponding transfer-function families they trigger.

### Region and reference setup

```text
region_init(@V_27:region(int));
tmp := make_ref(@V_27:region(int),4,as_7);
assume(tmp > NULL_REF);
```

- `region_init(...)` maps to region initialization handling.
- `make_ref(...)` maps to `ref_make(...)`.
- `assume(...)` refines state (numerical/reference constraint refinement depending on the form).

### Reference arithmetic and memory read/write

```text
(@V_27:region(int),taint.cast:ref) := gep_ref(@V_27:region(int),tmp:ref);
store_to_ref(@V_27:region(int),tmp:ref,tmp3:int32);
tmp4:int32 := load_from_ref(@V_27:region(int),tmp:ref);
```

- `gep_ref(...)` maps to `ref_gep(...)`.
- `store_to_ref(...)` maps to `ref_store(...)`.
- `load_from_ref(...)` maps to `ref_load(...)`.

### Arithmetic and assignment style

```text
@V_59 = 42;
arg = @V_40;
```

- Constant/variable assignment is modeled by numerical assignment transfer functions (`assign(...)`-style semantics).

### Calls and interprocedural transfer

```text
tmp6:int32 = call foo(tmp4:int32,...);
```

- Calls are handled by call transformers:
  - `callee_entry(...)` (map caller state to callee entry state)
  - `caller_continuation(...)` (map callee exit effects back to caller)

### Taint intrinsics and checks

```text
crab_intrinsic(add_tag,@V_27:region(int),taint.cast:ref,1:int32);
@V_61 = crab_intrinsic(check_does_not_have_tag,@V_25:region(int),tmp5:ref,1:int32);
assert(@V_61);
```

- `crab_intrinsic(...)` is dispatched through domain intrinsic hooks.
- `assert(...)` is checked against the current abstract state (safe/warn/error classification by analyzer/checker pipeline).

## 3) CrabIR-Like Operation Syntax

Printed operator tokens come from [crab/include/crab/domains/abstract_domain_operators.hpp](../crab/include/crab/domains/abstract_domain_operators.hpp):

- Arithmetic: `+`, `-`, `*`, `/`, `/_u`, `%`, `%_u`
- Bitwise: `&`, `|`, `^`, `<<`, `>>_l`, `>>_a`
- Integer conversions: `trunc`, `sext`, `zext`
- Boolean binary ops: `&`, `|`, `^`

This is why CrabIR output looks close to LLVM-style operations, while still using Crab-specific statement forms.

## 3.1) What Variables Can Look Like in CrabIR

Common kinds from the examples above:

- Integer scalar variable:
  - `@V_59:int32`
- Reference variable:
  - `tmp:ref`, `taint.cast:ref`
- Region variable:
  - `@V_27:region(int)`
  - `@V_9:region(unknown)`

Practical interpretation:

- A `region(...)` variable denotes an abstract memory region (a summary for one or more concrete memory objects/fields).
- `region(int)` usually indicates typed region content modeled as integer-typed payload.
- `region(unknown)` usually indicates unknown/less-typed region payload.

## 4) Practical Semantics Mapping (LLVM-ish)

The API in [crab/include/crab/domains/abstract_domain.hpp](../crab/include/crab/domains/abstract_domain.hpp) is designed so transfer semantics are close to LLVM IR intent:

- `assign(x, e)`: model SSA-style assignment of arithmetic expression.
- `apply(op, x, y, z|k)`: model arithmetic/bitwise instruction effects.
- `operator+=(csts)`: refine state with assumptions/guards.
- `assume_bool(v, is_negated)`: branch guard refinement.
- `ref_load` / `ref_store`: abstract memory read/write.
- `ref_gep`: pointer/reference arithmetic (offset-based).
- `array_load` / `array_store`: array memory abstraction.

Representative statement-to-semantics examples:

- `region_init(@V_27:region(int))`: create/init abstract memory region state.
- `assume(tmp > NULL_REF)`: constrain reference nullness.
- `store_to_ref(..., v:int32)`: write scalar abstract value into region cell.
- `load_from_ref(..., res:int32)`: read scalar abstract value from region cell.
- `tmp = call foo(...)`: compose caller/callee transformers interprocedurally.

## 5) Choosing Domain Type and Transfer Coverage

Different domain families usually implement different subsets first:

- Numerical domain: prioritize numerical + boolean transfer functions.
- Memory/reference domain: prioritize region/reference transfer functions.
- Array domain: prioritize array transfer functions.
- Reduced product / hybrid domain: combine all relevant categories and inter-domain reduction.

Reference examples:

- Split DBM uses numerical core and defaults unsupported categories:
  - [crab/include/crab/domains/split_dbm.hpp](../crab/include/crab/domains/split_dbm.hpp)

## 6) Default No-Op Macros for Unsupported Statement Classes

When introducing a domain with only standard numerical behavior, you can explicitly mark unsupported classes as no-op via macros in [crab/include/crab/domains/abstract_domain_macros.def](../crab/include/crab/domains/abstract_domain_macros.def):

- `BOOL_OPERATIONS_NOT_IMPLEMENTED(DOM)`
- `ARRAY_OPERATIONS_NOT_IMPLEMENTED(DOM)`
- `REGION_AND_REFERENCE_OPERATIONS_NOT_IMPLEMENTED(DOM)`

These macros provide empty/default implementations for those operation groups.

Important consequence:

- Statements in unsupported groups will not refine state (or will use fallback/top behavior), so precision may drop.
- This is acceptable for an initial domain bring-up, but you should document unsupported semantics.

## 7) Minimal Domain Skeleton Workflow

1. Implement lattice + numerical transfer functions in your domain class.
2. Add one or more NOT_IMPLEMENTED macros for unsupported categories.
3. Build and run simple CrabIR tests.
4. Gradually replace macro defaults with real transfer implementations as needed.

Continue with [How to Add a New Abstract Domain](05-how-to-add-abstract-domain.md) for registration, CLI exposure, and build integration.