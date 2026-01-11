/**
 * @file test_defrag.cpp
 * @brief Defragmentation tests for dalloc
 *
 * These tests verify the core functionality of dalloc - automatic
 * memory defragmentation after free operations.
 */

#include "dalloc_test_config.h"

class DefragTest : public DallocTestFixture {};

TEST_F(DefragTest, Defrag_SingleGap_Compacts) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));

    uint32_t offset_before = heap.offset;

    // Free first block - should trigger defragmentation
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // Offset should decrease (memory compacted)
    EXPECT_LT(heap.offset, offset_before);
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(DefragTest, Defrag_MultipleGaps_Compacts) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;
    uint8_t* ptr4 = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr3));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr4));

    // Free alternating blocks
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);
    dfree(&heap, reinterpret_cast<void**>(&ptr3), USING_PTR_ADDRESS);

    // Should have compacted to 2 allocations
    EXPECT_EQ(heap.alloc_info.allocations_num, 2u);
}

TEST_F(DefragTest, Defrag_PreservesData) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));

    // Write pattern to second block
    for (int i = 0; i < 32; i++) {
        ptr2[i] = static_cast<uint8_t>(0xAB + i);
    }

    // Free first block - ptr2 data should be moved
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // Verify data is preserved
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(ptr2[i], static_cast<uint8_t>(0xAB + i))
            << "Data corruption at index " << i;
    }
}

TEST_F(DefragTest, Defrag_UpdatesUserPointers) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));

    uint8_t* ptr2_before = ptr2;

    // Free first block - ptr2 should be updated
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // ptr2 should now point to where ptr1 was (start of heap)
    EXPECT_EQ(ptr2, heap_memory);
    EXPECT_NE(ptr2, ptr2_before);
}

TEST_F(DefragTest, Defrag_FirstBlock_Works) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr3));

    // Write identifiable data
    ptr2[0] = 0x22;
    ptr3[0] = 0x33;

    // Free first block
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // Check data moved correctly
    EXPECT_EQ(ptr2[0], 0x22);
    EXPECT_EQ(ptr3[0], 0x33);
    EXPECT_EQ(heap.alloc_info.allocations_num, 2u);
}

TEST_F(DefragTest, Defrag_LastBlock_Works) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr3));

    uint32_t offset_before = heap.offset;

    // Free last block - no data movement needed
    dfree(&heap, reinterpret_cast<void**>(&ptr3), USING_PTR_ADDRESS);

    EXPECT_LT(heap.offset, offset_before);
    EXPECT_EQ(heap.alloc_info.allocations_num, 2u);

    // First two blocks should be unchanged
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
}

TEST_F(DefragTest, Defrag_MiddleBlock_Works) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr3));

    // Write identifiable data
    ptr1[0] = 0x11;
    ptr3[0] = 0x33;

    uint8_t* ptr3_before = ptr3;

    // Free middle block
    dfree(&heap, reinterpret_cast<void**>(&ptr2), USING_PTR_ADDRESS);

    // ptr1 should be unchanged, ptr3 should move
    EXPECT_EQ(ptr1[0], 0x11);
    EXPECT_EQ(ptr3[0], 0x33);
    EXPECT_LT(ptr3, ptr3_before);  // ptr3 moved backward
}

TEST_F(DefragTest, Defrag_Alternating_Works) {
    uint8_t* ptrs[6] = {nullptr};

    // Allocate 6 blocks
    for (int i = 0; i < 6; i++) {
        dalloc(&heap, 32, reinterpret_cast<void**>(&ptrs[i]));
        ptrs[i][0] = static_cast<uint8_t>(i);  // Mark each block
    }

    EXPECT_EQ(heap.alloc_info.allocations_num, 6u);

    // Free even-indexed blocks (0, 2, 4)
    dfree(&heap, reinterpret_cast<void**>(&ptrs[0]), USING_PTR_ADDRESS);
    dfree(&heap, reinterpret_cast<void**>(&ptrs[2]), USING_PTR_ADDRESS);
    dfree(&heap, reinterpret_cast<void**>(&ptrs[4]), USING_PTR_ADDRESS);

    EXPECT_EQ(heap.alloc_info.allocations_num, 3u);

    // Remaining blocks (1, 3, 5) should have correct data
    EXPECT_EQ(ptrs[1][0], 1);
    EXPECT_EQ(ptrs[3][0], 3);
    EXPECT_EQ(ptrs[5][0], 5);
}

TEST_F(DefragTest, Defrag_MemoryContent_Correct) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;

    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr2));

    // Fill second block with pattern
    for (int i = 0; i < 64; i++) {
        ptr2[i] = static_cast<uint8_t>(i ^ 0x55);
    }

    // Free first block
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // Verify entire pattern
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(ptr2[i], static_cast<uint8_t>(i ^ 0x55))
            << "Pattern mismatch at index " << i;
    }
}

TEST_F(DefragTest, Defrag_PointerChain_AllUpdated) {
    uint8_t* ptrs[5] = {nullptr};

    // Allocate chain of blocks
    for (int i = 0; i < 5; i++) {
        dalloc(&heap, 20, reinterpret_cast<void**>(&ptrs[i]));
        // Store index in first byte
        ptrs[i][0] = static_cast<uint8_t>(i);
    }

    // Free first block - all subsequent pointers should update
    dfree(&heap, reinterpret_cast<void**>(&ptrs[0]), USING_PTR_ADDRESS);

    // Verify all remaining pointers are valid and data intact
    for (int i = 1; i < 5; i++) {
        ASSERT_NE(ptrs[i], nullptr) << "Pointer " << i << " is null";
        EXPECT_EQ(ptrs[i][0], static_cast<uint8_t>(i))
            << "Data corrupted in block " << i;
        EXPECT_GE(ptrs[i], heap_memory);
        EXPECT_LT(ptrs[i], heap_memory + TEST_HEAP_SIZE);
    }

    // Check ordering is maintained
    for (int i = 2; i < 5; i++) {
        EXPECT_GT(ptrs[i], ptrs[i-1]) << "Pointer order broken at " << i;
    }
}
