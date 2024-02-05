//
// Created by OPBOY on 2/4/2024.
//

#ifndef CONCURRENCY_SPMC_H
#define CONCURRENCY_SPMC_H

#include "utils.h"

#include <atomic>

/**
 * SPMCQueue
 * key features:
 * - writer won't wait for reader, without knowing the existence of readers
 * - reader's responsibility to not fall behind too much, much like udp multicasting
 * - it "multicasts" within a host among threads or processes
 * - readers should poll since it is non-blocking
 *
 * If you want an MPMC, you can create multiple SPMCQueues
 * Otherwise, simply replace ++write_idx with an atomic fetch_add (bad design because producer contention)
 */

template<typename T, std::uint32_t Cnt>
struct SPMCQueue {
    static_assert(Cnt && !(Cnt & (Cnt - 1)), "Cnt must be a power of 2");

    struct alignas(64) Block {
        std::atomic<std::uint32_t> idx_{0};
        T data;
    };

    struct Reader {
        SPMCQueue<T, Cnt>* queue_{nullptr};
        std::uint32_t next_idx_{};

        Reader(SPMCQueue<T, Cnt>* queue, std::uint32_t next_idx) : queue_(queue), next_idx_(next_idx) {}

        T* read() {
            auto& block = queue_->blocks_[next_idx_ & (Cnt - 1)];
            auto new_idx = block.idx_.load(std::memory_order_acquire);
            if (static_cast<int64_t>(new_idx) - static_cast<int64_t>(next_idx_) < 0) {
                return nullptr;
            }
            next_idx_ = new_idx + 1;
            return &block.data;
        }

        T* readLast() {
            T* ret = nullptr;
            // read until the last available
            while (auto p = read()) {
                ret = p;
            }
            return ret;
        }

        explicit operator bool() const { return queue_; }
    };

    std::array<Block, Cnt> blocks_;
    alignas(64) std::uint32_t write_idx_;  // does not need to be atomic

    // readers should be set up before writers begin writing
    // does not support dynamic subscription of readers
    Reader getReader() {
        return Reader(this, write_idx_ + 1);
    }

    // Writer is a function that takes a reference to a block data and writes to it
    template<typename Writer>
    void write(Writer writer) {
        ++write_idx_;
        auto& block = blocks_[write_idx_ & (Cnt - 1)];  // this is equivalent to write_idx_ % Cnt
        writer(block.data);
        block.idx_.store(write_idx_, std::memory_order_release);
    }
};

struct SampleMsg {
    std::uint64_t tsc;
    std::uint64_t idx;
    std::array<char, 64> data;
};

using SampleSPMCQueue = SPMCQueue<SampleMsg, 1024>;

#endif //CONCURRENCY_SPMC_H
