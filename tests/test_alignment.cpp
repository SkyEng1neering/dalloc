/**
 * @file test_alignment.cpp
 * @brief Alignment tests for dalloc
 */

#include "dalloc_test_config.h"

class AlignmentTest : public DallocTestFixture {};

TEST_F(AlignmentTest, Alignment_4Bytes_Correct) {
    uint8_t* ptr = nullptr;
    dalloc(&heap, 16, reinterpret_cast<void**>(&ptr));

    ASSERT_NE(ptr, nullptr);

    // Pointer should be 4-byte aligned
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % 4, 0u) << "Pointer not 4-byte aligned: " << std::hex << addr;
}

TEST_F(AlignmentTest, Alignment_OddSizes_Padded) {
    // Allocate odd sizes and verify all pointers are aligned
    size_t odd_sizes[] = {1, 3, 5, 7, 9, 11, 13, 17, 23, 31};
    uint8_t* ptrs[10] = {nullptr};

    for (size_t i = 0; i < 10; i++) {
        dalloc(&heap, odd_sizes[i], reinterpret_cast<void**>(&ptrs[i]));
        ASSERT_NE(ptrs[i], nullptr) << "Failed to allocate size " << odd_sizes[i];

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
        EXPECT_EQ(addr % 4, 0u)
            << "Size " << odd_sizes[i] << " not aligned: " << std::hex << addr;
    }

    // Verify sequential ordering
    for (size_t i = 1; i < 10; i++) {
        if (ptrs[i] != nullptr && ptrs[i-1] != nullptr) {
            EXPECT_GT(ptrs[i], ptrs[i-1])
                << "Non-sequential allocation at index " << i;
        }
    }
}

TEST_F(AlignmentTest, Alignment_AfterDefrag_Maintained) {
    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    uint8_t* ptr3 = nullptr;

    // Allocate with odd sizes
    dalloc(&heap, 13, reinterpret_cast<void**>(&ptr1));
    dalloc(&heap, 17, reinterpret_cast<void**>(&ptr2));
    dalloc(&heap, 23, reinterpret_cast<void**>(&ptr3));

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Free first block, triggering defragmentation
    dfree(&heap, reinterpret_cast<void**>(&ptr1), USING_PTR_ADDRESS);

    // After defrag, remaining pointers should still be aligned
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(ptr2);
    uintptr_t addr3 = reinterpret_cast<uintptr_t>(ptr3);

    EXPECT_EQ(addr2 % 4, 0u) << "ptr2 not aligned after defrag: " << std::hex << addr2;
    EXPECT_EQ(addr3 % 4, 0u) << "ptr3 not aligned after defrag: " << std::hex << addr3;
}

TEST_F(AlignmentTest, Alignment_ConsecutiveAllocs_AllAligned) {
    const int NUM_ALLOCS = 20;
    uint8_t* ptrs[NUM_ALLOCS] = {nullptr};

    // Various sizes including odd numbers
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = (i * 7 + 1) % 64 + 1;  // Sizes from 1 to 64
        dalloc(&heap, size, reinterpret_cast<void**>(&ptrs[i]));

        if (ptrs[i] != nullptr) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            EXPECT_EQ(addr % 4, 0u)
                << "Allocation " << i << " (size " << size
                << ") not aligned: " << std::hex << addr;
        }
    }
}

TEST_F(AlignmentTest, Alignment_OffsetIsAligned) {
    // Verify that heap offset stays aligned after allocations
    uint8_t* ptr = nullptr;

    // Allocate odd sizes
    dalloc(&heap, 5, reinterpret_cast<void**>(&ptr));
    EXPECT_EQ(heap.offset % 4, 0u) << "Offset not aligned after 5-byte alloc";

    dalloc(&heap, 11, reinterpret_cast<void**>(&ptr));
    EXPECT_EQ(heap.offset % 4, 0u) << "Offset not aligned after 11-byte alloc";

    dalloc(&heap, 1, reinterpret_cast<void**>(&ptr));
    EXPECT_EQ(heap.offset % 4, 0u) << "Offset not aligned after 1-byte alloc";

    dalloc(&heap, 27, reinterpret_cast<void**>(&ptr));
    EXPECT_EQ(heap.offset % 4, 0u) << "Offset not aligned after 27-byte alloc";
}

TEST_F(AlignmentTest, Alignment_MultipleDefrag_StaysAligned) {
    uint8_t* ptrs[8] = {nullptr};

    // Allocate 8 blocks with varying sizes
    for (int i = 0; i < 8; i++) {
        size_t size = (i * 3) + 5;  // 5, 8, 11, 14, 17, 20, 23, 26
        dalloc(&heap, size, reinterpret_cast<void**>(&ptrs[i]));
    }

    // Free every other block
    for (int i = 0; i < 8; i += 2) {
        if (ptrs[i] != nullptr) {
            dfree(&heap, reinterpret_cast<void**>(&ptrs[i]), USING_PTR_ADDRESS);
        }
    }

    // Check alignment of remaining pointers
    for (int i = 1; i < 8; i += 2) {
        if (ptrs[i] != nullptr) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            EXPECT_EQ(addr % 4, 0u)
                << "Block " << i << " not aligned after defrag: " << std::hex << addr;
        }
    }

    // Now free more and check again
    if (ptrs[1] != nullptr) {
        dfree(&heap, reinterpret_cast<void**>(&ptrs[1]), USING_PTR_ADDRESS);
    }

    for (int i = 3; i < 8; i += 2) {
        if (ptrs[i] != nullptr) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            EXPECT_EQ(addr % 4, 0u)
                << "Block " << i << " not aligned after second defrag";
        }
    }
}

