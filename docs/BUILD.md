# Build configuration

> **Responsibility of this file:** how the three targets (native / Python wheel /
> WASM npm) are built from one C++ core, and how dependencies are managed.

---

## CMake topology (decision 2026-06-25)

A **single root `CMakeLists.txt`**; the core is built once and reused by every
target.

```
mudraka_core            # STATIC/OBJECT lib · C++17 · zero third-party runtime deps
  ├─ MUDRAKA_BUILD_TESTS   → native tests link mudraka_core (oracle gate + parity)
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
- **Emscripten** — external toolchain, developer/CI-installed; its version is pinned
  in one place.
- Reproducibility: all pins live in one place (consistent with the central-config
  ethos).

Rationale: vcpkg/Conan would be overkill for so few deps and would force extra
tooling on contributors; `FetchContent` is CMake-native and pin-reproducible.

## Decision log
- **2026-06-25** — Single-root CMake; `mudraka_core` reused by all targets via
  composable `MUDRAKA_BUILD_{TESTS,PYTHON,WASM}` options.
- **2026-06-25** — Core zero-dep; test deps via pinned FetchContent (doctest,
  nlohmann/json); nanobind via pip; Emscripten external. No vcpkg/Conan.
