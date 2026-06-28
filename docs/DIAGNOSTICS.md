# Diagnostics & error model

> **Responsibility of this file:** how mudraka surfaces errors, dropped samples,
> gaps, and stream health — uniformly across native / Python / WASM. The data-flow
> that produces these signals is in `ARCHITECTURE.md`; the call surface in
> `PUBLIC_API.md`.

---

## Principles (decision 2026-06-25)

1. **No exceptions on the hot path.** `feed`/`pull` are real-time and cross an FFI
   boundary (exceptions don't marshal cleanly, especially Embind/WASM). Health is
   expressed via **status values and counters**, not throws.
2. **A malformed frame is counted, not fatal.** A corrupt notification increments a
   counter and the stream continues — one bad frame never kills the stream.
3. **Cold-path errors may throw.** Construction/reconfiguration with an invalid
   profile (cold path) may raise.
4. **The library never logs.** No stdout/stderr writes. Logging policy belongs to the
   consumer; mudraka only reports via return values and `stats()`.

## Surfaces

### Per-call status
- `feed` → `DecodeResult { samples_written, std::optional<seq>, DecodeStatus }`
  where `DecodeStatus ∈ { ok, malformed, partial }` (`partial` = bytes held in the
  decoder carry buffer awaiting the next notification).
- `pull` → `PullResult { written, new_cursor, lost }` (`lost` = samples overwritten
  before the consumer read them).

### Pollable snapshot — `stats()`
A cheap, poll-anytime struct consolidating stream health:
```cpp
struct Stats {
  uint64_t total_written;       // cumulative samples pushed (per channel)
  uint64_t total_overwritten;   // ring loss (consumer fell behind)
  uint64_t malformed_frames;    // frames that failed to decode
  uint64_t gap_count;           // device-sequence discontinuities detected
  std::optional<uint32_t> last_seq;
  double   estimated_rate_hz;   // effective rate from the ClockModel regression
};
```

### Gaps
Detected by the engine/`ClockModel` from `DecodeResult::seq` discontinuities (kept
out of the decoder). Aggregated into `gap_count`; also reflected at read time via
`PullResult::lost`.

## Cross-target mapping
Status/counter structs map cleanly to each binding (numpy structured access / JS
objects); **no exception marshaling on the hot path.** This uniformity is itself a
reason for the status-not-throw choice.

## Decision log
- **2026-06-25** — Error model: no hot-path exceptions; malformed frames counted and
  survived; cold-path may throw; no library logging. Diagnostics via per-call status
  (`DecodeResult`/`PullResult`) + a pollable `stats()` snapshot (loss, malformed,
  gaps, last_seq, estimated rate).
