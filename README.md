# BGEN — next-generation fork

> **This is a modernised community fork of the [original BGEN reference implementation](https://bitbucket.org/gavinband/bgen)** by Gavin Band & Jonathan Marchini.
> It retains full format compatibility while replacing the legacy build system with **modern CMake**, adding **pixi / conda-forge packaging**, and upgrading the codebase to **C++17**.

[![CI](https://github.com/jpfeuffer/bgen/actions/workflows/ci.yml/badge.svg)](https://github.com/jpfeuffer/bgen/actions/workflows/ci.yml)

---

## What's new in this fork

| Area | Before (upstream) | This fork |
|---|---|---|
| Build system | `waf` (Python-based) | CMake ≥ 3.25 with modern targets & install/export |
| C++ standard | C++11 | **C++17** |
| Package manager | manual / system deps | **[pixi](https://pixi.sh)** (conda-forge, reproducible) |
| CI/CD | Bitbucket Pipelines | **GitHub Actions** — cmake + pixi + conda package |
| Conda package | none | `pixi build` produces a `.conda` artefact |
| `std::auto_ptr` | used throughout | replaced with `std::unique_ptr` |
| `std::random_shuffle` | used in tests | replaced with `std::shuffle` (C++17) |
| CMake package config | none | `find_package(bgen)` works for downstream projects |
| R package | bundled in build dir | self-contained in `R/package/` with its own `pixi.toml` |

---

## What's included

- **Core library** — a C++ implementation of the [BGEN format](http://www.well.ox.ac.uk/~gav/bgen_format/bgen_format_v1.2.html) (v1.1 and v1.2), usable from any C++17 project via `find_package(bgen)`
- **[bgenix](https://bitbucket.org/gavinband/bgen/wiki/bgenix)** — index and efficiently retrieve subsets of a BGEN file
- **[cat-bgen](https://bitbucket.org/gavinband/bgen/wiki/cat-bgen)** — concatenate BGEN files
- **[edit-bgen](https://bitbucket.org/gavinband/bgen/wiki/edit-bgen)** — edit BGEN file metadata
- **[rbgen](R/package/)** — R package (separate pixi environment, not bundled in the conda package)
- **[Example programs](example/)** — `bgen_to_vcf`, `count_alleles`, etc.

---

## Quick start

### With pixi (recommended)

[Install pixi](https://pixi.sh/latest/#installation), then:

```bash
git clone https://github.com/YOUR_ORG/bgen.git
cd bgen
pixi run test          # configure, build, and run all tests
```

To build a conda package locally:

```bash
pixi build
```

### With CMake directly

Requires: CMake ≥ 3.25, a C++17 compiler, Boost ≥ 1.84, zlib, zstd, SQLite3, Ninja.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBGEN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Install to a prefix:

```bash
cmake --install build --prefix /usr/local
```

This installs the `bgenix`, `cat-bgen`, and `edit-bgen` binaries, the library, headers, and a
`bgenConfig.cmake` so downstream projects can do:

```cmake
find_package(bgen REQUIRED)
target_link_libraries(my_target PRIVATE bgen::bgen)
```

---

## R package

The R package lives in [`R/package/`](R/package/) and has its own pixi environment:

```bash
cd R/package
pixi run install   # R CMD INSTALL .
pixi run test      # run the test suite
```

---

## Citing BGEN

If you use this library, its tools, or example programs, please cite the original authors:

> Band, G. and Marchini, J., *"BGEN: a binary file format for imputed genotype and haplotype data"*, bioRxiv 308296; doi: https://doi.org/10.1101/308296

---

## License

Released under the [Boost Software License v1.0](LICENSE_1_0.txt) — a permissive open-source license compatible with many others.

This repository also uses [SQLite](https://www.sqlite.org/copyright.html) (public domain),
[Boost](https://www.boost.org/users/license.html) (Boost Software License), and
[zstandard](https://github.com/facebook/zstd/blob/dev/LICENSE) (BSD).
