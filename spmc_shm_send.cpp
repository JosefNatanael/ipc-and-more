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

    uint64_t i = 0;

    while (true) {
        for (int j = 0; j < 3; ++j) {
            queue->write([i](SampleMsg& msg) {
                msg.idx = i;
                msg.tsc = rdtscp();
            });
            ++i;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}