# KuberCore — Context Snapshot

**Date:** 2026-08-12  **Version:** v2.0  **Stage:** Part I, Stage 0 (build + measure)

> **This file is the session-recovery document.** If the working session is
> lost, reading this file alone should be enough to resume. Update it at the
> end of every session. Regenerate the file tree with `find`; never hand-edit it
> — hand-editing is exactly why v1.2 went stale.

---

## 1. What this is

**KuberCore** — a production-grade trading engine, and simultaneously a vehicle
for learning C++, Rust, and algorithms from first principles.

Renamed from "AlphaCore"/"Trading Systems Lab" on 2026-08-12. Kubera is the
Hindu god of wealth and treasurer of the gods.

**The central design claim:** a single matching engine core serves **both**
crypto spot and **binary prediction markets**. Normalize every NO order into its
YES equivalent (`BUY NO q @ p` ≡ `SELL q @ par−p`) and complementary matching —
minting a complete set from a YES buyer and a NO buyer with no resting
liquidity — becomes an *ordinary* price-time-priority match. The condition
`p_yes + p_no ≥ par` is algebraically identical to the normal crossing
condition. No second book, no branch in the match loop.

**Audience:** cold outreach to crypto startups and prediction-market companies;
C++/low-latency interviews secondary.

**Full plan:** `/home/sloth/.claude/plans/so-docs-has-my-immutable-quiche.md`

---

## 2. Next 3 concrete tasks

1. **Fix the 4 EventBus defects** (Stage 0, Day 2), one regression test each —
   see §7. The reentrancy UB at `event_bus.h:116` is the important one.
2. **Build `drishti/latency.h`** — pre-reserved sample vector, p50/p90/p99/
   p99.9/max, `clockOverheadNs()`. `steady_clock` for now; RDTSCP is Stage 4
   *on purpose*, so the two can be compared.
3. **`bench_dispatch`** — first real latency numbers on screen, with clock
   overhead printed and subtracted.

---

## 3. Build and test

```bash
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

| Preset | What | Use for |
|---|---|---|
| `dev` | Debug + ASan + UBSan | everything except numbers |
| `perf` | Release, `-O3 -march=native`, LTO | benchmarks only |

**Current status: green.** 7 tests, 0 failures, clean under ASan/UBSan.

### Benchmarking protocol — mandatory, this is a laptop

Numbers taken without these steps are noise, not data.

```bash
sudo cpupower frequency-set -g performance    # record the governor in output
taskset -c 2 ./build/perf/bin/<binary>        # pin; core 2 is a physical core
```
- Run 5×, report the **median run and the spread**.
- Print CPU model, governor, compiler, and flags in the results header.
- **Never benchmark under the `dev` preset** (sanitizers: 3–10×).

**Why it matters here:** 4 MB of *shared* L3 and a 1.4→3.7 GHz boost range mean
thermal throttling smears p99.9, and background load evicts the book from L3
mid-run. SMT siblings share L1/L2, so a co-scheduled thread pollutes the cache
directly.

---

## 4. Hardware

AMD Ryzen 5 PRO 3500U (Zen+) · 4 cores / 8 threads · L1d 32 KB per core ·
L2 512 KB · L3 4 MB shared · single NUMA node · Debian, kernel 7.1.3 ·
g++ 15.3 · clang 21.1 · CMake 4.3 · rustc 1.85

| Reachable locally | Not reachable |
|---|---|
| cache hierarchy, data-oriented design | AVX-512 (needs Zen 4) — **AVX2 only** |
| `rdtscp` (`constant_tsc` + `nonstop_tsc` present) | DPDK / RDMA / InfiniBand (needs Mellanox NIC) |
| `perf` / PMU counters (bare metal = full access) | NUMA / CCD topology (single node) |
| lock-free, false sharing, io_uring | |
| determinism, WAL, recovery, **active-active HA** (loopback) | |

**Not installed yet:** `perf` (`apt install linux-perf`), `liburing-dev`,
`clang-format`, `numactl`.

**Do not use the M1/M1 Pro for latency work.** Apple Silicon's timer runs at
24 MHz → ~41.7 ns granularity, so a 150 ns operation cannot be measured. Also
no `perf`, no io_uring, no real core pinning, NEON not AVX2.

---

## 5. File tree

Regenerate with:
`find . -type f -not -path './.git/*' -not -path './build/*' | sort`

```
.clang-format
.clang-tidy
.gitignore
CLAUDE.md                      # working rules: determinism, hot path, layering
CMakeLists.txt
CMakePresets.json              # dev (ASan/UBSan) | perf (O3 native LTO)
README.md                      # 627 lines, NOT yet rewritten
docs/
  ALPHACORE_REFERENCE.md       # append-only raw idea dump + Equiti JD (Appendix A)
  CONTEXT.md                   # this file
  LEARNING_CPP.md              # Part 1 (Event Bus); Parts 2+ not written
include/kuber/
  common.h                     # Timestamp, String, StringView, Id
  event_bus.h                  # header-only; 4 known defects, see §7
  logging.h                    # control-plane only; std::format based
src/core/
  logging.cpp                  # defines Logger's statics (the ODR lesson)
tests/
  CMakeLists.txt
  kuber_test.h                 # ~90-line harness, doctest-compatible names
  event_bus_test.cpp           # 7 tests
```

~688 lines of C++.

---

## 6. What works / stubbed / designed-only

**Works**
- Build configures, compiles, and tests green under ASan/UBSan.
- `EventBus` — type-erased pub/sub, C++20 concepts, per-type handler lists.
- `Logger` — level-filtered, `std::format`, control plane only.
- `kuber_test.h` — `CHECK`/`CHECK_EQ`/`REQUIRE`, real exit codes.

**Stubbed / thin**
- `common.h` — four type aliases; will be superseded by `core/types.h`.

**Designed only, no code**
- Everything in Stages 1–11: order book, matching engine, instruments,
  ledger, settlement, latency harness, flow generator, risk, WAL, HA, Rust.

**Deleted deliberately**
- `Config` + the `nlohmann/json` dependency (2026-08-12). Zero call sites, and
  its templates were defined in a `.cpp` with no explicit instantiation so they
  could never link. Returns properly in Stage 11 when it has real requirements.
- The CMake `docs`/doxygen target — no Doxyfile, doxygen not installed.

---

## 7. Known defects — EventBus (all verified, none fixed yet)

1. **Reentrancy UB, `event_bus.h:116`.** `publish()` range-for's over
   `list.handlers`. A handler calling `subscribe()` can reallocate the vector →
   dangling iterator; `unsubscribe()` invalidates it via `erase`. Both are
   routine in trading (a fill handler registering a follow-up; a one-shot
   handler removing itself). **Fix:** index-iterate against a size snapshot,
   tombstone on unsubscribe, compact only when the outermost `publish` returns.
2. **Const `getHandlerList()` (lines ~190) has no `end()` check** → UB on an
   unknown type. Zero callers. **Fix:** delete the overload.
3. **`clear()` resets `nextId_ = 1`** → recycled IDs, so a stale `HandlerId`
   can unsubscribe a *different* handler. **Fix:** drop the reset; pack the type
   index into the handle's high 32 bits (handle/generation pattern, reused for
   slab indices in Stage 1).
4. **Parallel vectors `ids` / `handlers`** can desync. **Fix:** one
   `struct Entry { HandlerId id; std::function<...> fn; }`.

**Bonus:** `handlers_` is an `unordered_map` keyed by a dense,
monotonically-assigned small integer — should be a `std::vector` indexed
directly. Removes a hash and a likely cache miss per publish, and improves
determinism.

---

## 8. Decisions, with reasoning

| Decision | Why |
|---|---|
| **KuberCore**, namespace `kuber` | Kubera = god of wealth. Also ends the three-way naming inconsistency (`trading-stack` / `TradingSystemsLab` / `namespace trading`) |
| Mythology in **namespaces and directories only** | `kuber::manthan::MatchingEngine` keeps identity without costing greppability. Type and file names stay descriptive |
| **Determinism committed from day one** | Replay, crash recovery, and active-active HA all fall out of it. ~free now, days of work to retrofit |
| **Flat array indexed by tick** for book levels | A binary market has 99 prices → whole book ≈ 3.2 KB, L1-resident. Crypto spot uses a price band, which real venues have anyway. `std::map` costs ~10 dependent cache misses to walk 10 levels |
| **C++ core, Rust offline** | Rust arrives Stage 7 as a replay tool over a binary log — separate process, **no FFI**. FFI is the worst first Rust experience: all `unsafe`, none of the guarantees. A read-only mmap is the most borrow-checker-friendly shape there is |
| **Plain aliases, not strong typedefs**, for `Price`/`Qty` | A strong `Price` needs ~15 operator overloads. `-Wconversion` plus a naming convention covers most of it. Revisit in Stage 5 |
| **Hand-rolled test harness** | Zero dependencies, teaches `__FILE__`/`#` stringification, and doctest trips `-Wpedantic -Werror` without a SYSTEM include. Macro names match doctest's so migration stays cheap |
| **`steady_clock` before RDTSCP** | The *comparison* in Stage 4 (~21 ns vs ~7 ns overhead) is a better lesson than starting with `rdtscp` |
| **`ALPHACORE_REFERENCE.md` left unsplit** | Owner's explicit call: it stays the append-only idea dump |
| **No third-party dependencies** | Currently zero. It builds anywhere with g++ and CMake, which matters for a repo strangers clone |

---

## 9. Learning matrix

| Topic | Status | Where |
|---|---|---|
| Concepts, templates, `std::function` | Done | Event Bus |
| Type erasure | Done | Event Bus |
| Designated initializers | Done | Event Bus |
| ODR / translation units | Done | `logging.cpp` statics |
| Preprocessor: `__FILE__`, `#` stringification | Done | `kuber_test.h` |
| Signed/unsigned conversion | Done | `-Wsign-conversion` on `unsubscribe` |
| Iterator invalidation | Next | EventBus defect 1 |
| RAII | Next | `ScopedTimer` |
| Cache lines, data-oriented design | Stage 1 | order book |
| Intrusive lists, free lists, slab pools | Stage 1 | `nidhi` |
| Bitmap scan (`countr_zero`) | Stage 1 | level occupancy |
| Static vs dynamic dispatch | Stage 1 | templated `FillSink` |
| Move semantics | Stage 1 | |
| Property testing / conservation laws | Stage 2 | ledger |
| RDTSCP, `perf`, PMU counters | Stage 4 | `drishti` |
| Atomics, memory ordering, false sharing | Stage 5 | SPSC ring |
| SIMD (AVX2) | Stage 6 | FIX parser |
| io_uring, `O_DIRECT` | Stage 7 | `chitragupta` |
| Rust ownership/borrowing | Stage 7 | `trikala` |
| WAL, snapshots, state hashing | Stage 9 | |

---

## 10. Open questions and deferred items

| Item | Trigger to revisit |
|---|---|
| Rent a Zen 4 bare-metal box (AVX-512, `isolcpus`, hugepages, DPDK) | **Stage 4.** Decision was "laptop only for now." Insist on bare metal — virtualized PMU is unreliable |
| Replace cancel-path `unordered_map` with open addressing | Stage 4, with a measured delta |
| Strong typedefs for `Price`/`Qty` | Stage 5 |
| `README.md` rewrite (627 → ~150 lines), Chapter 0 → `docs/JOURNAL.md` | Day 7 |
| GitHub Actions CI | Day 7 |
| Margin / health-factor / Monte-Carlo notes | Needs an oracle and mark-to-market — a different project. Reserved beyond `yama` |
| RFQ notes | Reserved as `prashna`; additive after Stage 2 |
