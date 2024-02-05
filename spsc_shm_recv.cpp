//
// Created by OPBOY on 2/4/2024.
//

#include "spsc.h"

int main() {
    if (!pinCpu(3)) {
        std::cerr << "Failed to pin CPU\n";
        return 1;
    }
    std::cout << "Pinned CPU to 3\n";
    SampleMsgQueue* queue = getSampleMsgQueue();
    SampleMsg* msg = nullptr;
    while (true) {
        while ((msg = queue->front()) == nullptr) {}
        auto latency = rdtscp();
        latency -= msg->timestamp;
        std::cout << "Latency: " << latency << " cycles\n";
        queue->pop();
    }
}