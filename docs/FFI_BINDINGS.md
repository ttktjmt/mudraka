# FFI & bindings

> **Responsibility of this file:** how the C++ core is exposed across the two FFI
> boundaries — Python (wheel) and WASM (npm) — including the binding technology,
> zero-copy marshaling, ownership, and object lifetime. The API *semantics* live in
> `PUBLIC_API.md`; this file is about *crossing the boundary*.
>
> Status: **in progress.**

---

## Python binding: **nanobind** (decision 2026-06-25)

Chosen over pybind11.

- We target **C++17**; nanobind requires C++17 (a fit, not a constraint).
- nanobind is pybind11's **successor** (same author): ~4× smaller binaries, faster
  compile and runtime, lower call overhead — relevant for a streaming hot path.
- First-class **`nb::ndarray`** for zero-copy numpy interop — directly serves the
  caller-provided-buffer contract (`feed` reads a uint8 view; `pull`/`envelope`
  write into caller numpy arrays; only the ring→array copy, no intermediates).
- Strong **scikit-build-core + cibuildwheel** integration (nanobind documents
  exactly this packaging path).
- Supports **free-threaded CPython (3.13+ nogil)** — future headroom for GIL-free
  `feed`/`pull` threading.
- Accepted downsides: newer/smaller ecosystem, Python 3.8+, recent CMake.

(No precedent inherited: `re/python_sdk` is pure ctypes, not a generated binding.)

The WASM boundary uses **Embind** (independent of this Python choice).

## Marshaling, ownership, lifetime (decision 2026-06-25)

Symmetric contract across both boundaries.

### Input — `feed(bytes)`
| Boundary | How | Copy? |
|----------|-----|-------|
| Python (nanobind) | `nb::bytes` / `nb::ndarray<uint8>` **read-only view** | none (read during the call) |
| WASM (Embind) | Web Bluetooth delivers a JS `ArrayBuffer`/`DataView` **outside** WASM heap, so the notification bytes are copied into the heap (`malloc` → `feed(ptr,len)`) | one small copy **per notification** (unavoidable; negligible) |

### Output — `pull` / `envelope`
- **Caller provides the destination buffer** on both sides; mudraka never retains a
  pointer to it past the call.
  - Python: caller's numpy `int32` arrays, written via writable `nb::ndarray` view
    (only the ring→array copy).
  - WASM: caller's region in WASM heap (`Int32Array` over `HEAP`); mudraka writes it;
    **JS reads the heap view directly (zero-copy on the JS side).**

### Ownership & lifetime
- Native `MudrakaStream` owns the heavy resources (pre-allocated ring, etc.).
  - Python: a nanobind class — **RAII, freed on GC**.
  - WASM: Embind objects are **not** GC'd → the lifetime contract **requires explicit
    `dispose()` / `.delete()`** (documented for consumers).
- Caller-provided output buffers are owned by the caller (numpy / JS).

### Threading
- Python: **release the GIL** during the native copy in `feed`/`pull` (safe — SPSC,
  no Python callbacks invoked from native), leaving room for separate driver threads.

## Decision log
- **2026-06-25** — Python binding = **nanobind** (over pybind11). WASM = Embind.
- **2026-06-25** — Symmetric marshaling: `feed` = read-only byte view (WASM copies one
  notification into heap, unavoidable); `pull`/`envelope` write into caller-provided
  buffers; native lifetime RAII (Python) / explicit `dispose` (WASM); GIL released
  during native copy.
