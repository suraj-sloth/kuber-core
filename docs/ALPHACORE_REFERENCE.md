LLM - DONT EDIT THIS, THIS HAS MY RAW IDEAS
DONT REUSE ANY OF THE NAMES USED HERE
THIS FILE HAS ALL THE RAW IDEAS AND CONTEXT THAT I PLAN TO HAVE
FORMALIZED PLANNING STAYS in CONTEXT.md
# AlphaCore Trading Engine Architecture Reference

**Source:** HFT Engine Architecture Guide
**Status:** Reference material — design patterns to absorb into Trading Systems Lab

---

## Core Philosophy

**Framework vs. App Logic (Inversion of Control)**

- The **framework** provides all low-level plumbing: networking, memory management, state machines, logging, threading
- Your **app logic** is a small module that plugs into the framework as a callback
- The framework calls you; you do not call the framework

```
Framework (Kitchen)          Your Code (Recipe)
───────────────────          ──────────────────
Network I/O & Parsing        PreprocessorImpl (validation)
Order State Machine          SimpleAllocator (routing decisions)
Risk Check Engine            Domain-specific rules
Outbound Transport           Exchange-specific adapters
Memory / Logging / Timers
```

---

## The Polling Heartbeat

- System runs a non-blocking `poll()` loop millions of times per second
- Never uses `sleep` — context switches cost 2-10 microseconds
- Pins execution loop to isolated CPU core
- 100% CPU spin; maximizes power to minimize latency

---

## 7-Step Order Lifecycle

| Step | Module | Responsibility |
|------|--------|----------------|
| 1 | `msg/ClientRequest.C` | Parse inbound FIX/binary packets |
| 2 | `om/PreprocessorImpl.C` | Validate order against risk rules |
| 3 | `om/ClientOrder.C` | Track lifecycle: New → Ack → Fill/Cancel |
| 4 | `model/SimpleAllocator.H` | Core routing decision |
| 5 | `em/ExchangeProfile.C` | Venue session state, hours, paths |
| 6 | `protocol/OutboundPacketFactory.C` | Build outbound wire messages |
| 7 | `app/ApplicationHeartbeat.C` | System init + continuous polling loop |

---

## Production Directory Layout

```
simple_trading_engine/trunk/src/
├── BUILD
├── trading_app.bzl
├── lib/nocopy/core/
│   ├── config/EngineConfig.H
│   ├── em/ExchangeProfile.C
│   ├── model/SimpleAllocator.H        ← routing brain
│   ├── msg/ClientRequest.C
│   ├── om/ClientOrder.C
│   ├── om/PreprocessorImpl.C          ← validation rules
│   ├── protocol/OutboundPacketFactory.C
│   └── app/ApplicationHeartbeat.C     ← polling loop
├── unit_tests/
└── deployment_cfg/
```

---

## C++ Performance Patterns

| Concept | Mechanism | Purpose |
|---------|-----------|---------|
| `ALLOCATOR_SETUP(...)` | Pre-processor macro | Eliminates runtime boilerplate |
| `template <typename T>` | Compile-time polymorphism | No vtable overhead |
| `auto&` | Pass-by-reference | Zero-copy, no cache misses |
| `auto*` | Raw pointer | Direct memory manipulation |
| `::instance()` | Singleton pattern | Single global access point |
| `ReturnCode::SUCCESS` | Typed enum | No string comparison |

---

## Low-Latency Techniques

### 1. Zero-Copy Serialization
- Network card (Solarflare/OpenOnload) writes frames directly to user-space memory
- Application parses fields from live network buffer — no intermediate copies

### 2. Slab Allocators
- Pre-allocate huge memory blocks at boot
- During trades, pull pre-sliced tracks from local block
- Zero runtime allocation (no malloc/new during active trading)

### 3. Core Pinning & Thread Isolation
- Pin critical thread to isolated physical core via affinity registers
- Thread spins at 100% — OS cannot schedule competing processes
- Eliminates context-switch penalties

### 4. Non-Blocking Polling
- Infinite loop queries hardware rings for signals
- Never yields or sleeps
- Trades power consumption for latency

---

## Allocator Code Patterns

### Single-Venue Routing
```cpp
template <typename ExecutionState>
common::ReturnCode allocate(ExecutionState& state, AllocatorArgument& arg) {
    auto& clientOrder = arg.clientOrder;
    auto& qtyToAllocate = *arg._quantityToAllocate;

    if (qtyToAllocate.targetQty() <= QuantityZero) {
        return common::ReturnCode::KEEP_CURRENT_ALLOCATION;
    }

    const auto exchangeId = em::ExchangeIndex::fromStringRef("MAIN_EXCH");
    auto& exchange = em::ExchangeManager::instance().getExchange(exchangeId);
    auto* entry = clientOrder.getAllocationEntry(exchange, SimpleAllocator::id);

    entry->Side = clientOrder.target()->Side;
    entry->OrdType = clientOrder.target()->OrdType;
    entry->OrdQty = qtyToAllocate.targetQty();

    arg._allocationEntries->add(exchangeId, entry);
    return common::ReturnCode::SUCCESS;
}
```

### Multi-Venue Split (Remainder Trick)
```cpp
const int halfQuantity = order.quantity / 2;              // 101 / 2 = 50
const int remainderQuantity = order.quantity - halfQuantity; // 101 - 50 = 51

// Atomic failure: if any venue unknown, reject entire order
// All-or-nothing, never half-execute
```

### Application Heartbeat
```cpp
void processApplicationSpecificLogic() {
    while (true) {
        context().marketDataEventManager().poll();  // price streams
        ReflexOrderManager::instance().poll();      // order state
    }
}
```

---

## Key Takeaways for Trading Systems Lab

1. **IoC is mandatory** — framework owns the loop, modules plug in as callbacks
2. **Zero-copy everywhere** — references, not copies; parse from network buffers
3. **Pre-allocate everything** — slab allocators, no runtime malloc
4. **Pin threads** — isolated cores, no OS scheduling interference
5. **Non-blocking polling** — never sleep, spin forever
6. **Atomic failure** — if any venue fails, reject entire order
7. **Typed enums** — no string comparisons in hot paths
8. **Templates over virtuals** — compile-time polymorphism, no vtable lookup

## More ref
## The Core Gist

This guide explains how to build a high-frequency trading engine from scratch using an Inversion of Control architecture. Instead of writing everything from the ground up, a high-performance framework handles all low-level plumbing (networking, memory, logging) while your custom logic simply plugs in as a module to make the core routing and validation decisions. The entire system is engineered for sub-microsecond execution by strictly avoiding data copying, avoiding mid-trade memory allocations, and using high-speed polling loops.

---

## High-Level Architecture & Workflow

```unset
   Inbound FIX Client Order
              │
              ▼
    [ OrderManager ] ──────► (Framework: Handles network I/O & parsing)
              │
              ▼
   [ PreprocessorImpl ] ────► (Your Logic: Validates order criteria)
              │
              ▼
     [ CheckCenter ] ───────► (Framework: Performs pre-trade risk checks)
              │
              ▼
    [ SimpleAllocator ] ────► (Your Logic: Decides where to route shares)
              │
              ▼
 [ ExchangeManager -> NYSE ] ─► (Framework: Outbound routing to the venue)
```

1. Framework vs. App Logic: The underlying framework (AlphaCore) acts like a fully equipped restaurant kitchen. Your specific trading app (SimpleAllocator) is just a recipe card you drop into it.
2. Inversion of Control: You do not write a `main()` loop that calls the framework. The framework runs a continuous pipeline and automatically triggers your `allocate()` function the exact millisecond an order arrives.
3. The Polling Heartbeat: The system runs a non-blocking `poll()` loop millions of times per second to check for new market data or order updates. It never uses "sleep" commands, as sleeping costs microseconds that a high-frequency trading firm cannot afford.

---

## Core Code Module: The Allocator

The "brain" of the application determines how an incoming order is carved up and targeted to external execution venues.

## Single-Venue Routing

When routing to a single venue like MainExchange, the logic extracts the customer order details, verifies the target quantity is valid, and registers a single execution slice containing the order side, price, type, and volume.

## Multi-Venue Split Routing

When splitting an order across multiple venues (e.g., MainExchange and AltExchange), the logic uses an explicit integer remainder calculation to prevent losing fractional shares:

- The Remainder Trick: Integer division drops decimals (e.g., `101 / 2 = 50`). To prevent losing a share, the second venue receives `Total - FirstVenue` (e.g., `101 - 50 = 51`).
- Atomic Failure: If any target venue is unknown or offline, the entire order is rejected immediately. Trading engines strictly enforce an "all-or-nothing" rule rather than executing a half-broken trade.

---

## C++ Performance Superpowers

To maintain sub-microsecond speeds, the codebase relies on precise language mechanics:

- Pass-by-Reference (`auto&`): Creates a direct alias to data in memory rather than duplicating it. Copying data takes time and creates cache misses.
- Slab Allocators: Memory chunks are pre-allocated in large blocks during system startup. The application never requests memory from the Operating System during an active trade.
- Zero-Copy Serialization: Network messages are read and parsed directly from the network buffer without moving them across intermediate buffers.
- The Singleton Pattern: Critical system utilities (like the `ExchangeManager`) exist as exactly one globally accessible instance to minimize lookup overhead.

---

If you want to keep tweaking the system, let me know if you would like to:

- See the full C++ code implementation for the multi-venue split allocator.
- Walk through an automated test script simulating an exchange rejection.
- Deep dive into how a Slab Allocator avoids memory fragmentation.
  
  ## 1. The Architectural Philosophy: Framework vs. App Logic

Building a high-frequency trading (HFT) engine requires a complete mental shift in how software is organized. In standard application development, your code controls the execution flow, calling third-party libraries when it needs help. In ultra-low-latency engineering, this relationship is entirely inverted.

```unset
                                      ALPHACORE FRAMEWORK BOUNDARY
┌────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                                │
│  ┌───────────────────┐      ┌────────────────────┐      ┌───────────────┐      ┌────────────┐  │
│  │  Network I/O &    │ ───► │ ClientOrderManager │ ───► │  CheckCenter  │ ───► │ Outbound   │  │
│  │  FIX Parsing      │      │ (State Tracking)   │      │ (Risk Checks) │      │ Transport  │  │
│  └───────────────────┘      └────────────────────┘      └───────────────┘      └────────────┘  │
│                                       │                                              ▲         │
└───  Inversion of Control (IoC)  ──────┼──────────────────────────────────────────────┼─────────┘
                                        │ (Framework Calls You)                        │ (You Register Slice)
                                        ▼                                              │
                              ┌────────────────────────────────────────────────────────┴┐
                              │               YOUR CUSTOM STRATEGY LAYER               │
                              │                  [SimpleAllocator]                      │
                              └─────────────────────────────────────────────────────────┘
```

## The Architecture Analogy

Think of the foundational infrastructure (AlphaCore) as a world-class restaurant kitchen. It provides the heavy plumbing, gas lines, automated refrigeration, and a team of rapid runners. You do not redesign the stoves every time you want to add a dish to the menu.

Instead, you write a small, laser-focused module (SimpleAllocator) which acts like a recipe card dropped into the framework slot. The kitchen handles the logistics of getting materials in and out; your code is solely responsible for the culinary decisions.

## Inversion of Control (IoC)

When examining an HFT codebase, you will notice something peculiar: there is no obvious `main()` function driving the trading workflow. Instead, the framework runs a highly optimized, non-blocking pipeline.

When a buy or sell order hits the network card, the framework intercepts the packet, parses the raw protocol bytes, tracks the order state, and at the precise microsecond everything is prepared, it reaches out and invokes your strategy's `allocate()` method. You do not call the framework; the framework calls you.

---

## 2. High-Performance Design Skeletons & Code Layouts

To maintain single-digit microsecond or nanosecond execution profiles, an engine cannot be structured as a single, massive monolithic file. Monoliths break hardware cache lines and are impossible to optimize effectively. Instead, the architecture isolates responsibilities into strict, bounded contexts.

## Production Directory Blueprint

Below is the structural layout of how a professional, zero-copy trading application is organized from scratch using modern build tools like Bazel:

```text
simple_trading_engine/trunk/src/
├── BUILD                             # Bazel build instructions declaring compile targets
├── trading_app.bzl                   # Compiler macros defining release & optimization artifacts
├── lib/nocopy/core/
│   ├── config/                       # Configuration matrices and environmental profiles
│   │   └── EngineConfig.H
│   ├── em/                           # Exchange Management: Hours, session state, network paths
│   │   └── ExchangeProfile.C
│   ├── model/                        # The Strategy Domain: Custom routing & allocation brains
│   │   └── SimpleAllocator.H        # <--- The Core Allocation Core
│   ├── msg/                          # Protocol Layer: Deserializes inbound FIX/binary messages
│   │   └── ClientRequest.C
│   ├── om/                           # Order Management: Keeps track of filled/canceled states
│   │   ├── ClientOrder.C
│   │   └── PreprocessorImpl.C       # <--- Inbound validation rules
│   ├── protocol/                     # Formatting engine for outbound execution requests
│   │   └── OutboundPacketFactory.C
│   └── app/                          # Engine Application Lifecycle management
│       └── ApplicationHeartbeat.C    # <--- Continuous polling loops & startup routine
├── unit_tests/                       # Automated edge-case simulation suites
└── deployment_cfg/                   # Production environment deployment manifests
```

## Mapping Jobs to Code Boundaries

Every component in this directory map correlates directly to a step in the lifecycle of an order:

|Functional Responsibility|Code Context|Primary Class/Component|
|---|---|---|
|Decode Wire Packets|`msg/`|`ClientRequest.C`|
|Enforce Validation Rules|`om/`|`PreprocessorImpl.C`|
|Track Lifecycle State|`om/`|`ClientOrder.C`|
|Determine Routing & Splits|`model/`|`SimpleAllocator.H`|
|Maintain Session Specs|`em/`|`ExchangeProfile.C`|
|Construct Wire Output|`protocol/`|`OutboundPacketFactory.C`|
|Keep the Engine Ticking|`app/`|`ApplicationHeartbeat.C`|

---

## 3. Deep-Dive Code Implementations

## Module A: The Core Single-Venue Allocator (`SimpleAllocator.H`)

This module handles the fundamental routing scenario: accepting an order from a client, verifying it contains volume, targeting a single major exchange, and building an execution slice.

```cpp
// SimpleAllocator.H
#ifndef SIMPLE_ALLOCATOR_H
#define SIMPLE_ALLOCATOR_H

// Framework boilerplate suppression macros
ALLOCATOR_SETUP( SimpleAllocator, NOCOPY_NO_STATE, NOCOPY_NO_SUBALLOCATOR )

template <typename ExecutionState>
common::ReturnCode allocate(ExecutionState& state, AllocatorArgument& arg)
{
    // Extract a direct reference to the customer's intent. 
    // No memory allocation or data copying occurs here.
    auto& clientOrder = arg.clientOrder;

    // Dereference the incoming target quantity pointer safely
    auto& qtyToAllocate = *arg._quantityToAllocate;
    
    // Low-overhead guard clause: drop out immediately if volume is zero
    if (qtyToAllocate.targetQty() <= QuantityZero)
    {
        return common::ReturnCode::KEEP_CURRENT_ALLOCATION;
    }

    // Lookup the static exchange index identifier for the primary venue
    const auto exchangeId = em::ExchangeIndex::fromStringRef(common::LightStringRef("MAIN_EXCH"));
    
    // Acquire a reference to the global singleton managing active exchange state
    auto& exchange = em::ExchangeManager::instance().getExchange(exchangeId);

    // Instantiate an execution "slice" mapping directly into pre-allocated memory
    auto* entry = clientOrder.getAllocationEntry(exchange, SimpleAllocator::id);
    
    // Blit structural attributes across via internal references
    entry->Side         = clientOrder.target()->Side;         // BUY or SELL
    entry->OrdType      = clientOrder.target()->OrdType;      // MARKET, LIMIT, etc.
    entry->TimeInForce  = clientOrder.target()->TimeInForce;  // DAY, IOC, GTC
    entry->OrdPrice     = clientOrder.targetPrice();
    entry->OrdQty       = qtyToAllocate.targetQty();          // Assign volume

    // Register the final routing slice inside the framework pipeline
    arg._allocationEntries->add(exchangeId, entry);

    return common::ReturnCode::SUCCESS;
}

ALLOCATOR_TEARDOWN( SimpleAllocator )
#endif // SIMPLE_ALLOCATOR_H
```

## Module B: Multi-Venue Split Allocator

Real-world trading logic rarely dumps massive size onto a single order book because doing so signals intent to the market and causes slippage. Instead, algorithms split size across multiple execution venues (e.g., `MAIN_EXCH` and `ALT_EXCH`).

This code demonstrates how to calculate precise distributions without dropping shares to integer rounding, while maintaining an all-or-nothing risk stance.

```cpp
// SimpleAllocatorSplit.cpp
#include <iostream>
#include <vector>
#include <string>

// Simulated framework status returns
enum class ReturnCode { SUCCESS, KEEP_CURRENT_ALLOCATION, REJECTED };

// Simulated structural entities for context grounding
struct TargetOrder {
    std::string symbol = "AAPL";
    int quantity = 101; // Odd lot to prove rounding calculations
    double price = 150.0;
};

struct ExecutionEntry {
    std::string targetVenue;
    int allocatedShares;
    double price;
};

// Internal venue abstraction tracking routing splits
struct RoutingVenue { 
    std::string name; 
    int shares; 
};

// Mock function representing framework verification
bool isKnownExchange(const std::string& venueName) {
    return (venueName == "MAIN_EXCH" || venueName == "ALT_EXCH");
}

template <typename OrderContext>
ReturnCode allocateSplit(const OrderContext& order, std::vector<ExecutionEntry>& outputSlices)
{
    if (order.quantity <= 0) {
        return ReturnCode::KEEP_CURRENT_ALLOCATION;
    }

    // --- The Remainder Trick ---
    // Integer division inherently drops decimal remainders (101 / 2 = 50)
    const int halfQuantity = order.quantity / 2;
    
    // Calculate the absolute remainder by subtracting the first half from total.
    // (101 - 50 = 51). This guarantees zero inventory leakage during distribution loops.
    const int remainderQuantity = order.quantity - halfQuantity;

    // Vector initialization leveraging local stack layouts
    std::vector<RoutingVenue> destinations = {
        {"MAIN_EXCH", halfQuantity},
        {"ALT_EXCH", remainderQuantity}
    };

    // Range-based lookups accessing contents directly via const reference
    for (const auto& destination : destinations) 
    {
        // Fail-safe check: protect engine integrity against corrupted configuration state
        if (!isKnownExchange(destination.name)) {
            // High-frequency engines enforce atomic failure profiles. 
            // Better to completely cancel an execution rather than trade half an order blindly.
            outputSlices.clear(); 
            return ReturnCode::REJECTED;
        }

        // Construct out-parameters in-place within the pre-allocated tracking buffer
        ExecutionEntry slice;
        slice.targetVenue      = destination.name;
        slice.allocatedShares  = destination.shares;
        slice.price            = order.price;
        outputSlices.push_back(slice);
    }

    return ReturnCode::SUCCESS;
}

int main() {
    TargetOrder rawOrder;
    std::vector<ExecutionEntry> orderSlices;

    ReturnCode result = allocateSplit(rawOrder, orderSlices);

    if (result == ReturnCode::SUCCESS) {
        std::cout << "Order Execution Slices Successfully Produced:\n";
        for (const auto& slice : orderSlices) {
            std::cout << " -> Routed " << slice.allocatedShares 
                      << " shares to " << slice.targetVenue 
                      << " @ $" << slice.price << "\n";
        }
    }
    return 0;
}
```

## Module C: The Engine Heartbeat (`ApplicationHeartbeat.C`)

This is the infrastructure module responsible for system initialization and driving the application thread continuously.

```cpp
// ApplicationHeartbeat.C
#include <iostream>

// Abstract interfaces simulating high-speed hardware polling managers
class MarketDataEventManager {
public:
    void poll() {
        // Interrogates kernel network ring buffers directly for fresh packet entries
    }
};

class ReflexOrderManager {
public:
    static ReflexOrderManager& instance() {
        static ReflexOrderManager instanceRef;
        return instanceRef;
    }
    void poll() {
        // Re-evaluates open exchange state orders against latency timers
    }
};

// Global context accessor simulation
struct ApplicationContext {
    MarketDataEventManager& marketDataEventManager() { return m_mdManager; }
private:
    MarketDataEventManager m_mdManager;
} g_appContext;

ApplicationContext& context() { return g_appContext; }

// --- STEP 3A: STARTUP LIFECYCLE ---
bool initializeApplicationSpecificLogic()
{
    std::cout << "[INIT] Initializing global electronic trading layout parameters...\n";
    
    // Pre-allocates memory tracks and maps underlying communication buses
    return true;
}

// --- STEP 3B: THE CONTINUOUS TICK LOOP ---
// This execution loop spins infinitely on a pinned, isolated core.
void processApplicationSpecificLogic()
{
    // High-speed hardware polling execution loop strategy.
    // The application never yields execution control or drops to sleep states.
    // Yields or thread-sleep configurations force context switches costing 2-10 microseconds.
    while (true) 
    {
        context().marketDataEventManager().poll();  // Scan for inbound price ticks
        ReflexOrderManager::instance().poll();      // Scan for open order execution updates
    }
}
```

---

## 4. C++ Concepts Decoded

Understanding why these code structures look the way they do requires extracting the explicit language features driving their design:

- Macros (`ALLOCATOR_SETUP(...)`): Code abstractions that expand into structural boilerplates during compilation. Used extensively by performance frameworks to eliminate object setup lines and establish low-level entry points without generating runtime function overhead.
- Templates (`template <typename ExecutionState>`): Metaprogramming constructs that prompt the compiler to generate concrete, type-specific code versions during compilation. By executing variable generation before deployment, the engine achieves absolute polymorphism without relying on expensive virtual lookup tables (`vtables`).
- References (`auto&`): Creates a direct memory alias to an existing structure instead of triggering a deep memory copy. In HFT execution pipelines, making deep object copies breaks hardware L1/L2 caches, dropping execution speeds instantly.
- Pointers (`auto*`): Stores the absolute raw memory hardware address of an entity. This allows memory-mapped components to update values directly inside the framework’s memory footprints without requiring data-passing interfaces.
- The Singleton Pattern (`::instance()`): An architectural constraint guaranteeing that exactly one unique, global instance of a component (like the `ExchangeManager`) can exist inside the processor space. This provides safe, predictable, zero-overhead access lines from any logic thread.
- Enums (`ReturnCode::SUCCESS`): Type-safe literal configurations compiled down to plain native integers. This eliminates string matching routines or magic numbers from execution pipelines, ensuring fast decision boundaries.

---

## 5. Architectural "Superpowers" Driving Sub-Microsecond Speed

High-frequency execution depends entirely on preventing standard operating system assumptions from slowing down execution paths. This involves utilizing four fundamental architectural tactics:

```unset
┌───────────────────────────────────────────────────────────────────────────────┐
│                          ULTRA-LOW LATENCY TECH STACK                         │
├───────────────────────┬───────────────────────┬───────────────────────────────┤
│    MEMORY PIPELINE    │  DATA SERIALIZATION   │       THREAD SCHEDULING       │
├───────────────────────┼───────────────────────┼───────────────────────────────┤
│  Slab Allocators     │  Zero-Copy Structures │  Core Pinning                 │
│  (Zero runtime        │  (Direct processing   │  (Isolates loops; completely  │
│   allocations)        │   from NIC buffer)    │   avoids OS context switches) │
└───────────────────────┴───────────────────────┴───────────────────────────────┘
```

1. Zero-Copy Memory Pipelines: Standard software reads network data into an OS buffer, moves it into an application cache, and parses it into intermediate business objects. An HFT engine leverages custom network cards (like Solarflare running OpenOnload) to project incoming frames directly into memory. The application reads variables right out of the live network frame buffer without copying bytes.
2. Slab Allocators: Making standard allocation requests to the OS (`malloc` or `new`) forces the system to scour memory tables for free segments, which causes dramatic performance drops. Instead, the engine claims gigantic blocks of hardware memory during boot initialization. When an active trade fires, the engine grabs small, pre-sliced memory tracks out of this block locally, reducing allocation latency to zero.
3. Core Pinning and Thread Isolation: Standard operating systems frequently rotate threads across different CPU cores to balance system load. HFT engines override this behavior by calling thread affinity registers, pinning the critical execution path to an isolated physical core. The execution loop spins at 100% capacity, blocking the OS from scheduling competing processes on that core and eliminating context-switching penalties.
4. Non-Blocking Polling Systems: HFT engines completely eliminate sleep commands or thread blocks from their codebases. Instead, the main pipeline executes an infinite loop that queries hardware rings for incoming signals. This deliberate design trade-off maximizes power consumption to minimize latency, ensuring the engine reacts to fresh pricing data in nanoseconds.

The three "superpowers" that make it fast

The repo's architecture notes call out the special technology. You don't need to master these yet, but knowing why they exist explains a lot of the weird code you'll see:

| Technology         | Plain-English purpose               | Why it's weird/special                                                    |
| ------------------ | ----------------------------------- | ------------------------------------------------------------------------- |
| **resq**         | Messaging between processes         | "Zero-copy" — data is shared in memory, never duplicated. 50 nanoseconds. |
| **spreadcast**        | Turning objects into bytes and back | Also zero-copy; a compiler plugin auto-generates the code                 |
| **statesave**          | Saving state that survives a crash  | ACID guarantees, like a database                                          |
| **Slab Allocator** | Handing out memory                  | Pre-grabs big chunks so it never has to ask the OS mid-trade              |

The recurring theme: **never copy data, never wait, never surprise-allocate memory**. Every trick is about shaving nanoseconds. When you see `auto&` (reference, no copy) everywhere instead of `auto` (copy), this is why.

---

So, how would you build one from scratch?

Putting it all together, the recipe is:

1. **Start with the framework** — `[CORE_FRAMEWORK]` gives you transport, state machine, memory, logging.
2. **Define your messages** (`msg/`) — how do orders arrive and leave?
3. **Write your validation** (`[VALIDATION_DIR]/PreprocessorImpl`) — reject bad orders early.
4. **Write your allocator** (`[ALLOCATOR_DIR]/`) — the one real decision: where does the order go? (Step 1 code)
5. **Wire up startup + the main loop** (`[MAIN_DIR]/`) — (Step 3 code)
6. **Add config** (`[CONFIG_DIR]/`) so it can be deployed to QA/prod.
7. **Write tests** (`[TEST_DIR]/`) proving new-order, fill, cancel, replace, reject all work.

`[SKELETON_APP]` is exactly this skeleton — its README literally says _"a skeleton project that you can extend to build your custom [PROPRIETARY_APPLICATION]."_ Every real trading app in this repo (`[APP_VARIANT_A]`, `[APP_VARIANT_B]`, etc.) is `[SKELETON_APP]` grown up with a smarter allocator and more validation.

---

Where I'd have you go next

To actually understand the `[SKELETON_APP]` code above rather than just read it, here's the C++ order I'd teach:

`+ C [LOGIC_HEADER].H`  
Describe what to build

---

Now that the complete structural blueprint and performance mechanics of the allocation loop are clear, what would you like to explore next? We can deep dive into how to configure a Bazel workspace optimized for hardware-specific vector instructions, or write a comprehensive lock-free ring buffer to move data between market data and execution threads.

# More Context
Yes — I remember. We were discussing your matching engine / exchange infrastructure project and you specifically wanted something different from just another matching engine, while still staying close to market infrastructure, crypto exchanges, and trading systems.

That is actually perfect for a Chainrisk-style pivot.

You should not abandon the project. You should evolve it from an exchange into a risk-aware exchange / lending simulator.

### The key insight

Right now your project is probably:

Current

### Exchange / OMS / Matching Engine

Orders

Order Book

Trades

Add a risk layer on top:

Chainrisk-style extension

### Risk-Aware Exchange / Lending Simulator

Orders

Matching

Trades

Positions

Risk Engine

Liquidations

Stress Simulator

This becomes much more unique than a plain exchange.

### What to keep from your current project

Assuming your current project already has:

* Order book

* Matching engine

* Users/accounts

* Balances

* Market data

* Trades

* REST/WebSocket APIs

Keep all of it.

These are valuable.

### What to add (the DeFi/Risk diversion)

### Step 1: Add Margin Accounts

Instead of just spot trading:

| Before    | After                      |
| --------- | -------------------------- |
| 1000 USDC | 1000 USDC + borrowed funds |
| Buy 1 ETH | Buy 2 ETH with leverage    |

Data model:

`struct Position { double collateral; double borrowed; double entryPrice; double quantity; };`

### Step 2: Add Health Factor

This is the Aave concept.

HF=Collateral Value×Liquidation ThresholdBorrowed ValueHF = \frac{\text{Collateral Value} \times \text{Liquidation Threshold}}{\text{Borrowed Value}}HF=Borrowed ValueCollateral Value×Liquidation Threshold

Example:

Collateral

$10,000

Borrowed

$6,000

Threshold

80%

Health Factor

1.33

* HF > 1 → safe

* HF = 1 → danger

* HF < 1 → liquidate

### Step 3: Add Price Shock Simulator

You already have market data.

Now add:

`simulatePriceShock(-20%); simulatePriceShock(-40%); simulatePriceShock(-60%);`

For every account:

* recalculate collateral

* recalculate borrowed value

* compute health factor

* mark for liquidation

This is literally the beginning of a stress testing engine.

### Step 4: Add Liquidation Engine

When HF < 1:

Sell collateral

Repay debt

Apply liquidation penalty

Return remainder

Pseudo:

`if (healthFactor < 1.0) { liquidate(position); }`

### Step 5: The Chainrisk part — Monte Carlo

This is the big upgrade.

Instead of one crash:

Single scenario

ETH: 4000 → 3200

Run 10,000 random paths.

`for (int i = 0; i < 10000; ++i) { auto path = generateRandomWalk(); simulate(path); }`

Output:

Probability of liquidation

12.4%

Expected bad debt

$42,000

Worst case loss

$310,000

Now you're no longer building an exchange.

You're building a risk simulation platform.

### Architecture evolution

Your current project

### Exchange Core

Order Book

Matching Engine

Positions & Balances

Add this

### Risk Layer

Health Factor Engine

Liquidation Engine

Stress Test Simulator

Risk Analytics Dashboard

### Why this is brilliant for your profile

You can now tell a very compelling story.

### Before

I built a matching engine.

### After

I built a high-performance exchange simulator and extended it with a multithreaded risk engine that performs Monte Carlo stress testing, liquidation analysis, and protocol solvency estimation.

That sounds much closer to Chainrisk.

### What technologies to use

Since your core is C++:

Matching engine

C++

Risk engine

C++

Monte Carlo

C++ threads / OpenMP

API

FastAPI or Actix

Dashboard

React + Plotly

Storage

PostgreSQL

### The one feature that will impress Arka

### Parallel simulation benchmark

Run:

1 thread

0.8M simulations/sec

4 threads

3.1M simulations/sec

8 threads

5.9M simulations/sec

Add a graph.

This leverages your systems background, not just DeFi knowledge.

### What to study while building

In order:

1

Aave Health Factor

1–2 hours

2

Liquidation threshold

30 min

3

Chainlink price feeds

30 min

4

Monte Carlo random walk

2 hours

5

Value at Risk (VaR)

1 hour

6

Stress testing

1 hour

You can learn enough in 2–3 days to build a credible prototype.

# Prediction Market support
Yes. **RFQ is the sweet spot.** It adds a completely new trading workflow without derailing the core exchange project.

Given our roadmap, I would **not** implement prediction markets yet. Instead, build the infrastructure that *could* support them later. That makes the architecture more realistic and keeps the project focused.

Here's what I'd add to the project plan.

---

# Current Project

```
Gateway
      │
      ▼
OMS
      │
      ▼
Risk
      │
      ▼
Matching Engine
      │
      ▼
Market Data
```

---

# Add one more service

```
Gateway
      │
      ▼
OMS
      │
 ┌────┴────┐
 │         │
 ▼         ▼
Order Book RFQ Service
 │         │
 ▼         ▼
Matching  Quote Engine
 Engine
```

Notice:

The RFQ path never reaches the matching engine.

That's exactly how many OTC systems work.

---

# Components to implement

## 1. RFQ Service

Receives

```
RequestQuote

Instrument

Quantity

Side
```

Example

```json
{
    "instrument": "BTCUSD",
    "side": "BUY",
    "quantity": 25
}
```

Responsibilities

* Validate request
* Generate RFQ ID
* Publish to market makers
* Wait for quotes
* Handle timeout

---

## 2. Quote Engine

Pretend to be a liquidity provider.

Input

```
RFQ
```

Output

```json
{
    "bid": 100.2,
    "ask": 100.4,
    "expires": 200ms
}
```

Initially

```
midprice ± spread
```

Later

* inventory adjustment
* volatility
* risk
* latency simulation

---

## 3. RFQ State Machine

Something interviewers love.

```
NEW

↓

SENT

↓

QUOTED

↓

ACCEPTED

↓

EXECUTED
```

Also

```
NEW

↓

TIMEOUT

↓

EXPIRED
```

---

## 4. Quote Expiration

Quotes should expire.

Example

```
Quote valid

250 ms
```

After

```
Expired
```

Reject execution.

Very realistic.

---

## 5. Multi Dealer RFQ

Instead of

```
Client

↓

Dealer
```

Do

```
        MM1

      /

Client

      \

        MM2

      \

        MM3
```

Collect

```
102.1

102.0

101.8
```

Automatically choose

```
Best Price
```

Exactly how institutional RFQ systems often work.

---

## 6. Quote Acceptance

```
Client

↓

Accept Quote

↓

Execution Report
```

Generate

```
Trade ID

Execution ID

Timestamp
```

Reuse your OMS messaging.

---

# Future (Don't Build Yet)

Later you can plug in

```
Prediction Contract

↓

RFQ

↓

Quote Engine

↓

Execution
```

No architecture changes required.

---

# Why this is valuable

This isn't just another feature—it's another **market model**.

Your project will support:

```
Continuous Trading

↓

Matching Engine
```

and

```
Request For Quote

↓

Dealer Pricing
```

Many production trading platforms support both.

---

# Stretch Goal (Don't Implement Yet)

Leave a placeholder interface like:

```rust
trait ExecutionVenue {
    fn execute(&self, request: ExecutionRequest) -> ExecutionResult;
}
```

Then implement:

```
OrderBookVenue
```

and

```
RFQVenue
```

In the future, adding an auction venue or prediction-market venue becomes straightforward without changing the OMS.

## Overall recommendation

If I were prioritizing for maximum impact:

1. ✅ Finish the core exchange (matching engine, OMS, market data, risk).
2. ✅ Add **RFQ Service** with quote lifecycle, expiry, and multi-dealer support.
3. ✅ Design interfaces so new execution venues can be added later.
4. ❌ Skip prediction-market-specific logic until the core infrastructure is polished.

This keeps the project coherent while giving you a compelling architecture to discuss with both traditional trading firms and startups like Totalis.

---
---

# APPENDIX A — Target JD: Equiti, Senior Rust HFT Engineer

**Added:** 2026-08-12. Appended on request. Raw JD text below is verbatim; the mapping table after it is my analysis, not part of the JD.

## Raw JD (as provided)

```
Required:
• Deep working knowledge of x86-64 / AMD64 microarchitecture: out-of-order
  execution, store buffers, cache hierarchy, TSC, RDTSCP
• Hands-on experience with kernel-bypass networking — DPDK, RDMA Verbs,
  XDP, or equivalent
• Proven ability to design, implement, and formally reason about lock-free
  concurrent data structures
• Production experience with Linux performance tooling at the hardware event
  level (perf, pmu-tools, rdpmc)
• Track record of owning a system component from design through production,
  including operational runbooks and failure mode documentation

Preferred:
• Experience with FIX protocol engine implementation or LP connectivity
  management in a live trading environment
• Familiarity with io_uring internals, O_DIRECT alignment constraints, and
  SQPOLL affinity configuration
• Experience with SIMD intrinsics (AVX-512, AVX2) in a production codebase
• Knowledge of AMD EPYC or Intel Xeon platform-level features: CCD topology,
  NUMA, Platform QoS, PCIe root complex layout
• Experience with active-active high-availability designs using RDMA or
  InfiniBand transport
• Undergraduate or postgraduate degree in Computer Science, Computer
  Engineering, or equivalent

What We Offer
• Subsystem ownership on a greenfield, production-grade system with real
  market impact
• Direct influence over architecture decisions at the component and system level
• A technically demanding team where correctness is non-negotiable and
  performance is measured against hardware limits
• Hardware access: production-class Dell PowerEdge R6615 servers available for
  profiling and validation
• Competitive senior compensation package with performance-based component
• [Benefits, equity, leave — as per company policy]

Our Stack
• Primary language: Rust (nightly, target-cpu=znver4)
• Networking: DPDK mlx5 PMD, Mellanox ConnectX-6 Lx (25 GbE), RDMA Verbs
• Storage I/O: io_uring SQPOLL + O_DIRECT, PERC H965i NV-cache write path
• Hardware: Dell PowerEdge R6615, AMD EPYC 9354P — 32 cores, 256 MB L3,
  single NUMA node
• SIMD: AVX-512 (Zen 4 — no frequency downclocking), FTZ/DAZ via MXCSR
• Protocols: FIX (LP + client), ITCH, MT5 API, RoCEv2
• Platform tuning: isolcpus, nohz_full, resctrl / AMD Platform QoS, huge pages
• Observability: Linux perf, flamegraph, pmu-tools, RDTSCP instrumentation
```

## How each bullet maps to the KuberCore plan

| JD bullet | KuberCore stage | Reachable on the Ryzen 3500U? |
|---|---|---|
| Cache hierarchy | **Stage 1** — flat-array order book; the whole design argument | Yes — it's the core of Day 3 |
| TSC, RDTSCP | **Stage 4** — `drishti/tsc.h`, calibration + fences | Yes — `rdtscp`, `constant_tsc`, `nonstop_tsc` all present |
| Out-of-order execution | **Stage 4** (why `rdtsc` needs fencing) + **Stage 5** (why atomics need ordering) | Yes |
| Store buffers | **Stage 5** — the concrete reason `x=1; r=y` can yield `r1==r2==0` | Yes |
| Lock-free + formally reason | **Stage 5** — SPSC ring, acquire/release, written correctness proof, TSan | Yes — 8 threads is plenty |
| perf / pmu-tools / rdpmc | **Stage 4** — prove the flat-array claim with real cache-miss counts | Yes — bare metal has full PMU access |
| Component ownership + **runbooks + failure modes** | **Ongoing** — `docs/runbooks/`, written as each component lands | Yes — it's writing, not hardware |
| **FIX engine / LP connectivity** | **Stage 6** — `dvara`, full session layer incl. gap-fill | Yes — *and this is already your real experience* |
| io_uring / O_DIRECT / SQPOLL | **Stage 7** — `chitragupta` log writer | Yes — kernel has it; needs `liburing-dev` |
| SIMD intrinsics | **Stage 6** — AVX2 inside the FIX parser (`_mm256_cmpeq_epi8` + `movemask` for SOH scanning) | **AVX2 only.** No AVX-512 — needs Zen 4 |
| Active-active HA | **Stage 10** — `sesha`, deterministic replicas + sequencer | Yes — two processes over loopback. RDMA transport only if hardware is rented |
| Kernel bypass (DPDK / RDMA / XDP) | **Out of scope** | **No** — needs a Mellanox NIC |
| EPYC CCD topology / NUMA | **Out of scope** | **No** — single NUMA node *(so is their EPYC 9354P)* |
| Rust as primary language | **Stage 7** — `trikala`, offline, no FFI. C++ stays the hot path | Yes |

**Honest read:** this is a senior posting wanting production experience across five specialised domains. It is not a near-term match. It is an excellent syllabus, and roughly two thirds of it is buildable on the existing hardware.

**The one line to lead with in any conversation:** FIX engine implementation and LP connectivity management is listed as *preferred*, and it is the bullet backed by actual employment history rather than a side project.

## Sections of this file now claimed by the naming scheme

- The **margin / health-factor / liquidation / Monte-Carlo** notes → reserved as a successor to `yama` (risk). Needs an oracle and mark-to-market; a genuinely different project from the fully-collateralized binary-outcome model, so deliberately not merged.
- The **RFQ / quote lifecycle / multi-dealer** notes → reserved as `prashna` ("question" — an RFQ is a question asked of dealers). Additive after Stage 2; the RFQ path never touches the matching engine.
