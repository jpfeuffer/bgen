# Contributing

## Prerequisites

Install [pixi](https://pixi.sh/latest/#installation). Everything else is managed automatically.

---

## Common commands

### Build & test (pixi — recommended)

```bash
pixi run test          # configure → build → ctest in one step
```

Individual steps:

```bash
pixi run configure     # cmake configure only
pixi run build         # cmake build only (after configure)
```

### Build & test (plain CMake)

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBGEN_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -V
```

### Build the conda package

```bash
pixi build             # produces a .conda artefact in the workspace
```

### Install locally

```bash
cmake --install build --prefix ~/.local   # or any other prefix
```

### R package

```bash
cd R/package
pixi run install       # R CMD INSTALL .
pixi run test          # run the test suite
```

---

## Project layout

```
CMakeLists.txt          root build config (CMake ≥ 3.25, C++17)
pixi.toml               pixi workspace + conda package definition
cmake/
  bgenConfig.cmake.in   package config template (find_package support)
  bgen_revision.hpp.in  version header template
genfile/include/        public headers for the core bgen library
src/                    core library sources
db/                     SQLite3 wrapper library (bgen::db)
appcontext/             CLI context helpers (bgen::appcontext)
apps/                   bgenix, cat-bgen, edit-bgen sources
example/                example programs and .bgen data files
test/                   unit tests (Catch2)
R/package/              R package (rbgen) with its own pixi.toml
.github/workflows/
  ci.yml                CI: cmake build + pixi build on ubuntu/macos
  cd.yml                CD: conda packages + binaries on version tags
```

---

## Branching & commits

- Branch names: `feature/<desc>`, `bugfix/<desc>`, `hotfix/<desc>`
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/):
  ```
  feat(bgenix): add --exclude-samples flag
  fix(cmake): handle SQLite3 target rename in CMake 4.x
  ```

## Releasing

Create a tag of the form `v1.2.3` — the CD workflow will automatically build
conda packages and release binaries and publish a GitHub Release.
