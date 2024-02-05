#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <thread>

/**
 * Work stealing queue: a queue that allows
 * - one thread to push/pop items into/from one end of the queue
 * - multiple threads to steal items from the other end
 * Unlike a regular SPMC:
 * - Owner: FIFO (owner produces and consumes fresh jobs)
 * - Thief: LIFO (thief consumes stale jobs)
 * Idea: To better utilize cache:
 * keep the local cache warm on each thread,
 * each thread trying to only work on jobs recently produced prior jobs on the same thread.
 * Other thief threads may take stale jobs from other threads that are probably cold in cache
 *
 * This is a direct implementation from this paper: Correct and Efficient Work-Stealing for Weak Memory Models
 * Nhat Minh Le, Antoniu Pop, Albert Cohen, Francesco Zappa Nardelli
 * INRIA and ENS Paris
 */

template<typename T>
struct WorkStealingQueue {
    using allocator_type = std::pmr::polymorphic_allocator<>;

    struct Array {
        using allocator_type = std::pmr::polymorphic_allocator<>;

        const std::size_t capacity_;
        const std::size_t modulo_;
        std::atomic<T>* S_;
        allocator_type allocator_;

        Array(std::size_t cap, allocator_type alloc = {}) : capacity_{cap}, modulo_{cap - 1},
                                                            S_{alloc.allocate_object<std::atomic<T>>(cap)},
                                                            allocator_(alloc) {
        }

        ~Array() {
            allocator_.deallocate_object(S_, capacity_);
        }

        template<typename Obj>
        void push(uint64_t idx, Obj&& obj) noexcept {
            S_[idx & modulo_].store(std::forward<Obj>(obj), std::memory_order_relaxed);
        }

        // removes and retrieves
        T pop(uint64_t idx) noexcept {
            return S_[idx % modulo_].load(std::memory_order_relaxed);
        }

        // expensive operation
        Array* resize(uint64_t bottom, uint64_t top) {
            Array* ptr = allocator_.new_object<Array>(capacity_ * 2);
            for (uint64_t i = top; i != bottom; ++i) {
                ptr->push(i, this->pop(i));
            }
            return ptr;
        }
    };


    // to avoid false sharing, put atomic variables on different cache lines
#ifdef __cpp_lib_hardware_interference_size
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> top_;
    alignas(std::hardware_destructive_interference_size) std::atomic<uint64_t> bottom_;
#else
    alignas(64) std::atomic<uint64_t> top_;
    alignas(64) std::atomic<uint64_t> bottom_;
#endif
    std::atomic<Array*> array_;
    std::pmr::vector<Array*> garbage_;

    explicit WorkStealingQueue(uint64_t capacity, allocator_type alloc = {}) : garbage_(alloc) {
        // only powers of 2 accepted for capacity
        assert(capacity && (!(capacity & (capacity - 1))));
        top_.store(0, std::memory_order_relaxed);
        bottom_.store(0, std::memory_order_relaxed);
        array_.store(alloc.new_object<Array>(capacity), std::memory_order_relaxed);
        garbage_.reserve(32);
    }

    ~WorkStealingQueue() {
        for (auto a: garbage_) {
            garbage_.get_allocator().deallocate_object(a);
        }
        // not actually atomic load, just a regular load is needed
        garbage_.get_allocator().deallocate_object(array_.load());
    }

    template<typename Obj>
    void push(Obj&& obj) {
        auto b = bottom_.load(std::memory_order_relaxed);
        // acquire here is important, we need updated b and t at this point
        auto t = top_.load(std::memory_order_acquire);

        /* [[start of "critical section"]] */
        auto* a = array_.load(std::memory_order_relaxed);

        // queue is full
        if (a->capacity_ - 1 < b - t) {
            auto* tmp = a->resize(b, t);
            garbage_.push_back(a);
            std::swap(a, tmp);
            array_.store(a, std::memory_order_relaxed);
        }

        a->push(b, std::forward<Obj>(obj));
        /* [[end of "critical section"]] */
        std::atomic_thread_fence(std::memory_order_release); // release to all threads
        bottom_.store(b + 1, std::memory_order_relaxed);
    }

    std::optional<T> steal() {
        auto t = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        auto b = bottom_.load(std::memory_order_acquire);
        std::optional<T> item;
        if (t < b) {
            auto* a = array_.load(std::memory_order_consume);
            item = a->pop(t);
            if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return std::nullopt;
            }
        }

        return item;
    }

    std::optional<T> pop() {
        auto b = bottom_.load(std::memory_order_relaxed) - 1;
        auto* a = array_.load(std::memory_order_relaxed);
        bottom_.store(b, std::memory_order_relaxed);
        // this generates a full barrier
        std::atomic_thread_fence(std::memory_order_seq_cst);

        auto t = top_.load(std::memory_order_relaxed);
        std::optional<T> item;
        if (t <= b) {
            item = a->pop(b);
            if (t == b) {
                // last item just got stolen
                if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    item = std::nullopt;
                }
                bottom_.store(b + 1, std::memory_order_relaxed);
            }
        } else {
            bottom_.store(b + 1, std::memory_order_relaxed);
        }
        return item;
    }


    [[nodiscard]] bool empty() const noexcept {
        auto b = bottom_.load(std::memory_order_relaxed);
        auto t = bottom_.load(std::memory_order_relaxed);
        return b <= t;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        auto b = bottom_.load(std::memory_order_relaxed);
        auto t = bottom_.load(std::memory_order_relaxed);
        return b >= t ? b - t : 0;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return array_.load(std::memory_order_relaxed);
    }
};

int main() {
    std::array<std::byte, 4096> buffer{};
    std::pmr::monotonic_buffer_resource mem_resource(buffer.data(), buffer.size());
    WorkStealingQueue<int> queue(1024, &mem_resource);

    // only one thread can push and pop
    std::jthread owner([&]() {
        for (int i = 0; i < 1'000'000; ++i) {
            queue.push(i);
        }
    });

    // multiple threads can steal from queue
    std::jthread thief([&queue] {
        while (!queue.empty()) {
            std::cout << "stolen" << std::endl;
            std::optional<int> item = queue.steal();
        }
    });

    return 0;
}
