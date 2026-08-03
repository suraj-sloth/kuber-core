// event_bus_test.cpp
//
// Compilation: g++ -std=c++20 -Wall -Wextra -o event_bus_test event_bus_test.cpp
// Run: ./event_bus_test
//
// This test demonstrates:
// 1. Subscribing to events
// 2. Publishing events
// 3. Unsubscribing handlers
// 4. Multiple handlers for the same event
// 5. Type safety — events are isolated by type

#include "trading/event_bus.h"
#include <cassert>
#include <iostream>
#include <string>

// ============================================================================
// Test 1: Basic subscribe and publish
// ============================================================================

void test_basic_publish() {
    std::cout << "Test 1: Basic publish... ";

    trading::EventBus bus;
    int receivedValue = 0;

    // Subscribe a lambda that captures `receivedValue` by reference
    // C++ Concept: Lambda with capture — closure over local variable
    bus.subscribe<trading::OrderEvent>([&receivedValue](const trading::OrderEvent& e) {
        receivedValue = e.quantity;
    });

    // Publish an event
    bus.publish(trading::OrderEvent{.orderId = 1, .price = 100.50, .quantity = 42, .isBuy = true});

    assert(receivedValue == 42);
    std::cout << "PASS\n";
}

// ============================================================================
// Test 2: Multiple handlers for same event
// ============================================================================

void test_multiple_handlers() {
    std::cout << "Test 2: Multiple handlers... ";

    trading::EventBus bus;
    int callCount = 0;

    bus.subscribe<trading::TradeEvent>([&callCount](const trading::TradeEvent&) {
        callCount++;
    });

    bus.subscribe<trading::TradeEvent>([&callCount](const trading::TradeEvent&) {
        callCount++;
    });

    bus.publish(trading::TradeEvent{});

    assert(callCount == 2);
    std::cout << "PASS\n";
}

// ============================================================================
// Test 3: Unsubscribe removes handler
// ============================================================================

void test_unsubscribe() {
    std::cout << "Test 3: Unsubscribe... ";

    trading::EventBus bus;
    int callCount = 0;

    auto id = bus.subscribe<trading::OrderEvent>([&callCount](const trading::OrderEvent&) {
        callCount++;
    });

    bus.publish(trading::OrderEvent{});
    assert(callCount == 1);

    // Unsubscribe
    bool removed = bus.unsubscribe<trading::OrderEvent>(id);
    assert(removed);

    // Should not be called again
    bus.publish(trading::OrderEvent{});
    assert(callCount == 1);

    std::cout << "PASS\n";
}

// ============================================================================
// Test 4: Event types are isolated
// ============================================================================

void test_type_isolation() {
    std::cout << "Test 4: Type isolation... ";

    trading::EventBus bus;
    int orderCount = 0;
    int tradeCount = 0;

    // These are separate subscription lists
    bus.subscribe<trading::OrderEvent>([&orderCount](const trading::OrderEvent&) {
        orderCount++;
    });

    bus.subscribe<trading::TradeEvent>([&tradeCount](const trading::TradeEvent&) {
        tradeCount++;
    });

    // Publishing OrderEvent only triggers OrderEvent handlers
    bus.publish(trading::OrderEvent{});
    assert(orderCount == 1);
    assert(tradeCount == 0);

    // Publishing TradeEvent only triggers TradeEvent handlers
    bus.publish(trading::TradeEvent{});
    assert(orderCount == 1);
    assert(tradeCount == 1);

    std::cout << "PASS\n";
}

// ============================================================================
// Test 5: Handler ID type safety
// ============================================================================

void test_handler_id_type_safety() {
    std::cout << "Test 5: Handler ID type safety... ";

    trading::EventBus bus;

    // Each subscribe call returns a unique HandlerId
    auto id1 = bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    auto id2 = bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});

    // IDs are distinct
    assert(id1 != id2);

    // Unsubscribing id1 does not affect id2
    bus.unsubscribe<trading::OrderEvent>(id1);
    assert(bus.handlerCount<trading::OrderEvent>() == 1);

    std::cout << "PASS\n";
}

// ============================================================================
// Test 6: Clear removes all handlers
// ============================================================================

void test_clear() {
    std::cout << "Test 6: Clear... ";

    trading::EventBus bus;

    bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    bus.subscribe<trading::OrderEvent>([](const trading::OrderEvent&) {});
    bus.subscribe<trading::TradeEvent>([](const trading::TradeEvent&) {});

    assert(bus.handlerCount<trading::OrderEvent>() == 2);
    assert(bus.handlerCount<trading::TradeEvent>() == 1);

    bus.clear();

    assert(bus.handlerCount<trading::OrderEvent>() == 0);
    assert(bus.handlerCount<trading::TradeEvent>() == 0);

    std::cout << "PASS\n";
}

// ============================================================================
// Test 7: Structured binding with designated initializers
// ============================================================================

void test_designated_initializers() {
    std::cout << "Test 7: Designated initializers... ";

    trading::EventBus bus;
    uint64_t receivedId = 0;
    double receivedPrice = 0.0;

    bus.subscribe<trading::OrderEvent>(
        [&receivedId, &receivedPrice](const trading::OrderEvent& e) {
            receivedId = e.orderId;
            receivedPrice = e.price;
        });

    // C++20 Designated Initializers: clear, self-documenting event construction
    bus.publish(trading::OrderEvent{
        .orderId = 12345,
        .price = 99.95,
        .quantity = 100,
        .isBuy = false
    });

    assert(receivedId == 12345);
    assert(receivedPrice == 99.95);

    std::cout << "PASS\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Event Bus Tests ===\n\n";

    test_basic_publish();
    test_multiple_handlers();
    test_unsubscribe();
    test_type_isolation();
    test_handler_id_type_safety();
    test_clear();
    test_designated_initializers();

    std::cout << "\nAll tests passed.\n";
    return 0;
}