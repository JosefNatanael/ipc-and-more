//
// Created by OPBOY on 2/4/2024.
//

#include <array>
#include <thread>
#include "spsc.h"

struct Msg {
    int32_t val_len_;
    uint64_t ts_;
    std::array<long, 4> val_;
};

using SpscQueue = SPSCQueue<Msg, 1024>;
SpscQueue queue_{};

const int loop = 1'000'000;
const int sleep_cycles = 1'000;
uint64_t alloc_lat = 0;
uint64_t push_lat = 0;

void sender() {
    if (!pinCpu(6)) {
        exit(1);
    }

    auto* queue = &queue_;
    int g_val = 0;
    while (g_val < loop) {
        auto t1 = rdtscp();
        auto* msg = queue->alloc();
        auto t2 = rdtscp();
        if (!msg) continue;
        int val_len = rand() % 4 + 1;
        msg->val_len_ = val_len;
        for (int i = 0; i < val_len; i++) {
            msg->val_[i] = ++g_val;
        }
        auto t3 = rdtscp();
        msg->ts_ = t3;
        queue->push();
        auto t4 = rdtscp();
        alloc_lat += t2 - t1;
        push_lat += t4 - t3;
        const auto expire = rdtsc() + sleep_cycles;
        while (rdtsc() < expire) {
        }
    }
}

void receiver() {
    if (!pinCpu(7)) {
        exit(1);
    }
    auto before = rdtscp();
    for (int i = 0; i < 99; i++) rdtscp();
    auto after = rdtscp();
    auto rdtscp_lat = (after - before) / 100;

    auto* q = &queue_;

    int cnt = 0;
    long sum_lat = 0;
    int g_val = 0;
    uint64_t front_lat = 0;
    uint64_t pop_lat = 0;
    while (g_val < loop) {
        auto t1 = rdtscp();
        Msg* msg = q->front();
        auto t2 = rdtscp();
        if (!msg) {
            continue;
        }
        sum_lat += t2 - msg->ts_;
        cnt++;
        for (int i = 0; i < msg->val_len_; i++) {
            ++g_val;
            assert(msg->val[i] == g_val);
        }
        auto t3 = rdtscp();
        q->pop();
        auto t4 = rdtscp();
        front_lat += t2 - t1;
        pop_lat += t4 - t3;

    }
    std::cout << "recv done, val: " << g_val << " rdtscp_lat: " << rdtscp_lat << " avg_lat: " << (sum_lat / cnt)
              << " alloc_lat: " << (alloc_lat / cnt - rdtscp_lat) << " push_lat: " << (push_lat / cnt - rdtscp_lat)
              << " front_lat: " << (front_lat / cnt - rdtscp_lat) << " pop_lat: " << (pop_lat / cnt - rdtscp_lat)
              << std::endl;
}

int main() {
    std::jthread recv(receiver);
    std::jthread send(sender);

    return 0;
}

