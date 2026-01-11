/**
 * @file test_stress.cpp
 * @brief Stress tests for dalloc with canary-based memory corruption detection
 */

#include "dalloc_test_config.h"
#include <random>
#include <vector>
#include <algorithm>

class StressTest : public DallocLargeHeapFixture {};

TEST_F(StressTest, Stress_RapidAllocFree) {
    for (int cycle = 0; cycle < 1000; cycle++) {
        uint8_t* ptr = nullptr;
        dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            // Write some data
            ptr[0] = static_cast<uint8_t>(cycle & 0xFF);
            ptr[31] = static_cast<uint8_t>((cycle >> 8) & 0xFF);

            dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
        }

        // Periodically check canary integrity
        if (cycle % 100 == 0) {
            assertCanaryIntegrity();
        }
    }

    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap.offset, 0u);
}

TEST_F(StressTest, Stress_RandomPattern) {
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> size_dist(4, 128);
    std::uniform_int_distribution<int> op_dist(0, 2);

    // Use fixed array - dalloc tracks pointer ADDRESSES, so we can't move pointers
    // After freeing, just set to nullptr and track active count separately
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};
    size_t next_slot = 0;  // Next slot to allocate into
    size_t active_count = 0;  // Number of active (non-null) allocations

    for (int i = 0; i < 500; i++) {
        int op = op_dist(rng);

        if (op == 0 || active_count == 0) {
            // Allocate into next available slot
            if (next_slot < MAX_NUM_OF_ALLOCATIONS) {
                int size = size_dist(rng);
                dalloc(&heap, size, reinterpret_cast<void**>(&ptrs[next_slot]));
                if (ptrs[next_slot] != nullptr) {
                    ptrs[next_slot][0] = 0xAA;
                    next_slot++;
                    active_count++;
                }
            }
        } else if (op == 1 && active_count > 0) {
            // Free random active pointer - find a non-null one
            size_t attempts = 0;
            while (attempts < next_slot) {
                size_t idx = rng() % next_slot;
                if (ptrs[idx] != nullptr) {
                    dfree(&heap, reinterpret_cast<void**>(&ptrs[idx]), USING_PTR_ADDRESS);
                    // ptrs[idx] is now nullptr (dfree sets it)
                    active_count--;
                    break;
                }
                attempts++;
            }
        } else if (op == 2 && active_count > 0) {
            // Verify random active pointer
            size_t attempts = 0;
            while (attempts < next_slot) {
                size_t idx = rng() % next_slot;
                if (ptrs[idx] != nullptr) {
                    EXPECT_EQ(ptrs[idx][0], 0xAA) << "Data corruption at iteration " << i;
                    break;
                }
                attempts++;
            }
        }

        // Check canary every 50 iterations
        if (i % 50 == 0) {
            assertCanaryIntegrity();
        }
    }

    // Cleanup - free all remaining active pointers
    for (size_t i = 0; i < next_slot; i++) {
        if (ptrs[i] != nullptr) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }
    }
}

TEST_F(StressTest, Stress_FragmentationRecovery) {
    uint8_t* ptrs[20] = {nullptr};

    // Allocate many blocks
    for (int i = 0; i < 20; i++) {
        dalloc(&heap, 64, reinterpret_cast<void**>(&ptrs[i]));
        if (ptrs[i] != nullptr) {
            ptrs[i][0] = static_cast<uint8_t>(i);
        }
    }

    uint32_t max_offset = heap.offset;

    // Free every other block (create fragmentation)
    for (int i = 0; i < 20; i += 2) {
        if (ptrs[i] != nullptr) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }
    }

    assertCanaryIntegrity();

    // Defragmentation should have compacted memory
    EXPECT_LT(heap.offset, max_offset);

    // Remaining blocks should have correct data
    for (int i = 1; i < 20; i += 2) {
        if (ptrs[i] != nullptr) {
            EXPECT_EQ(ptrs[i][0], static_cast<uint8_t>(i))
                << "Data corruption in block " << i;
        }
    }

    // Cleanup
    for (int i = 1; i < 20; i += 2) {
        if (ptrs[i] != nullptr) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }
    }
}

TEST_F(StressTest, Stress_ManySmallAllocs) {
    // Use fixed array because dalloc tracks pointer ADDRESSES
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};
    size_t count = 0;

    // Allocate as many small blocks as possible
    for (size_t i = 0; i < MAX_NUM_OF_ALLOCATIONS; i++) {
        dalloc(&heap, 8, reinterpret_cast<void**>(&ptrs[i]));
        if (ptrs[i] == nullptr) break;
        ptrs[i][0] = static_cast<uint8_t>(i & 0xFF);
        count++;
    }

    assertCanaryIntegrity();
    EXPECT_GT(count, 0u);

    // Verify all data
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(ptrs[i][0], static_cast<uint8_t>(i & 0xFF));
    }

    // Cleanup
    for (size_t i = 0; i < count; i++) {
        dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
    }
}

TEST_F(StressTest, Stress_FewLargeAllocs) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 2048, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 2048, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 2048, reinterpret_cast<void**>(&ptr3));

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Fill each block
    std::memset(ptr1, 0x11, 2048);
    std::memset(ptr2, 0x22, 2048);
    std::memset(ptr3, 0x33, 2048);

    assertCanaryIntegrity();

    // Verify
    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ptr1[i], 0x11) << "Block 1 corruption at " << i;
        ASSERT_EQ(ptr2[i], 0x22) << "Block 2 corruption at " << i;
        ASSERT_EQ(ptr3[i], 0x33) << "Block 3 corruption at " << i;
    }
}

TEST_F(StressTest, Stress_MixedSizes) {
    // Use fixed arrays because dalloc tracks pointer ADDRESSES
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};
    size_t alloc_sizes[MAX_NUM_OF_ALLOCATIONS] = {0};
    uint8_t patterns[MAX_NUM_OF_ALLOCATIONS] = {0};
    size_t count = 0;

    size_t sizes[] = {4, 16, 64, 256, 512};

    for (int i = 0; i < 100 && count < MAX_NUM_OF_ALLOCATIONS; i++) {
        size_t size = sizes[i % 5];
        dalloc(&heap, size, reinterpret_cast<void**>(&ptrs[count]));

        if (ptrs[count] != nullptr) {
            uint8_t pattern = static_cast<uint8_t>(i & 0xFF);
            std::memset(ptrs[count], pattern, size);
            alloc_sizes[count] = size;
            patterns[count] = pattern;
            count++;
        }

        if (i % 20 == 0) {
            assertCanaryIntegrity();
        }
    }

    EXPECT_GT(count, 0u);

    // Verify all allocations
    for (size_t idx = 0; idx < count; idx++) {
        for (size_t j = 0; j < alloc_sizes[idx]; j++) {
            EXPECT_EQ(ptrs[idx][j], patterns[idx])
                << "Corruption in alloc " << idx << " at byte " << j;
        }
    }

    // Cleanup
    for (size_t i = 0; i < count; i++) {
        dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
    }
}

TEST_F(StressTest, Stress_FullCycle) {
    // Use fixed array because dalloc tracks pointer ADDRESSES, not values
    // When using std::vector, pointer addresses change during reallocation
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};

    for (int cycle = 0; cycle < 100; cycle++) {
        size_t count = 0;

        // Fill heap (limited by MAX_NUM_OF_ALLOCATIONS or heap size)
        for (size_t i = 0; i < MAX_NUM_OF_ALLOCATIONS; i++) {
            dalloc(&heap, 64, reinterpret_cast<void**>(&ptrs[i]));
            if (ptrs[i] == nullptr) break;
            ptrs[i][0] = static_cast<uint8_t>(i + 1);
            count++;
        }

        EXPECT_GT(count, 0u);

        // Free all - must use original pointer addresses
        for (size_t i = 0; i < count; i++) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }

        EXPECT_EQ(heap.offset, 0u);
        EXPECT_EQ(heap.alloc_info.allocations_num, 0u);

        // Check canary every 10 cycles
        if (cycle % 10 == 0) {
            assertCanaryIntegrity();
        }

        // Reset array for next cycle
        std::memset(ptrs, 0, sizeof(ptrs));
    }
}

TEST_F(StressTest, Stress_CanaryCheck_AfterManyOps) {
    std::mt19937 rng(777);
    // Use fixed array - dalloc tracks pointer ADDRESSES, can't move pointers
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};
    size_t next_slot = 0;
    size_t active_count = 0;

    for (int i = 0; i < 10000; i++) {
        int op = rng() % 3;

        if (op == 0 || active_count == 0) {
            // Allocate into next slot
            if (next_slot < MAX_NUM_OF_ALLOCATIONS) {
                int size = (rng() % 64) + 4;
                dalloc(&heap, size, reinterpret_cast<void**>(&ptrs[next_slot]));
                if (ptrs[next_slot] != nullptr) {
                    next_slot++;
                    active_count++;
                }
            }
        } else {
            // Free random active pointer
            size_t attempts = 0;
            while (attempts < next_slot) {
                size_t idx = rng() % next_slot;
                if (ptrs[idx] != nullptr) {
                    dfree(&heap, reinterpret_cast<void**>(&ptrs[idx]), USING_PTR_ADDRESS);
                    active_count--;
                    break;
                }
                attempts++;
            }
        }
    }

    // Final canary check - critical test
    assertCanaryIntegrity();

    // Cleanup
    for (size_t i = 0; i < next_slot; i++) {
        if (ptrs[i] != nullptr) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }
    }
}

TEST_F(StressTest, Stress_DefragHeavy_NoCorruption) {
    // This test specifically stresses defragmentation
    for (int round = 0; round < 50; round++) {
        uint8_t* ptrs[10] = {nullptr};

        // Allocate 10 blocks
        for (int i = 0; i < 10; i++) {
            dalloc(&heap, 128, reinterpret_cast<void**>(&ptrs[i]));
            if (ptrs[i] != nullptr) {
                std::memset(ptrs[i], i + 1, 128);
            }
        }

        // Free in reverse order (triggers lots of defrag)
        for (int i = 9; i >= 0; i--) {
            if (ptrs[i] != nullptr) {
                dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
            }
            assertCanaryIntegrity();
        }

        EXPECT_EQ(heap.offset, 0u);
    }
}

TEST_F(StressTest, Stress_BoundaryWrites_NoOverflow) {
    // Allocate and write to exact boundaries
    for (int i = 0; i < 100; i++) {
        uint8_t* ptr = nullptr;
        size_t size = 64;
        dalloc(&heap, size, reinterpret_cast<void**>(&ptr));

        if (ptr != nullptr) {
            // Write to first and last byte
            ptr[0] = 0xAA;
            ptr[size - 1] = 0xBB;

            // Fill entire block
            for (size_t j = 0; j < size; j++) {
                ptr[j] = static_cast<uint8_t>(j);
            }

            dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
        }

        // Check for overflow after each operation
        assertCanaryIntegrity();
    }
}

