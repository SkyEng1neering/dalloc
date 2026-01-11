/**
 * @file test_basic.cpp
 * @brief Basic allocation and deallocation tests for dalloc
 */

#include "dalloc_test_config.h"

class BasicTest : public DallocTestFixture {};

// Test heap initialization
TEST_F(BasicTest, HeapInit_Success) {
    EXPECT_EQ(heap.offset, 0u);
    EXPECT_EQ(heap.total_size, TEST_HEAP_SIZE);
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap.mem, heap_memory);
}

TEST_F(BasicTest, HeapInit_ZeroesMemory) {
    // heap_init should zero the memory
    for (size_t i = 0; i < TEST_HEAP_SIZE; i++) {
        EXPECT_EQ(heap_memory[i], 0) << "Memory not zeroed at index " << i;
    }
}

// Test single allocation
TEST_F(BasicTest, SingleAlloc_ReturnsValidPtr) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));

    ASSERT_NE(ptr, nullptr);
    EXPECT_GE(ptr, heap_memory);
    EXPECT_LT(ptr, heap_memory + TEST_HEAP_SIZE);
}

TEST_F(BasicTest, MultipleAlloc_Sequential) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr3));

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Pointers should be sequential (with alignment)
    EXPECT_LT(ptr1, ptr2);
    EXPECT_LT(ptr2, ptr3);

    EXPECT_EQ(heap.alloc_info.allocations_num, 3u);
}

TEST_F(BasicTest, Alloc_ReturnsAligned) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 7, reinterpret_cast<void**>(&ptr));  // Odd size

    ASSERT_NE(ptr, nullptr);
    // Pointer should be 4-byte aligned
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 4, 0u);
}

// Allocation with size=0 should return NULL without allocating
TEST_F(BasicTest, Alloc_ZeroSize_ReturnsNull) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(0xDEADBEEF);  // Non-null initial
    dalloc(&heap, 0, reinterpret_cast<void**>(&ptr));

    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);  // No allocation
}

TEST_F(BasicTest, Alloc_UpdatesOffset) {
    uint32_t initial_offset = heap.offset;

    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));

    EXPECT_GT(heap.offset, initial_offset);
    EXPECT_GE(heap.offset, 16u);  // At least the requested size
}

TEST_F(BasicTest, Alloc_UpdatesAllocCount) {
    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);

    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr1));
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr2));
    EXPECT_EQ(heap.alloc_info.allocations_num, 2u);
}

// Test free operations
TEST_F(BasicTest, Free_Single_Success) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);

    EXPECT_EQ(heap.alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap.offset, 0u);
}

TEST_F(BasicTest, Free_UpdatesOffset) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr));
    uint32_t offset_after_alloc = heap.offset;

    dfree(&heap, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);

    EXPECT_LT(heap.offset, offset_after_alloc);
    EXPECT_EQ(heap.offset, 0u);
}

TEST_F(BasicTest, Free_NullHeap_NoOp) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));

    // Should not crash with null heap
    dfree(nullptr, reinterpret_cast<void**>(&ptr), USING_PTR_ADDRESS);

    // Original allocation should still exist
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(BasicTest, Free_InvalidPtr_NoOp) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));

    // Try to free a pointer that was never allocated
    uint8_t* fake_ptr = heap_memory + 500;
    dfree(&heap, reinterpret_cast<void**>(&fake_ptr), USING_PTR_ADDRESS);

    // Original allocation should still exist
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

// Test that allocated memory is usable
TEST_F(BasicTest, AllocatedMemory_IsUsable) {
    uint8_t* ptr = nullptr;
    const size_t alloc_size = 64;
    dalloc(&heap, alloc_size, reinterpret_cast<void**>(&ptr));

    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (size_t i = 0; i < alloc_size; i++) {
        ptr[i] = static_cast<uint8_t>(i);
    }

    // Read back and verify
    for (size_t i = 0; i < alloc_size; i++) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>(i)) << "Mismatch at index " << i;
    }
}
