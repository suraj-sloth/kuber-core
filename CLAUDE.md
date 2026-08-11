# CLAUDE.md — working rules for KuberCore

**Read `docs/CONTEXT.md` first.** It is the living state of the project: what
works, what is stubbed, what is only designed, and the next three tasks. Update
it at the end of every session.

KuberCore is a production-grade trading engine and a learning vehicle at the
same time. Both halves are load-bearing — see "Teaching" below.

---

## Build and test

```bash
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

- **`dev`** — Debug + AddressSanitizer + UBSan. Use for everything except numbers.
- **`perf`** — Release, `-O3 -march=native`, LTO. The *only* preset whose timings mean anything.
- **Never benchmark under `dev`.** Sanitizers cost 3–10x.

---

## The three rule sets

### 1. Determinism — the keystone

> **Same binary + same input log → byte-identical output log.**

Replay, debugging, crash recovery, and active-active HA all depend on this one
property. Enforced by test.

- **No wall-clock reads inside the engine.** Time arrives as a field on the
  input event. A `now()` call in the engine makes replay impossible.
- **Never *iterate* an `unordered_map`/`unordered_set` where order affects
  output.** Bucket order varies with insertion history and stdlib version.
  Lookups are fine; iteration is not.
- **No pointer or address values in output or in decisions.** ASLR varies them.
- **No uninitialized padding in anything serialized.** `static_assert` the size
  and zero-initialize.
- **Randomness is seeded from the input log**, never from entropy.
- **One thread owns all engine state.** The matching core is a single-writer
  state machine — that is how real exchanges shard, so it is a design decision,
  not a limitation. Say so rather than apologising for it.

### 2. Hot path — what is banned

The match loop, order book mutation, and anything they call:

no allocation · no exceptions (return a `RejectReason`) · no logging ·
no `std::string` · no `iostream` · no virtual · no `std::function` ·
no `std::map` · no `shared_ptr` · no float · no wall-clock

Mark the loop `noexcept`. The data plane gets integer counters, not log lines.

### 3. Layering — the invariant that protects the architecture

`OrderBook` and `MatchingEngine` take **ticks and lots and nothing else**. No
`InstrumentSpec` member. No `if (kind == BinaryOutcome)`.

Instrument normalization sits *above* the engine; collateral sits *below* it.
**One instrument-aware branch in the match loop and the "same core serves crypto
spot and prediction markets" claim collapses** — that claim is the whole point
of the design.

Production features arrive through compile-time seams that currently inline to
nothing (`RiskPolicy = NoRisk`, `JournalSink = NoJournal`). Fill the seam; do
not refactor the call sites.

---

## Naming

Subsystems are named from Hindu mythology, **in namespaces and directories
only**. Type names and file names stay plainly descriptive, so a stranger can
still grep for what they expect:

```
include/kuber/manthan/engine.h   ->   kuber::manthan::MatchingEngine
```

| Namespace | Subsystem | Status |
|---|---|---|
| `narada` | event bus | implemented |
| `manthan` | matching engine + order book | next |
| `kosha` | accounts, balances, collateral | planned |
| `satya` | settlement / market resolution | planned |
| `chitragupta` | trade recorder, WAL, journal | planned |
| `nidhi` | slab pool, ring buffers | planned |
| `drishti` | metrics, latency, PMU | planned |
| `maya` | synthetic order-flow generator | planned |
| `yama` | risk limits, kill switch | planned |
| `dvara` | FIX gateway | planned |
| `trikala` | replay / backtest (Rust) | planned |
| `sesha` | HA replication | planned |

Reserved, no code: `garuda` (router), `akash` (market data), `prashna` (RFQ).

Every header opens with one descriptive line:
```cpp
// manthan/engine.h — Matching engine: price-time priority CLOB.
// (Manthan = the churning of the ocean.)
```

Full glossary in `docs/GLOSSARY.md`.

---

## Dependencies

**No new third-party dependency without a note in `docs/CONTEXT.md` saying what
it is for and what was considered instead.** The repo currently has zero. That
is a feature: it builds anywhere with g++ and CMake, which matters for a repo
strangers are asked to clone.

---

## Teaching

The person building this is deliberately learning C++, Rust, and algorithms
from first principles. So:

1. Explain what the machine is actually doing. Never write "as you know."
2. Show the naive version first, so the optimization has something to beat.
3. **Hand him the drill before the answer** — a signature, a failing test, and
   the invariant. Do not paste the solution first.
4. Then compare, and say specifically *why* the versions differ.
5. **Measure.** "Faster" without a number does not count.
6. Write it up in `docs/LEARNING_CPP.md`, in its existing
   *What It Is / Why It Matters / Plain English* format.

**No stage is done until its write-up is done.**

---

## Style

- C++20. `-Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion`, and
  the warnings propagate to tests via the `kuber_warnings` INTERFACE target.
- Prefer `enum class` + a `switch` with **no `default:`** — then `-Wswitch` under
  `-Werror` turns "I added an enumerator and forgot a case" into a compile error.
- Tests use `CHECK`/`CHECK_EQ`/`REQUIRE` from `tests/kuber_test.h`.
  **Never `assert()`** — it vanishes under `-DNDEBUG`, which is exactly the bug
  the old test suite had.
- Explicit source lists in CMake, never `file(GLOB)`.
