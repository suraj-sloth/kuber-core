#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace trading {

// ============================================================================
// C++ Concept: Constraining template parameters
// ============================================================================
// A "concept" is a named boolean predicate checked at compile time.
// It prevents cryptic template errors by giving clear constraints.
//
// Here, we require that Handler is callable with const EventType& and returns void.
// If someone passes a handler that returns int, they get a clear error.
// ============================================================================

template <typename Handler, typename EventType>
concept VoidCallable = std::is_invocable_r_v<void, Handler, const EventType&>;

// ============================================================================
// C++ Concept: Event must be a plain struct/class
// ============================================================================
// Events should be lightweight value types passed by const reference.
// ============================================================================

template <typename T>
concept IsEvent = std::is_object_v<T> && !std::is_pointer_v<T>;

// ============================================================================
// EventBus: A type-safe publish/subscribe system
// ============================================================================
//
// C++ Concepts Used:
//   - Concepts (VoidCallable, IsEvent)        → compile-time validation
//   - std::function                            → type-erased callbacks
//   - Type-indexed storage via type_id         → each event type isolated
//   - Virtual dispatch for type erasure        → clean, safe callback storage
//
// Design Notes:
//   - Each event type has its own independent subscriber list
//   - Handlers are called in subscription order
//   - Thread safety is NOT included here — add it when you need it
//
// ============================================================================

class EventBus {
public:
    // Strong type for handler IDs — prevents mixing up integers
    struct HandlerId {
        uint64_t value;

        bool operator==(const HandlerId& other) const { return value == other.value; }
        bool operator!=(const HandlerId& other) const { return value != other.value; }
    };

    // Subscribe a handler for events of type EventType
    //
    // Usage:
    //   bus.subscribe<OrderEvent>([](const OrderEvent& e) {
    //       std::cout << "Got order: " << e.id << "\n";
    //   });
    //
    template <IsEvent EventType>
    HandlerId subscribe(std::function<void(const EventType&)> handler) {
        auto& list = getHandlerList<EventType>();

        HandlerId id{nextId_++};
        list.handlers.push_back(std::move(handler));
        list.ids.push_back(id);

        return id;
    }

    // Overload: accept any callable (lambdas, function pointers, etc.)
    //
    template <IsEvent EventType, typename Handler>
        requires VoidCallable<Handler, EventType>
    HandlerId subscribe(Handler&& handler) {
        return subscribe<EventType>(
            std::function<void(const EventType&)>(std::forward<Handler>(handler))
        );
    }

    // Unsubscribe a handler by its ID
    //
    // Returns true if the handler was found and removed.
    //
    template <IsEvent EventType>
    bool unsubscribe(HandlerId id) {
        auto& list = getHandlerList<EventType>();

        for (size_t i = 0; i < list.ids.size(); ++i) {
            if (list.ids[i] == id) {
                // The cast is required, not decoration: iterator operator+ takes
                // a SIGNED difference_type, so `begin() + i` with a size_t i is an
                // implicit sign conversion. -Wsign-conversion rejects it.
                const auto offset = static_cast<std::ptrdiff_t>(i);
                list.ids.erase(list.ids.begin() + offset);
                list.handlers.erase(list.handlers.begin() + offset);
                return true;
            }
        }
        return false;
    }

    // Publish an event to all subscribed handlers
    //
    // The event is passed by const reference — no copying.
    //
    template <IsEvent EventType>
    void publish(const EventType& event) {
        auto& list = getHandlerList<EventType>();

        for (auto& handler : list.handlers) {
            handler(event);
        }
    }

    // Remove all handlers for all event types
    void clear() {
        handlers_.clear();
        nextId_ = 1;
    }

    // Number of handlers for a specific event type
    template <IsEvent EventType>
    size_t handlerCount() const {
        auto it = handlers_.find(typeIndex<EventType>());
        if (it == handlers_.end()) return 0;
        return static_cast<const HandlerList<EventType>&>(*it->second).handlers.size();
    }

private:
    // ============================================================================
    // Internal: HandlerList stores handlers for one event type
    // ============================================================================

    struct HandlerListBase {
        virtual ~HandlerListBase() = default;
    };

    template <IsEvent EventType>
    struct HandlerList : HandlerListBase {
        std::vector<std::function<void(const EventType&)>> handlers;
        std::vector<HandlerId> ids;
    };

    // ============================================================================
    // Internal: Type index — maps C++ types to runtime integers
    // ============================================================================
    //
    // C++ Concept: Template specialization for type identification
    // Each unique EventType gets its own static `id` variable.
    // This is how we store multiple event types in a single map.
    //
    // ============================================================================

    template <IsEvent EventType>
    static uint32_t typeIndex() {
        static const uint32_t id = nextTypeIndex_++;
        return id;
    }

    static inline uint32_t nextTypeIndex_ = 0;

    // ============================================================================
    // Internal: Get the handler list for a specific event type
    // ============================================================================

    template <IsEvent EventType>
    HandlerList<EventType>& getHandlerList() {
        uint32_t idx = typeIndex<EventType>();

        auto it = handlers_.find(idx);
        if (it == handlers_.end()) {
            auto list = std::make_unique<HandlerList<EventType>>();
            auto* ptr = list.get();
            handlers_[idx] = std::move(list);
            return *ptr;
        }

        return static_cast<HandlerList<EventType>&>(*it->second);
    }

    template <IsEvent EventType>
    const HandlerList<EventType>& getHandlerList() const {
        uint32_t idx = typeIndex<EventType>();
        auto it = handlers_.find(idx);
        return static_cast<const HandlerList<EventType>&>(*it->second);
    }

    // ============================================================================
    // Member variables
    // ============================================================================

    uint64_t nextId_ = 1;

    // Single map: type_index → type-erased handler list
    std::unordered_map<uint32_t, std::unique_ptr<HandlerListBase>> handlers_;
};

// ============================================================================
// Example event types (defined here for completeness)
// ============================================================================

struct OrderEvent {
    uint64_t orderId;
    double price;
    int quantity;
    bool isBuy;
};

struct TradeEvent {
    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    double price;
    int quantity;
};

struct MarketDataEvent {
    const char* symbol;
    double bidPrice;
    double askPrice;
    int bidSize;
    int askSize;
};

struct genericEvent {
    int data;
};

} // namespace trading