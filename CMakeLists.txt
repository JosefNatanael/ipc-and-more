cmake_minimum_required(VERSION 3.25)
project(concurrency)

set(CMAKE_CXX_STANDARD 20)

add_executable(main main.cpp)
add_executable(wsq wsq.cpp)
add_executable(spsc_itc spsc_itc.cpp spsc.h)
add_executable(spsc_shm_recv spsc_shm_recv.cpp spsc.h)
add_executable(spsc_shm_send spsc_shm_send.cpp spsc.h)

add_executable(spmc_itc spmc_itc.cpp spmc.h)
add_executable(spmc_shm_recv spmc_shm_recv.cpp spmc.h)
add_executable(spmc_shm_send spmc_shm_send.cpp spmc.h)
