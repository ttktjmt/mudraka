# mudraka — documentation map

This directory holds the design and reference documents for **mudraka**, the C++
sEMG engine that decodes Mudra Link's raw SNC stream. Each file has a **single
responsibility**; use this map to find the right one.

| File | Responsibility | Status |
|------|----------------|--------|
| [CONTEXT.md](CONTEXT.md) | **Big picture.** What mudraka is / is not, the three projects and how they relate, repo strategy, source layout. **Read first.** | Active |
| [MUDRA_LINK_SIGNAL_SPEC.md](MUDRA_LINK_SIGNAL_SPEC.md) | **Reference.** Every signal the Mudra Link emits at the BLE/GATT level (facts about the device, not about mudraka). | Stable |
| [DECODE_VERIFICATION.md](DECODE_VERIFICATION.md) | **Strategy.** How we establish and *prove* that mudraka's SNC byte→sample decode is correct (the oracle, the lightweight harness, the gate). | Active |
| [CAPTURE_FIXTURE_FORMAT.md](CAPTURE_FIXTURE_FORMAT.md) | **Format.** On-disk layout of a recorded SNC test fixture (`capture.bin` / `index.json` / `meta.json` / `expected.jsonl`) and how it's produced. | Active |
| [PUBLIC_API.md](PUBLIC_API.md) | **Contract.** The public API surface — entry (`feed`) / exit (`pull`, `envelope`) semantics, threading rules, FFI boundary. | In progress |
| [ARCHITECTURE.md](ARCHITECTURE.md) | **Internals.** Module decomposition and data flow (the `IDecoder`→`SampleSink`→`RingBuffer` seam, where gap detection lives). | In progress |
| [CLOCK_MODEL.md](CLOCK_MODEL.md) | **Time & rate.** Timebase derivation (sample-index authority, drift correction) and the open true-sample-rate / viability question. | Active |
| [FFI_BINDINGS.md](FFI_BINDINGS.md) | **Boundary.** Binding tech (nanobind / Embind) and zero-copy marshaling, ownership, lifetime across Python & WASM. | In progress |
| [BUILD.md](BUILD.md) | **Build.** CMake topology (one core, three targets), packaging (scikit-build-core, Emscripten), dependency management. | Active |
| [DIAGNOSTICS.md](DIAGNOSTICS.md) | **Health.** Error model (status-not-throw) and the diagnostics surface (`stats()`, drops, gaps, malformed frames). | Active |
| [TEST_STRATEGY.md](TEST_STRATEGY.md) | **Verification.** Golden-driven tri-target decode parity, test depth, CI matrix. | Active |

> Documents not yet written (created as their design node is resolved):
> `SNC_PACKET_HYPOTHESIS.md` (the swappable layout guess — blocked on disassembly/capture).

## How these docs are maintained

We are designing mudraka node-by-node (a dependency-ordered design tree). **Every
time a node is resolved, the relevant doc here is updated** (or a new
single-responsibility file is added and listed above). Decisions carry a date so
the rationale is traceable.

## Project context

See [CONTEXT.md](CONTEXT.md) for the full picture (the three projects, mudraka's
responsibility boundary, distribution, repo strategy, source layout).
