# Trading Systems Lab – Context Snapshot (v1.2)

**Date:** August 3, 2026
**Status:** Implementation started — Event Bus complete

Developer skills: assume new to in C++ and rust. explain everything in detail so as to teach as while implementing.

---

## Project Definition

A modular framework simulating major components found in real trading systems. The project is a **learning platform** with production-quality architecture, not a single application.

---

## Current State

### Files
```
TradingSystemsLab/
├── README.md                     # Full project planning
├── CMakeLists.txt                # Build config (C++20, tests, install)
├── include/trading/
│   ├── common.h                  # Types: Timestamp, String, Id
│   ├── config.h                  # Configuration system
│   ├── logging.h                 # Logger with macros
│   └── event_bus.h               # Event Bus (header-only, C++20 concepts)
├── tests/
│   └── event_bus_test.cpp        # 7 passing tests
└── docs/
    ├── CONTEXT.md                # This file
    └── ALPHACORE_REFERENCE.md    # IoC patterns, zero-copy, slab allocators
```

### What We Built: Event Bus

**C++ Concepts Taught:**
- `concept VoidCallable` — compile-time callable validation
- `concept IsEvent` — constrains event types to plain structs
- Variadic templates — any event shape
- `std::function` — type-erased callbacks
- `std::unique_ptr` — ownership semantics
- Virtual dispatch for type erasure
- Designated initializers (C++20)
- `requires` clauses

**Design:**
- Header-only (templates must be in headers)
- Type-safe: each event type has independent handler list
- `HandlerId` strong type prevents accidental misuse
- `publish()` passes events by const reference (zero-copy)

**Test Results:** 7/7 passing

---

## C++ Learning Matrix (Progress)

| C++ Topic         | Status | Project Integration  |
| ----------------- | ------ | -------------------- |
| Concepts          | Done   | Event Bus            |
| Templates         | Done   | Event Bus            |
| std::function     | Done   | Event Bus            |
| RAII              | Next   | Resource management  |
| Move semantics    |        | Order lifecycle      |
| CRTP              |        | Strategy framework   |
| Memory pools      |        | Order allocation     |
| Atomics           |        | Lock-free queues     |
| Cache locality    |        | Matching engine      |
| Custom allocators |        | Memory subsystem     |
| Threading         |        | Market data pipeline |

---

## Next Steps

- Phase 2: Config, Logging, Metrics
- Phase 3: OMS, Risk Engine, Market Data Engine, Smart Order Router
