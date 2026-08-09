#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace showcore {

template <typename Value, std::size_t StorageCapacity>
class SpscQueue {
    static_assert(StorageCapacity >= 2U);
    static_assert(std::is_default_constructible_v<Value>);
    static_assert(std::is_nothrow_copy_assignable_v<Value>);

public:
    static constexpr std::size_t capacity = StorageCapacity - 1U;

    [[nodiscard]] bool try_push(const Value& value) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == read_.load(std::memory_order_acquire)) {
            return false;
        }
        storage_[write] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(Value& value) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) {
            return false;
        }
        value = storage_[read];
        read_.store(increment(read), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_.load(std::memory_order_acquire) ==
            write_.load(std::memory_order_acquire);
    }

    void reset() noexcept {
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t value) noexcept {
        return (value + 1U) % StorageCapacity;
    }

    std::array<Value, StorageCapacity> storage_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};

}  // namespace showcore
