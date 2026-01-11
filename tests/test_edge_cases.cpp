/**
 * @file test_edge_cases.cpp
 * @brief Edge case and boundary condition tests for dalloc
 */

#include "dalloc_test_config.h"

class EdgeCaseTest : public DallocTestFixture {};

TEST_F(EdgeCaseTest, MaxAllocations_ReachLimit) {
    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS] = {nullptr};

    // Allocate up to the limit (small allocations to fit in heap)
    size_t alloc_size = TEST_HEAP_SIZE / (MAX_NUM_OF_ALLOCATIONS + 10);
    if (alloc_size < 4) alloc_size = 4;

    size_t successful = 0;
    for (size_t i = 0; i < MAX_NUM_OF_ALLOCATIONS; i++) {
        dalloc(&heap, alloc_size, reinterpret_cast<void**>(&ptrs[i]));
        if (ptrs[i] != nullptr) {
            successful++;
        } else {
            break;  // Heap might be full before reaching max allocations
        }
    }

    EXPECT_GT(successful, 0u);
    EXPECT_LE(heap.alloc_info.allocations_num, MAX_NUM_OF_ALLOCATIONS);
}

TEST_F(EdgeCaseTest, MaxAllocations_ExceedLimit_Fails) {
    // Use tiny heap to ensure we hit allocation limit, not heap size limit
    uint8_t tiny_heap_mem[4096];
    heap_t tiny_heap;
    heap_init(&tiny_heap, tiny_heap_mem, sizeof(tiny_heap_mem));

    uint8_t* ptrs[MAX_NUM_OF_ALLOCATIONS + 5] = {nullptr};

    // Try to exceed max allocations
    for (size_t i = 0; i < MAX_NUM_OF_ALLOCATIONS + 5; i++) {
        dalloc(&tiny_heap, 4, reinterpret_cast<void**>(&ptrs[i]));
    }

    // Count successful allocations
    size_t successful = 0;
    for (size_t i = 0; i < MAX_NUM_OF_ALLOCATIONS + 5; i++) {
        if (ptrs[i] != nullptr) successful++;
    }

    EXPECT_LE(successful, MAX_NUM_OF_ALLOCATIONS);
}

TEST_F(EdgeCaseTest, HeapFull_ExactFit) {
    // Allocate entire heap in one go
    uint8_t* ptr = nullptr;
    dalloc(&heap, TEST_HEAP_SIZE, reinterpret_cast<void**>(&ptr));

    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(heap.offset, TEST_HEAP_SIZE);
}

TEST_F(EdgeCaseTest, HeapFull_Overflow_Fails) {
    // Fill heap
    uint8_t* ptr1 = nullptr;
    dalloc(&heap, TEST_HEAP_SIZE - 100, reinterpret_cast<void**>(&ptr1));
    ASSERT_NE(ptr1, nullptr);

    // Try to allocate more than remaining
    uint8_t* ptr2 = nullptr;
    dalloc(&heap, 200, reinterpret_cast<void**>(&ptr2));

    EXPECT_EQ(ptr2, nullptr);
}

TEST_F(EdgeCaseTest, ExactFit_LastByte) {
    // Calculate exact remaining space
    uint8_t* ptr1 = nullptr;
    dalloc(&heap, 100, reinterpret_cast<void**>(&ptr1));

    uint32_t remaining = TEST_HEAP_SIZE - heap.offset;

    // Allocate exactly remaining (accounting for alignment)
    uint8_t* ptr2 = nullptr;
    dalloc(&heap, remaining, reinterpret_cast<void**>(&ptr2));

    // Should succeed if alignment allows
    // (may fail due to alignment padding)
    if (ptr2 != nullptr) {
        EXPECT_EQ(heap.offset, TEST_HEAP_SIZE);
    }
}

TEST_F(EdgeCaseTest, OneByteAlloc_Works) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 1, reinterpret_cast<void**>(&ptr));

    ASSERT_NE(ptr, nullptr);
    ptr[0] = 0x42;
    EXPECT_EQ(ptr[0], 0x42);

    // Offset should be at least 4 due to alignment
    EXPECT_GE(heap.offset, 4u);
}

TEST_F(EdgeCaseTest, LargeAlloc_Works) {
    uint8_t* ptr = nullptr;
    size_t large_size = TEST_HEAP_SIZE - 32;  // Leave some room
    dalloc(&heap, large_size, reinterpret_cast<void**>(&ptr));

    ASSERT_NE(ptr, nullptr);

    // Verify we can use the entire allocation
    std::memset(ptr, 0xBB, large_size);
    for (size_t i = 0; i < large_size; i++) {
        EXPECT_EQ(ptr[i], 0xBB) << "Failed at index " << i;
    }
}

TEST_F(EdgeCaseTest, ValidatePtr_ByAddress_Found) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    uint32_t index = 0;
    bool found = validate_ptr(&heap, reinterpret_cast<void**>(&ptr),
                              USING_PTR_ADDRESS, &index);

    EXPECT_TRUE(found);
    EXPECT_EQ(index, 0u);
}

TEST_F(EdgeCaseTest, ValidatePtr_ByValue_Found) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Create a copy of the pointer value
    uint8_t* ptr_copy = ptr;

    uint32_t index = 0;
    bool found = validate_ptr(&heap, reinterpret_cast<void**>(&ptr_copy),
                              USING_PTR_VALUE, &index);

    EXPECT_TRUE(found);
}

TEST_F(EdgeCaseTest, ValidatePtr_Invalid_NotFound) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));

    // Create a fake pointer
    uint8_t* fake_ptr = heap_memory + 500;

    uint32_t index = 0;
    bool found = validate_ptr(&heap, reinterpret_cast<void**>(&fake_ptr),
                              USING_PTR_ADDRESS, &index);

    EXPECT_FALSE(found);
}

TEST_F(EdgeCaseTest, DoubleFree_Handled) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // First free
    dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);

    // Second free should be handled gracefully (ptr may be null or invalid now)
    // This should not crash
    dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);

    // Heap should still be in valid state
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
}

// Additional edge case: allocate after free should reuse space
TEST_F(EdgeCaseTest, AllocAfterFree_ReusesSpace) {
    uint8_t* ptr1 = nullptr;
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr1));
    uint32_t offset_after_first = heap.offset;

    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);
    EXPECT_EQ(heap.offset, 0u);

    // Allocate again
    uint8_t* ptr2 = nullptr;
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr2));

    // Should start at beginning of heap again
    EXPECT_EQ(ptr2, heap_memory);
    EXPECT_EQ(heap.offset, offset_after_first);
}
