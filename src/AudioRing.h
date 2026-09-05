// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioTypes.h"
#include <atomic>

namespace Luma {
// One producer, one consumer. Only the consumer advances readIndex.
// Full queues drop the incoming block; the producer never overwrites a reader.
class AudioRing {
public:
    static constexpr uint32_t Capacity = 16;
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    bool push(const AudioBlock &block) noexcept {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        if (write - readIndex.load(std::memory_order_acquire) == Capacity) {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        blocks[write % Capacity] = block;
        writeIndex.store(write + 1, std::memory_order_release);
        return true;
    }
    bool pop(AudioBlock &block) noexcept {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire)) return false;
        block = blocks[read % Capacity];
        readIndex.store(read + 1, std::memory_order_release);
        return true;
    }
    void keepNewest(uint32_t count) noexcept {
        const auto write = writeIndex.load(std::memory_order_acquire);
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (write - read > count) readIndex.store(write - count, std::memory_order_release);
    }
    uint32_t droppedBlocks() const { return dropped.load(std::memory_order_relaxed); }
private:
    std::array<AudioBlock, Capacity> blocks{};
    alignas(64) std::atomic<uint32_t> writeIndex{0};
    alignas(64) std::atomic<uint32_t> readIndex{0};
    std::atomic<uint32_t> dropped{0};
};
}
