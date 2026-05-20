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
| Remote files | local filesystem only | **AWS S3** support via range requests (`s3://…`) |

---

## What's included

- **Core library** — a C++ implementation of the [BGEN format](http://www.well.ox.ac.uk/~gav/bgen_format/bgen_format_v1.2.html) (v1.1 and v1.2), usable from any C++17 project via `find_package(bgen)`
- **[bgenix](https://bitbucket.org/gavinband/bgen/wiki/bgenix)** — index and efficiently retrieve subsets of a BGEN file
- **[cat-bgen](https://bitbucket.org/gavinband/bgen/wiki/cat-bgen)** — concatenate BGEN files
- **[edit-bgen](https://bitbucket.org/gavinband/bgen/wiki/edit-bgen)** — edit BGEN file metadata
- **[rbgen](R/package/)** — R package (separate pixi environment, not bundled in the conda package)
- **[Example programs](example/)** — `bgen_to_vcf`, `count_alleles`, etc.
- **[AWS S3 support](#aws-s3-support)** — read BGEN files directly from S3 using `s3://bucket/key` URIs

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

To also enable S3 support, add `-DBGEN_WITH_S3=ON` and ensure the AWS SDK for C++ is findable:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBGEN_WITH_S3=ON
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

## AWS S3 support

The library can read BGEN files directly from AWS S3 without downloading them first.
It uses [HTTP range requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests) via the
[AWS SDK for C++](https://github.com/aws/aws-sdk-cpp), so only the blocks actually needed are fetched.

### Enabling

S3 support is opt-in.  Pass `-DBGEN_WITH_S3=ON` to CMake and make sure the AWS SDK is on your `CMAKE_PREFIX_PATH`:

```bash
# With pixi (installs aws-sdk-cpp automatically):
pixi run -e s3 configure
pixi run -e s3 build

# Or with CMake directly (requires aws-sdk-cpp on the prefix path):
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBGEN_WITH_S3=ON
cmake --build build --parallel
```

### Usage

Pass an `s3://bucket/key` URI anywhere a filename is accepted:

```cpp
// C++ API
auto view = genfile::bgen::View::create("s3://my-bucket/cohort.bgen");
while (view->read_variant(&snpid, &rsid, &chr, &pos, &alleles)) {
    view->read_genotype_data_block(setter);
}
```

```bash
# Command-line tools
bgenix -g s3://my-bucket/cohort.bgen -list
cat-bgen -g s3://my-bucket/part1.bgen s3://my-bucket/part2.bgen -og merged.bgen
```

### Authentication

Credentials are resolved by the AWS SDK's default provider chain in this order:

1. Environment variables — `AWS_ACCESS_KEY_ID` / `AWS_SECRET_ACCESS_KEY` / `AWS_SESSION_TOKEN`
2. `~/.aws/credentials` and `~/.aws/config`
3. EC2/ECS/EKS instance metadata

The AWS region is picked up from `AWS_DEFAULT_REGION` or `~/.aws/config`.
You can also set it programmatically when constructing a stream directly:

```cpp
#include "genfile/bgen/S3StreamBuf.hpp"
auto stream = genfile::bgen::make_s3_istream("s3://my-bucket/cohort.bgen", "eu-west-1");
```

### Tuning

The default read block size is **1 MB**.  For high-latency connections or very large genotype blocks,
construct an `S3StreamBuf` directly with a larger block size:

```cpp
auto buf = std::make_unique<genfile::bgen::S3StreamBuf>(
    "my-bucket", "cohort.bgen",
    /* region = */ "us-east-1",
    /* block_size = */ 8 * 1024 * 1024   // 8 MB
);
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
[Boost](https://www.boost.org/users/license.html) (Boost Software License),
[zstandard](https://github.com/facebook/zstd/blob/dev/LICENSE) (BSD), and optionally the
[AWS SDK for C++](https://github.com/aws/aws-sdk-cpp/blob/main/LICENSE) (Apache 2.0, only when built with `-DBGEN_WITH_S3=ON`).
