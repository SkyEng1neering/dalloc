/**
 * @file test_realloc.cpp
 * @brief Reallocation and pointer replacement tests for dalloc
 */

#include "dalloc_test_config.h"

class ReallocTest : public DallocTestFixture {};

TEST_F(ReallocTest, Realloc_Grow_Success) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (int i = 0; i < 32; i++) {
        ptr[i] = static_cast<uint8_t>(i);
    }

    uint32_t offset_before = heap.offset;

    // Grow allocation
    bool result = drealloc(&heap, 64, reinterpret_cast<void**>(&ptr));

    EXPECT_TRUE(result);
    EXPECT_GT(heap.offset, offset_before);

    // Original data should be preserved
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>(i)) << "Data lost at index " << i;
    }
}

TEST_F(ReallocTest, Realloc_Shrink_Success) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 64, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (int i = 0; i < 64; i++) {
        ptr[i] = static_cast<uint8_t>(i ^ 0x55);
    }

    uint32_t offset_before = heap.offset;

    // Shrink allocation
    bool result = drealloc(&heap, 32, reinterpret_cast<void**>(&ptr));

    EXPECT_TRUE(result);
    EXPECT_LE(heap.offset, offset_before);

    // Data in the remaining portion should be preserved
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>(i ^ 0x55)) << "Data lost at index " << i;
    }
}

TEST_F(ReallocTest, Realloc_SameSize_Success) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Realloc to same size
    bool result = drealloc(&heap, 32, reinterpret_cast<void**>(&ptr));

    EXPECT_TRUE(result);
    EXPECT_NE(ptr, nullptr);
    // Allocation count should stay same
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(ReallocTest, Realloc_PreservesData) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 48, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Fill with complex pattern
    for (int i = 0; i < 48; i++) {
        ptr[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    // Grow
    drealloc(&heap, 96, reinterpret_cast<void**>(&ptr));

    // Verify original data
    for (int i = 0; i < 48; i++) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>((i * 7 + 13) % 256))
            << "Data corruption at index " << i;
    }
}

TEST_F(ReallocTest, Realloc_InvalidPtr_Fails) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));

    // Try to realloc a fake pointer
    uint8_t* fake_ptr = heap_memory + 500;
    bool result = drealloc(&heap, 64, reinterpret_cast<void**>(&fake_ptr));

    EXPECT_FALSE(result);
    // Original allocation should be intact
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(ReallocTest, Realloc_ToZero_BehaviorTest) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Realloc to zero - behavior depends on implementation
    // May fail, act as free, or allocate minimum size
    bool result = drealloc(&heap, 0, reinterpret_cast<void**>(&ptr));

    // Either succeeds or fails, but heap should remain valid
    if (result) {
        // If succeeded, allocation count may be 1 (reallocated) or less
        EXPECT_LE(heap.alloc_info.allocations_num, 1u);
    } else {
        // If failed, original allocation should still exist
        EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
    }
}

TEST_F(ReallocTest, ReplacePointers_Success) {
    uint8_t* ptr1 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    ASSERT_NE(ptr1, nullptr);

    uint8_t* ptr1_value = ptr1;  // Save original value

    // Replace ptr1 with a new pointer location
    uint8_t* new_ptr = nullptr;
    replace_pointers(&heap, reinterpret_cast<void**>(&ptr1),
                     reinterpret_cast<void**>(&new_ptr));

    // new_ptr should now have the original ptr1 value
    EXPECT_EQ(new_ptr, ptr1_value);
    // ptr1 should be NULL after replacement
    EXPECT_EQ(ptr1, nullptr);
    // Allocation count unchanged
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(ReallocTest, ReplacePointers_InvalidPtr_NoOp) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr));

    // Try to replace a fake pointer
    uint8_t* fake_ptr = heap_memory + 500;
    uint8_t* new_ptr = nullptr;

    // Should not crash
    replace_pointers(&heap, reinterpret_cast<void**>(&fake_ptr),
                     reinterpret_cast<void**>(&new_ptr));

    // Original allocation should be intact
    EXPECT_EQ(heap.alloc_info.allocations_num, 1u);
}

TEST_F(ReallocTest, Realloc_WithOtherAllocations_Works) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr3));

    // Write identifiable data
    ptr1[0] = 0x11;
    ptr2[0] = 0x22;
    ptr3[0] = 0x33;

    // Realloc middle allocation
    bool result = drealloc(&heap, 64, reinterpret_cast<void**>(&ptr2));

    EXPECT_TRUE(result);
    // All data should be preserved
    EXPECT_EQ(ptr1[0], 0x11);
    EXPECT_EQ(ptr2[0], 0x22);
    EXPECT_EQ(ptr3[0], 0x33);
}

// Test that drealloc gracefully fails when not enough space
TEST_F(ReallocTest, Realloc_Grow_NotEnoughSpace_Fails) {
    // Fill most of the heap
    uint8_t* ptr1 = nullptr;
    dalloc(&heap, TEST_HEAP_SIZE - 64, reinterpret_cast<void**>(&ptr1));
    ASSERT_NE(ptr1, nullptr);

    uint8_t* ptr2 = nullptr;
    dalloc(&heap, 32, reinterpret_cast<void**>(&ptr2));
    // ptr2 might be null if not enough space

    if (ptr2 != nullptr) {
        // Try to grow ptr2 beyond available space
        bool result = drealloc(&heap, 128, reinterpret_cast<void**>(&ptr2));
        EXPECT_FALSE(result);
    }
}

