//
// Created by OPBOY on 2/4/2024.
//

#ifndef CONCURRENCY_UTILS_H
#define CONCURRENCY_UTILS_H

#include <bits/stdc++.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * rdtsc: read time-stamp counter, a 64-bit register that counts the number of CPU cycles since the last reset
 * might not be accurate on modern CPUs due to out-of-order execution
 */
inline unsigned long long rdtsc() {
    return __builtin_ia32_rdtsc();
}

/**
 * rdtscp: read time-stamp counter and processor ID
 * prevents reordering of the instruction around the rdtscp
 */
inline unsigned long long rdtscp() {
    unsigned int dummy;
    return __builtin_ia32_rdtscp(&dummy);
}

/**
 * CPU pinning: to avoid context switching
 */
bool pinCpu(int cpuid) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpuid, &s);

    // pid = 0 => current process
    if (sched_setaffinity(0, sizeof(cpu_set_t), &s) != 0) {
        std::cout << "Failed to pin CPU" << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

/**
 * Shared memory mapping
 */
template<typename T>
T* shmMap(const char* filename) {
    // 1. create shared memory of size 0 initially
    int shm_fd = shm_open(filename, O_CREAT | O_RDWR, 0666);  // create if not exist, read/write
    if (shm_fd == -1) {
        std::cerr << "Failed to open shared memory" << strerror(errno) << std::endl;
        return nullptr;
    }
    // 2. change the size of our shared memory area
    if (ftruncate(shm_fd, sizeof(T))) {
        std::cerr << "Failed to truncate shared memory" << strerror(errno) << std::endl;
        return nullptr;
    }
    // 3. map the shared memory to our address space
    // mmap establishes a mapping between the address space of the process and a file that is associated with the fd at offset for len bytes
    T* ptr = static_cast<T*>(mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    close(shm_fd);  // close the file descriptor (not the shared memory itself)
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory" << strerror(errno) << std::endl;
        return nullptr;
    }
    return ptr;
}

#endif //CONCURRENCY_UTILS_H
