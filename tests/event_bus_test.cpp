// event_bus_test.cpp
//
// Build/run:  cmake --build --preset dev && ctest --preset dev
//
// Ported from assert() to the CHECK/REQUIRE macros in kuber_test.h. That port
// is not cosmetic: assert() expands to nothing when NDEBUG is defined, so the
// previous version of this file passed vacuously in any Release build, and its
// main() returned 0 whether or not anything failed.

#include "kuber_test.h"
#include "trading/event_bus.h"

// ---------------------------------------------------------------------------
// Test 1: Basic subscribe and publish
// ---------------------------------------------------------------------------
void test_basic_publish() {
    trading::EventBus bus;
    int receivedValue = 0;

    // Lambda with capture — a closure over a local variable.
    bus.subscribe<trading::OrderEvent>(
        [&receivedValue](const trading::OrderEvent& e) { receivedValue = e.quantity; });

    bus.publish(trading::OrderEvent{.orderId = 1, .price = 100.50, .quantity = 42, .isBuy = true});

    CHECK_EQ(receivedValue, 42);
}

// ---------------------------------------------------------------------------
// Test 2: Multiple handlers for the same event
// ---------------------------------------------------------------------------
void test_multiple_handlers() {
    trading::EventBus bus;
    int callCount = 0;

    bus.subscribe<trading::TradeEvent>([&callCount](const trading::TradeEvent&) { ++callCount; });
    bus.subscribe<trading::TradeEvent>([&callCount](const trading::TradeEvent&) { ++callCount; });

    bus.publish(trading::TradeEvent{});

    CHECK_EQ(callCount, 2);
}

// ---------------------------------------------------------------------------
// Test 3: Unsubscribe removes the handler
// ---------------------------------------------------------------------------
void test_unsubscribe() {
    trading::EventBus bus;
    int callCount = 0;

    const auto id =
        bus.subscribe<trading::OrderEvent>([&callCount](const trading::OrderEvent&) { ++callCount; });

    bus.publish(trading::OrderEvent{});
    CHECK_EQ(callCount, 1);

    const bool removed = bus.unsubscribe<trading::OrderEvent>(id);
    CHECK(removed);

    bus.publish(trading::OrderEvent{});
    CHECK_EQ(callCount, 1);  // unchanged — the handler is gone
}

// ---------------------------------------------------------------------------
// Test 4: Event types are isolated from each other
// ---------------------------------------------------------------------------
void test_type_isolation() {
    trading::EventBus bus;
    int orderCount = 0;
    int tradeCount = 0;

    bus.subscribe<trading::OrderEvent>([&orderCount](const trading::OrderEvent&) { ++orderCount; });
    bus.subscribe<trading::TradeEvent>([&tradeCount](const trading::TradeEvent&) { ++tradeCount; });

    bus.publish(trading::OrderEvent{});
    CHECK_EQ(orderCount, 1);
    CHECK_EQ(tradeCount, 0);

    bus.publish(trading::TradeEvent{});
    CHECK_EQ(orderCount, 1);
    CHECK_EQ(tradeCount, 1);
}

// ---------------------------------------------------------------------------
// Test 5: Handler IDs are distinct and independently removable
// ---------------------------------------------------------------------------
void test_handler_id_type_safety() {
    trading::EventBus bus;

    const auto id1 = bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    const auto id2 = bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});

    CHECK(id1 != id2);

    bus.unsubscribe<trading::OrderEvent>(id1);
    // 1U, not 1: handlerCount() returns size_t, and comparing signed to
    // unsigned is exactly the class of bug -Wsign-compare exists to catch.
    CHECK_EQ(bus.handlerCount<trading::OrderEvent>(), 1U);
}

// ---------------------------------------------------------------------------
// Test 6: clear() removes every handler for every type
// ---------------------------------------------------------------------------
void test_clear() {
    trading::EventBus bus;

    bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    bus.subscribe<trading::TradeEvent>([](const trading::TradeEvent&) {});

    CHECK_EQ(bus.handlerCount<trading::OrderEvent>(), 2U);
    CHECK_EQ(bus.handlerCount<trading::TradeEvent>(), 1U);

    bus.clear();

    CHECK_EQ(bus.handlerCount<trading::OrderEvent>(), 0U);
    CHECK_EQ(bus.handlerCount<trading::TradeEvent>(), 0U);
}

// ---------------------------------------------------------------------------
// Test 7: C++20 designated initializers
// ---------------------------------------------------------------------------
void test_designated_initializers() {
    trading::EventBus bus;
    std::uint64_t receivedId = 0;
    double receivedPrice = 0.0;

    bus.subscribe<trading::OrderEvent>([&receivedId, &receivedPrice](const trading::OrderEvent& e) {
        receivedId = e.orderId;
        receivedPrice = e.price;
    });

    bus.publish(
        trading::OrderEvent{.orderId = 12345, .price = 99.95, .quantity = 100, .isBuy = false});

    CHECK_EQ(receivedId, 12345U);
    // Exact float comparison is safe here only because the value was copied,
    // never computed. Never write this against arithmetic results.
    CHECK_EQ(receivedPrice, 99.95);
}

int main() {
    std::cout << "=== Event Bus Tests ===\n\n";

    RUN_TEST(test_basic_publish);
    RUN_TEST(test_multiple_handlers);
    RUN_TEST(test_unsubscribe);
    RUN_TEST(test_type_isolation);
    RUN_TEST(test_handler_id_type_safety);
    RUN_TEST(test_clear);
    RUN_TEST(test_designated_initializers);

    return TEST_SUMMARY();
}
