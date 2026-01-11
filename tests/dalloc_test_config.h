#ifndef DALLOC_TEST_CONFIG_H
#define DALLOC_TEST_CONFIG_H

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

extern "C" {
#include "dalloc.h"
}

// Test heap configuration
static constexpr size_t TEST_HEAP_SIZE = 1024;
static constexpr size_t TEST_LARGE_HEAP_SIZE = 8192;

// Canary configuration for memory corruption detection
static constexpr size_t CANARY_SIZE = 64;
static constexpr uint8_t CANARY_PATTERN = 0xCD;  // "Clean memory" pattern
static constexpr uint8_t UNINIT_PATTERN = 0xAA;  // Uninitialized memory pattern

/**
 * @brief Test fixture with canary bytes for memory corruption detection
 *
 * Memory layout:
 *   [CANARY_BEFORE][HEAP_MEMORY][CANARY_AFTER]
 *
 * If dalloc writes outside heap bounds, canary bytes will be corrupted
 * and TearDown will detect it.
 */
class DallocTestFixture : public ::testing::Test {
protected:
    // Canary bytes BEFORE heap
    uint8_t canary_before[CANARY_SIZE];

    // Actual heap memory
    uint8_t heap_memory[TEST_HEAP_SIZE];

    // Canary bytes AFTER heap
    uint8_t canary_after[CANARY_SIZE];

    // Heap structure
    heap_t heap;

    void SetUp() override {
        // Fill canaries with known pattern
        std::memset(canary_before, CANARY_PATTERN, CANARY_SIZE);
        std::memset(canary_after, CANARY_PATTERN, CANARY_SIZE);

        // Fill heap with uninitialized pattern (to detect use of uninit memory)
        std::memset(heap_memory, UNINIT_PATTERN, TEST_HEAP_SIZE);

        // Initialize heap
        heap_init(&heap, heap_memory, TEST_HEAP_SIZE);
    }

    void TearDown() override {
        // Check canary bytes for corruption
        checkCanaryIntegrity();
    }

    // Verify canary bytes are intact (no buffer overflow/underflow)
    void checkCanaryIntegrity() {
        for (size_t i = 0; i < CANARY_SIZE; i++) {
            EXPECT_EQ(canary_before[i], CANARY_PATTERN)
                << "Buffer UNDERFLOW detected at canary_before[" << i << "]";
            EXPECT_EQ(canary_after[i], CANARY_PATTERN)
                << "Buffer OVERFLOW detected at canary_after[" << i << "]";
        }
    }

    // Explicitly check canaries mid-test (for stress tests)
    void assertCanaryIntegrity() {
        for (size_t i = 0; i < CANARY_SIZE; i++) {
            ASSERT_EQ(canary_before[i], CANARY_PATTERN)
                << "Buffer UNDERFLOW detected at canary_before[" << i << "]";
            ASSERT_EQ(canary_after[i], CANARY_PATTERN)
                << "Buffer OVERFLOW detected at canary_after[" << i << "]";
        }
    }
};

/**
 * @brief Large heap fixture for stress tests
 */
class DallocLargeHeapFixture : public ::testing::Test {
protected:
    uint8_t canary_before[CANARY_SIZE];
    uint8_t heap_memory[TEST_LARGE_HEAP_SIZE];
    uint8_t canary_after[CANARY_SIZE];
    heap_t heap;

    void SetUp() override {
        std::memset(canary_before, CANARY_PATTERN, CANARY_SIZE);
        std::memset(canary_after, CANARY_PATTERN, CANARY_SIZE);
        std::memset(heap_memory, UNINIT_PATTERN, TEST_LARGE_HEAP_SIZE);
        heap_init(&heap, heap_memory, TEST_LARGE_HEAP_SIZE);
    }

    void TearDown() override {
        for (size_t i = 0; i < CANARY_SIZE; i++) {
            EXPECT_EQ(canary_before[i], CANARY_PATTERN)
                << "Buffer UNDERFLOW detected";
            EXPECT_EQ(canary_after[i], CANARY_PATTERN)
                << "Buffer OVERFLOW detected";
        }
    }

    void assertCanaryIntegrity() {
        for (size_t i = 0; i < CANARY_SIZE; i++) {
            ASSERT_EQ(canary_before[i], CANARY_PATTERN)
                << "Buffer UNDERFLOW detected at canary_before[" << i << "]";
            ASSERT_EQ(canary_after[i], CANARY_PATTERN)
                << "Buffer OVERFLOW detected at canary_after[" << i << "]";
        }
    }
};

#endif // DALLOC_TEST_CONFIG_H
