# Building Outside Docker

This page describes how to build Clam/Crab natively on a Linux host.

## Platform

Tested and supported: **Ubuntu 22.04 (x86\_64)**.

Other Debian/Ubuntu variants may work if the same LLVM 14 packages are available. Other Linux distributions require adapting the package-manager commands.

## Requirements

The following versions were used to produce the artifact and are known to work. Older versions may work but are untested.

| Component | Minimum / Tested version | Notes |
|---|---|---|
| **CMake** | ≥ 3.3 (tested: 3.24) | `cmake_minimum_required(VERSION 3.3)` in `CMakeLists.txt` |
| **Ninja** | tested 1.10.1 | Used as the CMake generator (`-GNinja`) |
| **LLVM / Clang** | `14.0.0` | Ubuntu package `llvm-14` (`14.0.0-1ubuntu1.1`) is recommended; alternatively build LLVM 14 from the [llvm-project](https://github.com/llvm/llvm-project) repository and point `-DLLVM_DIR` at the install tree |
| **GCC (multilib)** | system default on Ubuntu 22.04 | Required by Crab sub-dependencies |
| **GMP** | system (`libgmp-dev`) | Required by Crab |
| **MPFR** | system (`libmpfr-dev`) | Required when `-DCRAB_USE_ELINA=ON` or `-DCRAB_USE_APRON=ON` |
| **Boost** | ≥ 1.65 (artifact uses 1.84.0) | Required by Clam and by sea-dsa. Built from source in these instructions; a system Boost installation is also accepted via `-DCUSTOM_BOOST_ROOT` |
| **Python** | ≥ 3.6 | `clam.py` driver and test scripts |
| **opam / OCaml** | optional | Only needed if building with Elina's OCaml bindings |

## 0. Get the source code

Before copying, remove the `build/` directory inside the container to reduce transfer size:

```bash
docker run -it <image-name> bash
# inside the container:
rm -rf /home/agent/clam/build
exit
```

Then copy the source tree to your host:

```bash
# Option A – copy out of the stopped/running Docker container
docker cp <container-id>:/home/agent/clam /destination/clam

# Option B – extract from a provided archive
tar xf clam.tar.gz
```

## 1. System dependencies

```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install -y \
  build-essential git pkg-config \
  libgmp-dev libmpfr-dev \
  cmake cmake-data unzip zlib1g-dev \
  ninja-build \
  libclang-dev libclang-cpp-dev \
  gcc-multilib libssl-dev \
  less curl wget rsync m4
```

## 2. LLVM 14

```bash
sudo apt-get install -y clang-14 lldb-14 lld-14 clang-format-14
```

Verify:

```bash
clang-14 --version
```

## 3. Python packages

```bash
sudo apt-get install -y python3 python3-pip python3-setuptools python3-wheel
pip3 install lit OutputCheck PyYAML matplotlib pandas ipython notebook
```

## 4. Boost 1.84 (from source)

The build uses Boost 1.84.0 installed to `/opt/boost`. If you have a suitable system Boost, adjust `-DCUSTOM_BOOST_ROOT` accordingly.

```bash
cd /tmp
curl -sSOL https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
tar xf boost_1_84_0.tar.gz
cd boost_1_84_0
./bootstrap.sh --prefix=/opt/boost
./b2 -j$(nproc) install
```

## 5. Configure

```bash
cd clam
mkdir build && cd build

cmake .. -GNinja \
  -DCUSTOM_BOOST_ROOT=/opt/boost \
  -DCLAM_ENABLE_ASAN=OFF \
  -DCRAB_USE_APRON=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLLVM_DIR=/usr/lib/llvm-14/lib/cmake/llvm \
  -DCMAKE_INSTALL_PREFIX=run \
  -DCMAKE_CXX_COMPILER=clang++-14 \
  -DCMAKE_C_COMPILER=clang-14 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -DCRAB_USE_LDD=ON \
  -DCRAB_USE_ELINA=ON \
  -DCLAM_USE_DBM_SAFEINT=OFF \
  -DCLAM_USE_DBM_BIGNUM=OFF \
  -DSEA_BUILD_IDSA=ON
```

## 6. Build

LDD and Elina are external dependencies fetched and built in stages:

```bash
# Stage 1: build LDD
cmake --build . --target ldd
cmake ..

# Stage 2: build Elina
cmake --build . --target elina
cmake ..

# Stage 3: full install
ninja install
```

After a successful build the installed tree is at `build/run/`.

## 7. Set environment

```bash
export CLAM_ROOT=$(pwd)/run   # run from inside build/
export PATH=$CLAM_ROOT/bin:$PATH
```

Verify:

```bash
clam --version
clam-pp --version
```

## Key CMake options

| Option | Value used | Effect |
|---|---|---|
| `LLVM_DIR` | `/usr/lib/llvm-14/lib/cmake/llvm` | Path to LLVMConfig.cmake; adjust if LLVM is installed elsewhere |
| `CMAKE_CXX_COMPILER` / `CMAKE_C_COMPILER` | `clang++-14` / `clang-14` | Must match the LLVM version above |
| `CMAKE_INSTALL_PREFIX` | `run` | Installs the tool tree into `build/run/`; set `CLAM_ROOT` to this path |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | Release build with debug symbols; use `Debug` for development |
| `CUSTOM_BOOST_ROOT` | `/opt/boost` | Prefix of a custom Boost installation; omit to use the system Boost |
| `CRAB_USE_LDD` | ON | Enable the **Boxes** domain (requires LDD/CUDD, fetched automatically) |
| `CRAB_USE_ELINA` | ON | Enable **Elina** domains (Octagon, Polyhedra); fetches and builds Elina. **Incompatible with `CRAB_USE_APRON=ON`** |
| `CRAB_USE_APRON` | OFF | Enable the **Apron** library domains instead of Elina. **Incompatible with `CRAB_USE_ELINA=ON`** |
| `SEA_BUILD_IDSA` | ON | Build IDSA (inductive data-structure analysis) support |
| `CLAM_USE_DBM_SAFEINT` | OFF | DBM domain with safe (checked) integer arithmetic |
| `CLAM_USE_DBM_BIGNUM` | OFF | DBM domain with arbitrary-precision integers |
| `CLAM_ENABLE_ASAN` | OFF | Address sanitizer; enable only for debugging (large overhead) |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | ON | Emit `compile_commands.json` for IDE/tooling integration |

## Troubleshooting

- **`LLVM_DIR` not found**: confirm `/usr/lib/llvm-14/lib/cmake/llvm/LLVMConfig.cmake` exists. If LLVM is installed elsewhere, adjust the path.
- **Boost not found**: pass the actual prefix with `-DCUSTOM_BOOST_ROOT=<path>`.
- **Elina/LDD download fails**: the build fetches them via `FetchContent`; ensure the host has outbound HTTPS access during configure.
