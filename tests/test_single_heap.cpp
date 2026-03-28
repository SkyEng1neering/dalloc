/**
 * @file test_single_heap.cpp
 * @brief Tests for single heap API (USE_SINGLE_HEAP_MEMORY)
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "dalloc.h"
}

#ifdef USE_SINGLE_HEAP_MEMORY

class SingleHeapTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 1024;
    uint8_t buffer[BUFFER_SIZE];

    void SetUp() override {
        // Ensure heap is unregistered before each test
        dalloc_unregister_heap(true);
        std::memset(buffer, 0xAA, BUFFER_SIZE);
    }

    void TearDown() override {
        // Cleanup after each test
        dalloc_unregister_heap(true);
    }
};

// ==================== Registration Tests ====================

TEST_F(SingleHeapTest, Register_Success) {
    EXPECT_FALSE(dalloc_is_initialized());

    bool result = dalloc_register_heap(buffer, BUFFER_SIZE);

    EXPECT_TRUE(result);
    EXPECT_TRUE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, Register_NullBuffer_Fails) {
    bool result = dalloc_register_heap(nullptr, BUFFER_SIZE);

    EXPECT_FALSE(result);
    EXPECT_FALSE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, Register_ZeroSize_Fails) {
    bool result = dalloc_register_heap(buffer, 0);

    EXPECT_FALSE(result);
    EXPECT_FALSE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, Register_AlreadyRegistered_Fails) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t another_buffer[512];
    bool result = dalloc_register_heap(another_buffer, sizeof(another_buffer));

    EXPECT_FALSE(result);
    // Original registration should still be valid
    EXPECT_TRUE(dalloc_is_initialized());
}

// ==================== Unregister Tests ====================

TEST_F(SingleHeapTest, Unregister_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);
    EXPECT_TRUE(dalloc_is_initialized());

    bool result = dalloc_unregister_heap(false);

    EXPECT_TRUE(result);
    EXPECT_FALSE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, Unregister_NotRegistered_Fails) {
    EXPECT_FALSE(dalloc_is_initialized());

    bool result = dalloc_unregister_heap(false);

    EXPECT_FALSE(result);
}

TEST_F(SingleHeapTest, Unregister_WithAllocations_NoForce_Fails) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    bool result = dalloc_unregister_heap(false);

    EXPECT_FALSE(result);
    EXPECT_TRUE(dalloc_is_initialized());

    // Cleanup
    def_dfree(reinterpret_cast<void**>(&ptr));
}

TEST_F(SingleHeapTest, Unregister_WithAllocations_Force_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    bool result = dalloc_unregister_heap(true);

    EXPECT_TRUE(result);
    EXPECT_FALSE(dalloc_is_initialized());
    // Note: ptr is now a dangling pointer
}

TEST_F(SingleHeapTest, Unregister_ThenRegisterNew_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);
    dalloc_unregister_heap(false);

    uint8_t new_buffer[512];
    bool result = dalloc_register_heap(new_buffer, sizeof(new_buffer));

    EXPECT_TRUE(result);
    EXPECT_TRUE(dalloc_is_initialized());
}

// ==================== Reset Tests ====================

TEST_F(SingleHeapTest, Reset_ClearsAllocations) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr1 = nullptr;
    uint8_t* ptr2 = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr1));
    def_dalloc(64, reinterpret_cast<void**>(&ptr2));

    heap_t* heap = dalloc_get_default_heap();
    ASSERT_NE(heap, nullptr);
    EXPECT_EQ(heap->alloc_info.allocations_num, 2u);

    dalloc_reset_heap();

    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap->offset, 0u);
    EXPECT_TRUE(dalloc_is_initialized());  // Still registered
}

TEST_F(SingleHeapTest, Reset_NotRegistered_NoOp) {
    EXPECT_FALSE(dalloc_is_initialized());

    // Should not crash
    dalloc_reset_heap();

    EXPECT_FALSE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, Reset_ThenAllocate_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr1 = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr1));

    dalloc_reset_heap();

    // Should be able to allocate again
    uint8_t* ptr2 = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr2));

    EXPECT_NE(ptr2, nullptr);

    heap_t* heap = dalloc_get_default_heap();
    EXPECT_EQ(heap->alloc_info.allocations_num, 1u);
}

// ==================== Get Default Heap Tests ====================

TEST_F(SingleHeapTest, GetDefaultHeap_Registered_ReturnsPointer) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    heap_t* heap = dalloc_get_default_heap();

    EXPECT_NE(heap, nullptr);
    EXPECT_EQ(heap->total_size, BUFFER_SIZE);
}

TEST_F(SingleHeapTest, GetDefaultHeap_NotRegistered_ReturnsNull) {
    EXPECT_FALSE(dalloc_is_initialized());

    heap_t* heap = dalloc_get_default_heap();

    EXPECT_EQ(heap, nullptr);
}

// ==================== def_* Functions Without Registration ====================

TEST_F(SingleHeapTest, DefDalloc_NotRegistered_ReturnsNull) {
    EXPECT_FALSE(dalloc_is_initialized());

    uint8_t* ptr = reinterpret_cast<uint8_t*>(0xDEADBEEF);
    def_dalloc(64, reinterpret_cast<void**>(&ptr));

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(SingleHeapTest, DefDfree_NotRegistered_NoOp) {
    EXPECT_FALSE(dalloc_is_initialized());

    uint8_t* ptr = reinterpret_cast<uint8_t*>(0xDEADBEEF);

    // Should not crash
    def_dfree(reinterpret_cast<void**>(&ptr));
}

TEST_F(SingleHeapTest, DefDrealloc_NotRegistered_ReturnsFalse) {
    EXPECT_FALSE(dalloc_is_initialized());

    uint8_t* ptr = nullptr;
    bool result = def_drealloc(64, reinterpret_cast<void**>(&ptr));

    EXPECT_FALSE(result);
}

TEST_F(SingleHeapTest, DefReplacePointers_NotRegistered_NoOp) {
    EXPECT_FALSE(dalloc_is_initialized());

    uint8_t* ptr1 = reinterpret_cast<uint8_t*>(0xDEADBEEF);
    uint8_t* ptr2 = nullptr;

    // Should not crash
    def_replace_pointers(reinterpret_cast<void**>(&ptr1),
                         reinterpret_cast<void**>(&ptr2));
}

// ==================== def_* Functions With Registration ====================

TEST_F(SingleHeapTest, DefDalloc_Registered_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));

    EXPECT_NE(ptr, nullptr);

    // Verify we can write to allocated memory
    std::memset(ptr, 0x55, 64);
    EXPECT_EQ(ptr[0], 0x55);
    EXPECT_EQ(ptr[63], 0x55);
}

TEST_F(SingleHeapTest, DefDfree_Registered_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    def_dfree(reinterpret_cast<void**>(&ptr));

    heap_t* heap = dalloc_get_default_heap();
    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
}

TEST_F(SingleHeapTest, DefDrealloc_Registered_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(32, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (int i = 0; i < 32; i++) {
        ptr[i] = static_cast<uint8_t>(i);
    }

    // Grow
    bool result = def_drealloc(64, reinterpret_cast<void**>(&ptr));

    EXPECT_TRUE(result);
    EXPECT_NE(ptr, nullptr);

    // Verify data preserved
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(ptr[i], static_cast<uint8_t>(i));
    }
}

TEST_F(SingleHeapTest, DefReplacePointers_Registered_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr1 = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr1));
    ASSERT_NE(ptr1, nullptr);

    uint8_t* original_value = ptr1;
    uint8_t* ptr2 = nullptr;

    def_replace_pointers(reinterpret_cast<void**>(&ptr1),
                         reinterpret_cast<void**>(&ptr2));

    EXPECT_EQ(ptr1, nullptr);
    EXPECT_EQ(ptr2, original_value);
}

// ==================== Heap Info String Tests ====================

TEST_F(SingleHeapTest, DefHeapInfoStr_Success) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));
    ASSERT_NE(ptr, nullptr);

    char buf[128];
    int n = def_dalloc_heap_info_str(buf, sizeof(buf));

    EXPECT_GT(n, 0);
    EXPECT_NE(std::strstr(buf, "allocs=1"), nullptr);
    // Check total size is present
    char expected[32];
    std::snprintf(expected, sizeof(expected), "/%lu", (unsigned long)BUFFER_SIZE);
    EXPECT_NE(std::strstr(buf, expected), nullptr);

    def_dfree(reinterpret_cast<void**>(&ptr));
}

TEST_F(SingleHeapTest, DefHeapInfoStr_NotRegistered_ReturnsError) {
    EXPECT_FALSE(dalloc_is_initialized());

    char buf[128] = "garbage";
    int n = def_dalloc_heap_info_str(buf, sizeof(buf));

    EXPECT_EQ(n, -1);
    EXPECT_EQ(buf[0], '\0');
}

TEST_F(SingleHeapTest, DefHeapInfoStr_NullBuf_ReturnsError) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    int n = def_dalloc_heap_info_str(nullptr, 128);
    EXPECT_EQ(n, -1);
}

TEST_F(SingleHeapTest, DefHeapInfoStr_ZeroBufSize_ReturnsError) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    char buf[1];
    int n = def_dalloc_heap_info_str(buf, 0);
    EXPECT_EQ(n, -1);
}

// ==================== Full Workflow Tests ====================

TEST_F(SingleHeapTest, FullWorkflow_RegisterAllocFreeUnregister) {
    // Register
    EXPECT_TRUE(dalloc_register_heap(buffer, BUFFER_SIZE));

    // Allocate
    uint8_t* ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&ptr));
    EXPECT_NE(ptr, nullptr);

    // Use
    ptr[0] = 0x12;
    ptr[63] = 0x34;

    // Free
    def_dfree(reinterpret_cast<void**>(&ptr));
    EXPECT_EQ(ptr, nullptr);

    // Unregister (should succeed - no allocations)
    EXPECT_TRUE(dalloc_unregister_heap(false));
    EXPECT_FALSE(dalloc_is_initialized());
}

TEST_F(SingleHeapTest, FullWorkflow_MultipleAllocationsAndReset) {
    dalloc_register_heap(buffer, BUFFER_SIZE);

    // Multiple allocations
    uint8_t* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = nullptr;
        def_dalloc(32, reinterpret_cast<void**>(&ptrs[i]));
        ASSERT_NE(ptrs[i], nullptr);
        ptrs[i][0] = static_cast<uint8_t>(i);
    }

    heap_t* heap = dalloc_get_default_heap();
    EXPECT_EQ(heap->alloc_info.allocations_num, 10u);

    // Reset instead of freeing individually
    dalloc_reset_heap();

    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap->offset, 0u);

    // Can allocate again
    uint8_t* new_ptr = nullptr;
    def_dalloc(64, reinterpret_cast<void**>(&new_ptr));
    EXPECT_NE(new_ptr, nullptr);
}

// ==================== Stress Tests for Single Heap ====================

class SingleHeapStressTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];

    void SetUp() override {
        dalloc_unregister_heap(true);
        std::memset(buffer, 0xAA, BUFFER_SIZE);
        dalloc_register_heap(buffer, BUFFER_SIZE);
    }

    void TearDown() override {
        dalloc_unregister_heap(true);
    }
};

TEST_F(SingleHeapStressTest, Stress_RapidAllocFree) {
    for (int i = 0; i < 100; i++) {
        uint8_t* ptr = nullptr;
        def_dalloc(32, reinterpret_cast<void**>(&ptr));
        ASSERT_NE(ptr, nullptr) << "Allocation failed at iteration " << i;

        ptr[0] = static_cast<uint8_t>(i);
        ptr[31] = static_cast<uint8_t>(i);

        def_dfree(reinterpret_cast<void**>(&ptr));
        EXPECT_EQ(ptr, nullptr);
    }

    heap_t* heap = dalloc_get_default_heap();
    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
    EXPECT_EQ(heap->offset, 0u);
}

TEST_F(SingleHeapStressTest, Stress_MultiplePointersDefrag) {
    for (int cycle = 0; cycle < 50; cycle++) {
        uint8_t* ptr1 = nullptr;
        uint8_t* ptr2 = nullptr;
        uint8_t* ptr3 = nullptr;

        def_dalloc(64, reinterpret_cast<void**>(&ptr1));
        def_dalloc(64, reinterpret_cast<void**>(&ptr2));
        def_dalloc(64, reinterpret_cast<void**>(&ptr3));

        ASSERT_NE(ptr1, nullptr);
        ASSERT_NE(ptr2, nullptr);
        ASSERT_NE(ptr3, nullptr);

        ptr1[0] = 0x11;
        ptr2[0] = 0x22;
        ptr3[0] = 0x33;

        // Free middle - triggers defrag
        def_dfree(reinterpret_cast<void**>(&ptr1));

        // Verify data preserved after defrag
        EXPECT_EQ(ptr2[0], 0x22) << "Data corruption at cycle " << cycle;
        EXPECT_EQ(ptr3[0], 0x33) << "Data corruption at cycle " << cycle;

        def_dfree(reinterpret_cast<void**>(&ptr2));
        def_dfree(reinterpret_cast<void**>(&ptr3));
    }
}

TEST_F(SingleHeapStressTest, Stress_AlternatingPattern) {
    uint8_t* ptrs[20] = {nullptr};

    for (int round = 0; round < 20; round++) {
        // Allocate all
        for (int i = 0; i < 20; i++) {
            def_dalloc(32, reinterpret_cast<void**>(&ptrs[i]));
            if (ptrs[i] != nullptr) {
                ptrs[i][0] = static_cast<uint8_t>(i);
            }
        }

        // Free alternating
        for (int i = 0; i < 20; i += 2) {
            if (ptrs[i] != nullptr) {
                def_dfree(reinterpret_cast<void**>(&ptrs[i]));
            }
        }

        // Verify remaining
        for (int i = 1; i < 20; i += 2) {
            if (ptrs[i] != nullptr) {
                EXPECT_EQ(ptrs[i][0], static_cast<uint8_t>(i))
                    << "Corruption at round " << round << " ptr " << i;
            }
        }

        // Free rest
        for (int i = 1; i < 20; i += 2) {
            if (ptrs[i] != nullptr) {
                def_dfree(reinterpret_cast<void**>(&ptrs[i]));
            }
        }

        // Reset array
        std::memset(ptrs, 0, sizeof(ptrs));
    }
}

TEST_F(SingleHeapStressTest, Stress_ReregisterHeap) {
    // This tests the scenario from usmart_ptr tests:
    // unregister and reregister heap between operations

    for (int cycle = 0; cycle < 20; cycle++) {
        uint8_t* ptr = nullptr;
        def_dalloc(64, reinterpret_cast<void**>(&ptr));
        ASSERT_NE(ptr, nullptr);

        ptr[0] = static_cast<uint8_t>(cycle);
        def_dfree(reinterpret_cast<void**>(&ptr));

        // Unregister and reregister (simulates test fixture SetUp/TearDown)
        dalloc_unregister_heap(true);
        std::memset(buffer, 0xBB, BUFFER_SIZE);
        dalloc_register_heap(buffer, BUFFER_SIZE);
    }

    heap_t* heap = dalloc_get_default_heap();
    EXPECT_NE(heap, nullptr);
    EXPECT_EQ(heap->alloc_info.allocations_num, 0u);
}

#endif // USE_SINGLE_HEAP_MEMORY
