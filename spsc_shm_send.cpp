//
// Created by OPBOY on 2/4/2024.
//

#include "spsc.h"

int main() {
    if (!pinCpu(2)) {
        std::cerr << "Failed to pin CPU\n";
        return 1;
    }
    std::cout << "Pinned CPU to 2\n";
    SampleMsgQueue* queue = getSampleMsgQueue();
    SampleMsg* msg = nullptr;


    // test send message "0123456789012345678901234567890123456789", which is 40 bytes for 1000 times
    for (int i = 0; i < 1000; i++) {
        while ((msg = queue->alloc()) == nullptr) {}
        std::strcpy(msg->buffer, "0123456789012345678901234567890123456789");
        msg->timestamp = rdtscp();
        queue->push();
        // sleep for 1ms
        usleep(1000);
    }

    while (true) {
        while ((msg = queue->alloc()) == nullptr) {}
        std::cout << "input: " << std::flush;
        if (std::cin >> msg->buffer) {
            msg->timestamp = rdtscp();
            queue->push();
        } else {
            break;
        }
    }

    return 0;
}