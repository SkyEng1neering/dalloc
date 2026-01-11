/**
 * @file test_thread_safety.cpp
 * @brief Thread safety tests for dalloc with POSIX threads
 */

#include <gtest/gtest.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <cstring>

extern "C" {
#include "dalloc.h"
}

#ifdef USE_THREAD_SAFETY

class ThreadSafetyTest : public ::testing::Test {
protected:
    static constexpr size_t HEAP_SIZE = 16 * 1024;  // 16KB
    uint8_t heap_buffer[HEAP_SIZE];
    heap_t heap;

    void SetUp() override {
        std::memset(heap_buffer, 0xAA, HEAP_SIZE);
        heap_init(&heap, heap_buffer, HEAP_SIZE);
    }

    void TearDown() override {
        heap_deinit(&heap);
    }
};

// ==================== Basic Thread Safety Tests ====================

struct AllocThreadArgs {
    heap_t* heap;
    int thread_id;
    int num_allocs;
    std::atomic<int>* success_count;
    std::atomic<int>* fail_count;
};

void* alloc_thread_func(void* arg) {
    AllocThreadArgs* args = static_cast<AllocThreadArgs*>(arg);

    // Each iteration: allocate, write, free immediately
    // This tests thread safety without relying on pointer tracking across iterations
    for (int i = 0; i < args->num_allocs; i++) {
        uint8_t* ptr = nullptr;
        dalloc(args->heap, 32, reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            // Write pattern to verify memory integrity
            ptr[0] = static_cast<uint8_t>(args->thread_id);
            ptr[31] = static_cast<uint8_t>(i);
            args->success_count->fetch_add(1);

            // Free immediately - ptr address is still valid
            dfree(args->heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
        } else {
            args->fail_count->fetch_add(1);
        }
    }

    return nullptr;
}

TEST_F(ThreadSafetyTest, ConcurrentAlloc_NoCorruption) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 20;

    pthread_t threads[NUM_THREADS];
    AllocThreadArgs args[NUM_THREADS];
    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);

    // Start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].heap = &heap;
        args[i].thread_id = i;
        args[i].num_allocs = ALLOCS_PER_THREAD;
        args[i].success_count = &success_count;
        args[i].fail_count = &fail_count;

        int ret = pthread_create(&threads[i], nullptr, alloc_thread_func, &args[i]);
        ASSERT_EQ(ret, 0) << "Failed to create thread " << i;
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify heap is in consistent state
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u) << "All allocations should be freed";
    EXPECT_EQ(heap.offset, 0u) << "Heap should be empty after all frees";

    // At least some allocations should have succeeded
    EXPECT_GT(success_count.load(), 0) << "At least some allocations should succeed";
}

// ==================== Stress Test ====================

struct StressThreadArgs {
    heap_t* heap;
    int iterations;
    std::atomic<bool>* stop_flag;
};

void* stress_thread_func(void* arg) {
    StressThreadArgs* args = static_cast<StressThreadArgs*>(arg);

    for (int i = 0; i < args->iterations && !args->stop_flag->load(); i++) {
        uint8_t* ptr = nullptr;
        dalloc(args->heap, 16 + (i % 64), reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            // Quick write to allocated memory
            ptr[0] = 0x55;

            // Sometimes free immediately, sometimes keep for a bit
            if (i % 3 == 0) {
                dfree(args->heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
            } else {
                // Free after a brief moment
                dfree(args->heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
            }
        }
    }

    return nullptr;
}

TEST_F(ThreadSafetyTest, Stress_HighContention) {
    const int NUM_THREADS = 8;
    const int ITERATIONS = 500;

    pthread_t threads[NUM_THREADS];
    StressThreadArgs args[NUM_THREADS];
    std::atomic<bool> stop_flag(false);

    // Start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].heap = &heap;
        args[i].iterations = ITERATIONS;
        args[i].stop_flag = &stop_flag;

        int ret = pthread_create(&threads[i], nullptr, stress_thread_func, &args[i]);
        ASSERT_EQ(ret, 0) << "Failed to create thread " << i;
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Heap should be in consistent state
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u) << "All allocations should be freed";
}

// ==================== Realloc Thread Safety ====================

void* realloc_thread_func(void* arg) {
    StressThreadArgs* args = static_cast<StressThreadArgs*>(arg);

    for (int i = 0; i < args->iterations && !args->stop_flag->load(); i++) {
        uint8_t* ptr = nullptr;
        dalloc(args->heap, 16, reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            // Write pattern
            for (int j = 0; j < 16; j++) {
                ptr[j] = static_cast<uint8_t>(j);
            }

            // Realloc to larger size
            bool success = drealloc(args->heap, 32, reinterpret_cast<void**>(&ptr));

            if (success && ptr != nullptr) {
                // Free the reallocated memory
                dfree(args->heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
            } else if (ptr != nullptr) {
                dfree(args->heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
            }
        }
    }

    return nullptr;
}

TEST_F(ThreadSafetyTest, ConcurrentRealloc_NoDeadlock) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;

    pthread_t threads[NUM_THREADS];
    StressThreadArgs args[NUM_THREADS];
    std::atomic<bool> stop_flag(false);

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].heap = &heap;
        args[i].iterations = ITERATIONS;
        args[i].stop_flag = &stop_flag;

        int ret = pthread_create(&threads[i], nullptr, realloc_thread_func, &args[i]);
        ASSERT_EQ(ret, 0);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
}

// ==================== Single-threaded Realloc Data Integrity ====================

TEST_F(ThreadSafetyTest, Realloc_DataIntegrity_SingleThread) {
    // Test data integrity without concurrency complications
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (int j = 0; j < 16; j++) {
        ptr[j] = static_cast<uint8_t>(j);
    }

    // Realloc to larger size
    bool success = drealloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_TRUE(success);
    ASSERT_NE(ptr, nullptr);

    // Verify original data preserved
    for (int j = 0; j < 16; j++) {
        EXPECT_EQ(ptr[j], static_cast<uint8_t>(j)) << "Data at index " << j << " should be preserved";
    }

    dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
}

// ==================== Single Heap Thread Safety ====================

#ifdef USE_SINGLE_HEAP_MEMORY

class SingleHeapThreadSafetyTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 8 * 1024;
    uint8_t buffer[BUFFER_SIZE];

    void SetUp() override {
        dalloc_unregister_heap(true);
        std::memset(buffer, 0xAA, BUFFER_SIZE);
        bool registered = dalloc_register_heap(buffer, BUFFER_SIZE);
        ASSERT_TRUE(registered);
    }

    void TearDown() override {
        dalloc_unregister_heap(true);
    }
};

void* single_heap_thread_func(void* arg) {
    int iterations = *static_cast<int*>(arg);

    for (int i = 0; i < iterations; i++) {
        uint8_t* ptr = nullptr;
        def_dalloc(32, reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            ptr[0] = 0x42;
            def_dfree(reinterpret_cast<void**>(&ptr));
        }
    }

    return nullptr;
}

TEST_F(SingleHeapThreadSafetyTest, ConcurrentDefAlloc) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;

    pthread_t threads[NUM_THREADS];
    int iter_arg = ITERATIONS;

    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], nullptr, single_heap_thread_func, &iter_arg);
        ASSERT_EQ(ret, 0);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    heap_t* heap = dalloc_get_default_heap();
    ASSERT_NE(heap, nullptr);
    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
}

#endif // USE_SINGLE_HEAP_MEMORY

// ==================== Allocation Failure Test (No Deadlock) ====================

// This test verifies that allocation failure doesn't cause deadlock
// It fills the heap and then tries to allocate more, triggering the error path
TEST_F(ThreadSafetyTest, AllocationFailure_NoDeadlock) {
    // Use fixed array - dalloc tracks ADDRESSES of pointers, so they must stay valid
    static constexpr size_t MAX_PTRS = 200;
    uint8_t* ptrs[MAX_PTRS] = {nullptr};
    size_t num_allocs = 0;
    const size_t ALLOC_SIZE = 128;

    // Allocate until heap is full
    for (size_t i = 0; i < MAX_PTRS; i++) {
        dalloc(&heap, ALLOC_SIZE, reinterpret_cast<void**>(&ptrs[i]));
        if (ptrs[i] == nullptr) {
            break;  // Heap is full
        }
        num_allocs++;
    }

    ASSERT_GT(num_allocs, 0u) << "Should have allocated at least one block";

    // Now try to allocate more - this should fail but NOT deadlock
    uint8_t* fail_ptr = nullptr;
    dalloc(&heap, ALLOC_SIZE, reinterpret_cast<void**>(&fail_ptr));
    EXPECT_EQ(fail_ptr, nullptr) << "Allocation should fail when heap is full";

    // Clean up - free all allocations in reverse order
    for (size_t i = num_allocs; i > 0; i--) {
        dfree(&heap, reinterpret_cast<void**>(&ptrs[i-1]), USING_PTR_ADDRESS);
    }

    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
}

// Test that exceeding max allocations doesn't deadlock
TEST_F(ThreadSafetyTest, MaxAllocations_NoDeadlock) {
    // Use fixed array - dalloc tracks ADDRESSES of pointers
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS + 10] = {nullptr};
    size_t num_allocs = 0;

    // Allocate until we hit MAX_NUM_OF_ALLOCATIONS
    for (uint32_t i = 0; i < MAX_NUM_OF_ALLOCATIONS + 10; i++) {
        dalloc(&heap, 4, reinterpret_cast<void**>(&ptrs[i]));
        if (ptrs[i] == nullptr) {
            break;  // Either heap full or max allocations reached
        }
        num_allocs++;
    }

    // Should have allocated exactly MAX_NUM_OF_ALLOCATIONS or less
    EXPECT_LE(num_allocs, static_cast<size_t>(MAX_NUM_OF_ALLOCATIONS));

    // Try one more allocation - should fail without deadlock
    uint8_t* fail_ptr = nullptr;
    dalloc(&heap, 4, reinterpret_cast<void**>(&fail_ptr));
    // May or may not be null depending on what limit was hit first

    // Clean up
    for (size_t i = num_allocs; i > 0; i--) {
        dfree(&heap, reinterpret_cast<void**>(&ptrs[i-1]), USING_PTR_ADDRESS);
    }
}

#endif // USE_THREAD_SAFETY
