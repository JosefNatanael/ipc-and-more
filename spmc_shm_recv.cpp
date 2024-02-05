//
// Created by OPBOY on 2/4/2024.
//

#include "spmc.h"

int main() {
    const char* shm_name = "spmc_shm";
    auto* queue = shmMap<SampleSPMCQueue>(shm_name);
    if (!queue) {
        return 1;
    }
    auto reader = queue->getReader();
    std::cout << "reader size: " << sizeof(reader) << std::endl;

    while (true) {
        auto* msg = reader.readLast();
        if (!msg) {
            continue;
        }
        auto now = rdtscp();
        auto latency = now - msg->tsc;
        std::cout << "i: " << msg->idx << " latency: " << latency << std::endl;
    }

}