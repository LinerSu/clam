# How to Add a New Abstract Domain

This guide adapts the workflow from the Clam wiki page AddAbstractDomain to the current layout of this repository.

Prerequisite:

- [Transfer Functions and CrabIR Primer](06-transfer-functions-and-crabir.md)

Goal: add a new domain so it is selectable with `--crab-dom=<your-domain>` and usable by analysis.

## 0) Files You Will Touch

At minimum, you will edit these files:

- [include/clam/CrabDomain.hh](../include/clam/CrabDomain.hh)
- [lib/Clam/ClamOptions.def](../lib/Clam/ClamOptions.def)
- [lib/Clam/crab/domains/register_domains.hh](../lib/Clam/crab/domains/register_domains.hh)
- [lib/Clam/crab/domains/crab_domains.hh](../lib/Clam/crab/domains/crab_domains.hh)
- [lib/Clam/RegisterAnalysis.cc](../lib/Clam/RegisterAnalysis.cc)
- [lib/Clam/CMakeLists.txt](../lib/Clam/CMakeLists.txt)
- `py/clam.py`

And you will add domain implementation files under:

- [lib/Clam/crab/domains/](../lib/Clam/crab/domains/)

## 1) Add the domain kind and CLI name

Edit [include/clam/CrabDomain.hh](../include/clam/CrabDomain.hh) and add a new `constexpr Type`.

1. Pick the next available numeric id.
2. Add a new entry, for example:

```cpp
constexpr Type MY_NEW_DOM(N, "my-new-domain", "text description", true, false);
```

3. Add `MY_NEW_DOM` to `CrabDomain::List`.

Use the booleans consistently with existing domains:

   - unique numeric id,
   - short CLI name,
   - description,
   - relational/disjunctive flags.

In this branch, [lib/Clam/CrabDomainParser.cc](../lib/Clam/CrabDomainParser.cc) iterates over `CrabDomain::List`, so adding to `List` is what makes parsing work.

## 2) Add the option in ClamOptions.def

Edit [lib/Clam/ClamOptions.def](../lib/Clam/ClamOptions.def) in the `XClamDomain("crab-dom", ...)` option block.

Add one value entry such as:

```cpp
clEnumValN(clam::CrabDomain::MY_NEW_DOM, "my-new-domain", "Here description")
```

This is the same step highlighted by the wiki and is required for `clam --help` domain listing.

## 3) Create and register the domain implementation

In [lib/Clam/crab/domains/](../lib/Clam/crab/domains/):

1. Add implementation files (typically):
   - `my_new_domain.hh`
   - `my_new_domain.cc`
2. Update [lib/Clam/crab/domains/crab_domains.hh](../lib/Clam/crab/domains/crab_domains.hh) to include your new header.
3. Add a declaration in [lib/Clam/crab/domains/register_domains.hh](../lib/Clam/crab/domains/register_domains.hh):

```cpp
extern bool register_my_new_domain();
```

4. In your `.cc`, use the registration style used by other domains via macros from `crab_defs.hh`.

## 4) Register it in DomainRegistry

Edit [lib/Clam/RegisterAnalysis.cc](../lib/Clam/RegisterAnalysis.cc) and add your registration call inside `DomainRegistry::registerAllDomains()`:

```cpp
register_my_new_domain();
```

If this step is skipped, analysis fails at runtime with domain-not-found behavior.

## 5) Add it to ClamAnalysis build

Edit [lib/Clam/CMakeLists.txt](../lib/Clam/CMakeLists.txt) and add your source file:

```cmake
crab/domains/my_new_domain.cc
```

inside the `add_llvm_library(ClamAnalysis ...)` file list.

## 6) Expose it in py/clam.py

Edit `py/clam.py`, option `--crab-dom`:

1. Add the token to `choices=[...]`.
2. Add a help line in the domain description text.

This keeps Python frontend and native `clam` option set aligned.

## 7) Smoke test

After wiring, run:

```bash
CLAM_ROOT="<path-to-clam-root>"
"$CLAM_ROOT/bin/clam" --help | rg crab-dom
```

Confirm `my-new-domain` appears in help output.

Then run a simple benchmark:

```bash
"$CLAM_ROOT/bin/clam" INPUT.bc --crab-dom=YOUR_DOMAIN --crab-check=assert
```

If domain registration fails, CLAM usually reports that analysis for the selected domain was not found.

## 8) Practical checklist

- [ ] Added new domain token and metadata in `CrabDomain.hh`
- [ ] Added `clEnumValN(...)` entry in `ClamOptions.def`
- [ ] Added new domain files under `lib/Clam/crab/domains/`
- [ ] Included header in `crab_domains.hh`
- [ ] Added token to `py/clam.py --crab-dom` choices/help
- [ ] Added declaration in `register_domains.hh`
- [ ] Called `register_*_domain()` in `RegisterAnalysis.cc`
- [ ] Added `.cc` file to `lib/Clam/CMakeLists.txt` (`ClamAnalysis`)
- [ ] Verified command-line selection works
- [ ] Added at least one regression test
