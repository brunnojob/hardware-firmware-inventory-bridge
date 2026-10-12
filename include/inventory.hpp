#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
namespace inventory {
struct Movement { std::uint64_t sequence; std::string key; std::string sku; std::int32_t delta; std::int64_t balance; };
struct Repository {
    virtual ~Repository() = default;
    virtual void commit(const Movement&) = 0;
};
class MemoryRepository final : public Repository {
public:
    std::vector<Movement> events;
    void commit(const Movement& movement) override { events.push_back(movement); }
};
inline bool identifier(const std::string& text) {
    if (text.empty() || text.size() > 64) return false;
    for (unsigned char c : text) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
class Ledger {
    mutable std::mutex mutex_;
    Repository& repository_;
    std::map<std::string, std::int64_t> balances_;
    std::map<std::string, Movement> keys_;
    std::uint64_t sequence_ = 0;
public:
    explicit Ledger(Repository& repository) : repository_(repository) {}
    Movement move(const std::string& sku, std::int32_t delta, const std::string& key) {
        std::lock_guard lock(mutex_);
        if (!identifier(sku) || !identifier(key) || delta == 0) throw std::invalid_argument("invalid stock movement");
        if (const auto found = keys_.find(key); found != keys_.end()) {
            if (found->second.sku != sku || found->second.delta != delta) throw std::logic_error("idempotency conflict");
            return found->second;
        }
        auto balances = balances_; auto keys = keys_;
        auto& balance = balances[sku];
        if (delta > 0 && balance > std::numeric_limits<std::int64_t>::max() - delta) throw std::overflow_error("stock overflow");
        if (delta < 0 && balance < -(std::int64_t)delta) throw std::runtime_error("insufficient stock");
        balance += delta;
        Movement movement{sequence_ + 1, key, sku, delta, balance};
        keys.emplace(key, movement);
        repository_.commit(movement);
        balances_.swap(balances); keys_.swap(keys); ++sequence_;
        return movement;
    }
    std::map<std::string, std::int64_t> snapshot() const { std::lock_guard lock(mutex_); return balances_; }
};
}
