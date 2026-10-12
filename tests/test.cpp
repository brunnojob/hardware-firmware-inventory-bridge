#include "inventory.hpp"
#include "wire.h"
#include <cassert>
struct Failing final : inventory::Repository { void commit(const inventory::Movement&) override { throw std::runtime_error("failed commit"); } };
template<class F> void rejected(F f) { bool denied = false; try { f(); } catch (const std::exception&) { denied = true; } assert(denied); }
int main() {
    inventory::MemoryRepository repository; inventory::Ledger ledger(repository);
    assert(ledger.move("breaker", 10, "delivery-1").balance == 10);
    assert(ledger.move("breaker", 10, "delivery-1").balance == 10);
    assert(repository.events.size() == 1);
    rejected([&] { ledger.move("breaker", 11, "delivery-1"); });
    assert(ledger.move("breaker", -4, "withdrawal-1").balance == 6);
    rejected([&] { ledger.move("breaker", -7, "withdrawal-2"); });
    rejected([&] { ledger.move("invalid\"sku", 1, "key"); });
    assert(ledger.snapshot().at("breaker") == 6);
    Failing failure; inventory::Ledger atomic(failure);
    rejected([&] { atomic.move("cable", 2, "delivery"); });
    assert(atomic.snapshot().empty());
    wire_sample source{100, 2000, 2500, 6500, 1, 1}, decoded{};
    uint8_t frame[WIRE_FRAME_SIZE]; wire_encode(&source, frame);
    assert(wire_decode(frame, &decoded) && decoded.temperature == source.temperature);
    frame[12] ^= 1; assert(!wire_decode(frame, &decoded));
}
