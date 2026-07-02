# Build configuration

> **Responsibility of this file:** how the three targets (native / Python wheel /
> WASM npm) are built from one C++ core, and how dependencies are managed.

---

## CMake topology (decision 2026-06-25)

A **single root `CMakeLists.txt`**; the core is built once and reused by every
target.

```
mudraka_core            # STATIC/OBJECT lib · C++17 · zero third-party runtime deps
  ├─ MUDRAKA_BUILD_TESTS   → native tests link mudraka_core (decode + parity)
  ├─ MUDRAKA_BUILD_PYTHON  → nanobind target links mudraka_core (driven by scikit-build-core)
  └─ MUDRAKA_BUILD_WASM    → embind target links mudraka_core (emcmake / emcc)
```

Options are composable. **All bindings link the same `mudraka_core`** → this is what
makes the native/wasm/python **decode-parity** guarantee meaningful (one source of
truth, three thin shells).

## Packaging

- **Python wheel:** `scikit-build-core` backend in `pyproject.toml`; **cibuildwheel**
  matrix in CI (manylinux / macOS / Windows, incl. arm64).
- **WASM npm:** Emscripten toolchain (`emcmake`); the embind target emits `.js` +
  `.wasm`, bundled into the npm package in a separate packaging step.

## Dependency management (decision 2026-06-25)

- **Core has zero third-party deps** — C++17 stdlib only (keeps wheel/wasm small and
  portable).
- **Test-only deps via pinned `FetchContent`** (git tag pinned, reproducible, no
  extra contributor tooling):
  - **doctest** — lightweight C++ test framework.
  - **nlohmann/json** — read fixture `index.json` / `meta.json` / `expected.jsonl`
    **in tests only**. The core never depends on JSON (it reads `.bin` only).
- **nanobind** — obtained via pip and located through its CMake config
  (scikit-build-core convention).
- **Emscripten** — external toolchain, developer/CI-installed. **Pinned: 6.0.1**
  (via emsdk: `emsdk install 6.0.1 && emsdk activate 6.0.1`). CI installs the same tag.
  The WASM target sets `-sENVIRONMENT=web,worker,node` so one artifact serves both the
  browser (npm consumer) and the Node parity check (`tools/verify_wasm.mjs`).
- Reproducibility: all pins live in one place (consistent with the central-config
  ethos).

Rationale: vcpkg/Conan would be overkill for so few deps and would force extra
tooling on contributors; `FetchContent` is CMake-native and pin-reproducible.

## Release & versioning (decision 2026-07-02)

Tag-driven (`.github/workflows/release.yml`): pushing `vX.Y.Z` gates on the test
matrix, then publishes. **The git tag is the single source of version truth** —
stamped into `pyproject.toml` (`tools/stamp_version.py`) and `npm/package.json`
(`npm version`) at build time, so the wheel and npm package never drift. PyPI uses
cibuildwheel + **Trusted Publishing** (OIDC, no stored token); npm also uses
**Trusted Publishing** (OIDC — no `NPM_TOKEN`; provenance auto). License: **Apache-2.0**
(repo `LICENSE`).

**One-time setup (token-less, both OIDC):**
- **PyPI** — add a Trusted Publisher for project `mudraka`: repo `ttktjmt/mudraka`,
  workflow `release.yml`, environment `pypi`. (Supports a *pending* publisher, so this
  can be done before the first release.)
- **npm** — npm Trusted Publishing is configured in the **package's** settings, so the
  package must exist first. **Bootstrap once, locally:**
  ```bash
  source ~/emsdk/emsdk_env.sh
  emcmake cmake -S . -B build-wasm -DMUDRAKA_BUILD_WASM=ON -DMUDRAKA_BUILD_TESTS=OFF
  cmake --build build-wasm
  cp build-wasm/bindings/wasm/mudraka.js build-wasm/bindings/wasm/mudraka.wasm README.md npm/
  cd npm && npm login && npm publish --access public   # creates mudraka@0.1.0 (no provenance locally)
  ```
  Then on npmjs.com → package `mudraka` → Settings → **Trusted Publisher** → GitHub
  Actions: repo `ttktjmt/mudraka`, workflow `release.yml` (leave environment blank).
  After that, every `vX.Y.Z` tag publishes via OIDC with provenance — no local step.

## Decision log
- **2026-06-25** — Single-root CMake; `mudraka_core` reused by all targets via
  composable `MUDRAKA_BUILD_{TESTS,PYTHON,WASM}` options.
- **2026-07-02** — Tag-driven release; git tag = single version source (stamped into
  pyproject + package.json); PyPI Trusted Publishing + npm provenance; Apache-2.0.
- **2026-06-25** — Core zero-dep; test deps via pinned FetchContent (doctest,
  nlohmann/json); nanobind via pip; Emscripten external. No vcpkg/Conan.
