# C++ Learning Notes — Event Bus

## What We Built

An **Event Bus** — a publish/subscribe system where components react to events without knowing about each other.

```
Publisher                     EventBus                    Subscribers
─────────                     ────────                    ───────────
OrderEvent{...}  ──────►      ┌─────────────┐
                              │ OrderEvent   │ ──────►  handler1()
                              │ TradeEvent   │ ──────►  handler2()
                              │ MarketData   │ ──────►  handler3()
                              └─────────────┘
```

**Why it matters in trading:** Components like OMS, Risk Engine, and Position Manager need to react to events (order arrived, order filled, price changed) without being tightly coupled to each other.

---

## Concept 1: `concept` (C++20)

### What It Is

A `concept` is a named boolean predicate checked at **compile time** (before the program runs). It constrains what types a template can accept.

### Without Concepts (Old Way)

```cpp
template <typename Handler, typename EventType>
void subscribe(Handler handler) {
    // If Handler can't accept EventType, you get pages of cryptic errors
    handler(EventType{});
}
```

Error message (confusing):
```
error: no matching function for call to 'operator()'
note: candidate template ignored: deduced conflicting types for parameter 'Handler'
```

### With Concepts (New Way)

```cpp
template <typename Handler, typename EventType>
concept VoidCallable = std::is_invocable_r_v<void, Handler, const EventType&>;

template <VoidCallable<EventType> Handler, typename EventType>
void subscribe(Handler handler) {
    handler(EventType{});
}
```

Error message (clear):
```
error: constraints not satisfied
note: 'Handler' must be callable with 'const EventType&' and return 'void'
```

### Our Two Concepts

```cpp
// Concept 1: Handler must accept const EventType& and return void
template <typename Handler, typename EventType>
concept VoidCallable = std::is_invocable_r_v<void, Handler, const EventType&>;

// Concept 2: EventType must be a plain struct/class (not a pointer)
template <typename T>
concept IsEvent = std::is_object_v<T> && !std::is_pointer_v<T>;
```

### Plain English

- `VoidCallable`: "The handler function must accept `const EventType&` and return nothing"
- `IsEvent`: "The event type must be a normal object, not a pointer"

---

## Concept 2: Strong Typing with `struct`

### What It Is

Instead of using a plain `uint64_t` for handler IDs, we wrap it in a struct:

```cpp
struct HandlerId {
    uint64_t value;
    bool operator==(const HandlerId& other) const { return value == other.value; }
    bool operator!=(const HandlerId& other) const { return value != other.value; }
};
```

### Why It Matters

If we used plain `uint64_t`:

```cpp
uint64_t handlerId = bus.subscribe(...);
uint64_t orderId = 12345;

// Compiler can't tell these apart — both are just numbers!
bus.unsubscribe(orderId);  // Bug! But compiles fine
```

With `HandlerId`:

```cpp
HandlerId handlerId = bus.subscribe(...);
uint64_t orderId = 12345;

bus.unsubscribe(orderId);  // Compiler error! Can't convert uint64_t to HandlerId
```

### Plain English

`HandlerId` is a "typed integer" that prevents you from mixing up different kinds of IDs.

---

## Concept 3: Type Erasure

### The Problem

The EventBus needs to store handlers for different event types in one place:

```cpp
// These are DIFFERENT types:
std::function<void(const OrderEvent&)> handler1;
std::function<void(const TradeEvent&)> handler2;

// You CANNOT put them in the same vector:
std::vector<???> handlers;  // What goes in <???>?
```

### The Solution

We hide the specific type behind a base class:

```cpp
// Base class — knows nothing about specific event types
struct HandlerListBase {
    virtual ~HandlerListBase() = default;  // Virtual destructor for safe cleanup
};

// Derived class — knows about one specific event type
template <IsEvent EventType>
struct HandlerList : HandlerListBase {
    std::vector<std::function<void(const EventType&)>> handlers;
    std::vector<HandlerId> ids;
};
```

Now we can store different types:

```cpp
std::unique_ptr<HandlerListBase> list1 = std::make_unique<HandlerList<OrderEvent>>();
std::unique_ptr<HandlerListBase> list2 = std::make_unique<HandlerList<TradeEvent>>();

// Both are HandlerListBase*, so they can go in the same container
```

### Plain English

We're hiding the specific type behind a generic interface, so we can store different types in the same container. It's like storing different shapes (circles, squares) in one box by calling them all "shapes."

---

## Concept 4: `std::unique_ptr` (RAII)

### What It Is

`std::unique_ptr` is a smart pointer that automatically deletes the object it points to when it goes out of scope.

```cpp
{
    auto ptr = std::make_unique<HandlerList<OrderEvent>>();
    // ptr owns the HandlerList
}  // ptr goes out of scope → HandlerList is automatically deleted
```

### Without unique_ptr (Dangerous)

```cpp
{
    auto* ptr = new HandlerList<OrderEvent>();
    // ptr owns the HandlerList
}  // ptr goes out of scope → HandlerList is LEAKED (memory not freed)
```

### Why It Matters

- No memory leaks
- No need to remember to call `delete`
- Exception-safe (if an exception occurs, cleanup still happens)

### Plain English

`unique_ptr` is a "responsibility token" — whoever holds it is responsible for the object, and when they're gone, the object is cleaned up automatically.

---

## Concept 5: `std::function`

### What It Is

`std::function` is a wrapper that can store **any callable** — lambdas, function pointers, function objects:

```cpp
std::function<void(int)> f1 = [](int x) { std::cout << x; };  // Lambda
std::function<void(int)> f2 = [](int x) { std::cout << x * 2; };  // Different lambda
std::function<void(int)> f3 = someFunction;  // Function pointer
```

### Why It Matters

We need to store handlers that do different things. Without `std::function`, we'd need templates everywhere:

```cpp
// Without std::function — each handler type is different
template <typename F>
struct Handler {
    F func;
};

Handler<Lambda1> h1{lambda1};
Handler<Lambda2> h2{lambda2};  // Different type!
```

With `std::function`, they're all the same type:

```cpp
std::function<void(int)> h1 = lambda1;
std::function<void(int)> h2 = lambda2;  // Same type!
```

### Plain English

`std::function` is a "universal container" for anything you can call.

---

## Concept 6: Type Index (Static Variables in Templates)

### What It Is

```cpp
template <IsEvent EventType>
static uint32_t typeIndex() {
    static const uint32_t id = nextTypeIndex_++;
    return id;
}
```

Each time you instantiate this template with a different `EventType`, you get a **new static variable**:

```cpp
typeIndex<OrderEvent>()   // Returns 0 (first call)
typeIndex<OrderEvent>()   // Returns 0 (same static variable)
typeIndex<TradeEvent>()   // Returns 1 (new static variable)
typeIndex<TradeEvent>()   // Returns 1 (same static variable)
```

### Why It Matters

We need to store handlers for different event types in one `unordered_map`. The key is the type index — a unique number for each event type.

```cpp
std::unordered_map<uint32_t, std::unique_ptr<HandlerListBase>> handlers_;

// typeIndex<OrderEvent>() → key 0
// typeIndex<TradeEvent>() → key 1
```

### Plain English

It's like giving each event type a unique ID card number. `OrderEvent` is always type 0, `TradeEvent` is always type 1.

---

## Concept 7: `const&` (Pass by Const Reference)

### What It Is

```cpp
void publish(const OrderEvent& event);  // const reference — no copy
void publish(OrderEvent event);         // by value — makes a copy
```

### Why It Matters

In trading, you might have an `OrderEvent` with 20 fields. Copying it takes time and uses memory. Passing by `const&` just passes the memory address — almost free.

```cpp
// This copies the entire struct:
void bad(OrderEvent event) {
    // event is a copy — slower, uses more memory
}

// This just references the original:
void good(const OrderEvent& event) {
    // event is the original — fast, no extra memory
}
```

### Plain English

`const&` means "I promise not to modify it, and I'll use the original, not a copy."

---

## Concept 8: Designated Initializers (C++20)

### What It Is

```cpp
// Old way (C++17):
OrderEvent event;
event.orderId = 12345;
event.price = 99.50;
event.quantity = 100;
event.isBuy = true;

// New way (C++20):
OrderEvent event{
    .orderId = 12345,
    .price = 99.50,
    .quantity = 100,
    .isBuy = true
};
```

### Why It Matters

- Clearer — you can see which value goes to which field
- Safer — compiler catches typos in field names
- More compact — one line instead of four

### Plain English

Designated initializers let you name each field when creating a struct.

---

## How It All Fits Together

```cpp
EventBus bus;

// Subscribe: store handler with type-safe ID
auto id = bus.subscribe<OrderEvent>([](const OrderEvent& e) {
    std::cout << "Order: " << e.orderId << "\n";
});

// Publish: call all handlers for this event type
bus.publish(OrderEvent{.orderId = 1, .price = 100.0, .quantity = 50, .isBuy = true});

// Unsubscribe: remove handler by ID
bus.unsubscribe<OrderEvent>(id);
```

---

## Summary of C++ Concepts

| Concept | What It Is | Why We Used It |
|---------|------------|----------------|
| `concept` | Compile-time type constraint | Clear error messages |
| Strong typing | Wrapping primitives in structs | Prevent accidental misuse |
| Type erasure | Hiding types behind base class | Store different types together |
| `std::unique_ptr` | Automatic memory cleanup | No memory leaks |
| `std::function` | Universal callable wrapper | Store any handler |
| Type index | Static variable per type | Map event types to integers |
| `const&` | Read-only reference, no copy | Zero-copy passing |
| Designated initializers | Named field initialization | Clear, safe struct creation |

---

## Next Steps

- RAII (Resource Acquisition Is Initialization)
- Move Semantics
- CRTP (Curiously Recurring Template Pattern)
- Memory Pools
