//
// Created by OPBOY on 2/4/2024.
//
#pragma once

#include "utils.h"

#include <array>
#include <atomic>
#include <iostream>

/**
 * SPSC Queue for ITC and IPC (linux)
 * Latency should hover around 50-100ns between two CPU cores on the same node for a 10-200B message.
 */

template<typename T, std::uint32_t Cnt>
struct SPSCQueue {
    // must be a power of 2 to use the modulo trick
    static_assert(Cnt && !(Cnt & (Cnt - 1)), "Cnt must be a power of 2");

    // avoid false sharing
#ifdef __cpp_lib_hardware_interference_size
    alignas(std::hardware_destructive_interference_size) std::array<T, Cnt> data_;
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> write_idx_{0};
    alignas(std::hardware_destructive_interference_size) std::size_t read_idx_cache_{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> read_idx_{0};
#else
    alignas(128) std::array<T, Cnt> data_{};
    alignas(128) std::atomic<std::size_t> write_idx_{0};
    alignas(128) std::size_t read_idx_cache_{0};
    alignas(128) std::atomic<std::size_t> read_idx_{0};
#endif

    T* alloc() {
        const auto write_idx = write_idx_.load(std::memory_order_relaxed);
        if (write_idx - read_idx_cache_ == Cnt) {
            read_idx_cache_ = read_idx_.load(std::memory_order_acquire);
            if (__builtin_expect(write_idx - read_idx_cache_ == Cnt, 0)) {
                // expect false that the queue is full
                return nullptr;
            }
        }
        return &data_[write_idx_ & (Cnt - 1)]; // this is equivalent to write_idx_ % Cnt
    }

    void push() {
        write_idx_.fetch_add(1, std::memory_order_release);
    }

    template<typename Writer>
    bool tryPush(Writer writer) {
        T* p = alloc();
        if (!p) return false;
        writer(p);
        push();
        return true;
    }

    template<typename Writer>
    void blockPush(Writer writer) {
        while (!tryPush(writer)) {}
    }

    T* front() {
        auto read_idx = read_idx_.load(std::memory_order_relaxed);
        auto write_idx = write_idx_.load(std::memory_order_acquire);
        if (read_idx == write_idx) {
            return nullptr;
        }
        return &data_[read_idx & (Cnt - 1)];  // this is equivalent to read_idx % Cnt
    }

    void pop() {
        read_idx_.fetch_add(1, std::memory_order_release);
    }

    template<typename Reader>
    bool tryPop(Reader reader) {
        T* p = front();
        if (!p) return false;
        reader(p);
        pop();
        return true;
    }
};

struct SampleMsg {
    std::uint64_t timestamp;
    char buffer[56];
};

using SampleMsgQueue = SPSCQueue<SampleMsg, 8>;

SampleMsgQueue* getSampleMsgQueue() {
    return shmMap<SampleMsgQueue>("sample_msg_queue");
}
