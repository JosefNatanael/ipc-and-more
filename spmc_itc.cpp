//
// Created by OPBOY on 2/4/2024.
//

#include "spmc.h"

#include <thread>
#include <vector>

const uint64_t max_msg = 10'000;
SampleSPMCQueue queue_{};

void read_thread(int cpu, std::latch& latch) {
    if (!pinCpu(cpu)) {
        exit(1);
    }
    auto reader = queue_.getReader();
    latch.arrive_and_wait();
    uint64_t total = 0;
    uint64_t count = 0;
    while (true) {
        auto* msg = reader.read();
        if (!msg) {
            continue;
        }
        auto now = rdtscp();
        auto latency = now - msg->tsc;
        total += latency;
        ++count;
        if (msg->idx == max_msg - 1) {
            std::cout << "avg latency: " << total / count << " drop count: " << max_msg - count << std::endl;
            return;
        }
    }
}

int main() {
    std::latch latch(5);
    std::vector<std::jthread> reader_threads;
    for (int i = 0; i < 4; ++i) {
        reader_threads.emplace_back(read_thread, i + 2, std::ref(latch));
    }

    if (!pinCpu(1)) {
        exit(1);
    }
    latch.arrive_and_wait();
    std::cout << "Producer running\n";
    for (uint64_t i = 0; i < max_msg; ++i) {
        queue_.write([i](SampleMsg& msg) {
            msg.idx = i;
            msg.tsc = rdtscp();
        });
        auto expire = rdtscp() + 1'000;
        while (rdtscp() < expire) {
            continue;
        }
    }
    std::cout << "Producer stopped\n";

    return 0;
}