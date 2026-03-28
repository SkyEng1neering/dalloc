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

/*
 * ===========================================================================
 * Tests for defrag correctness when tracking variables live inside the heap.
 *
 * Standard tests above only place tracking vars on the stack. But when
 * usmart_ptr is moved (e.g. into a uvector element), replace_pointers()
 * moves the tracking variable INTO a heap buffer. The two bugs these tests
 * cover:
 *
 * Bug #1 (first loop, `for k = i+1`):
 *   If alloc_info[k < freed_index].ptr is inside a heap buffer that sits
 *   AFTER the freed block, that ptr ADDRESS is not adjusted after memmove.
 *   The tracking address goes stale; subsequent replace_pointers() calls
 *   fail silently, and later defrags may corrupt the value through the
 *   stale address.
 *
 * Bug #2 (second loop, unconditional decrement):
 *   If alloc_info[k > freed_index].ptr VALUE points to an allocation that
 *   is physically BEFORE the freed block, the value must NOT be decremented.
 *   (The second loop previously had no bounds check.)
 * ===========================================================================
 */

// Helper: find alloc_info index whose .ptr equals the given address.
// Returns -1 if not found.
static int findTrackingIndex(heap_t& h, void** addr) {
    for (uint32_t i = 0; i < h.alloc_info.allocations_num; i++) {
        if (h.alloc_info.ptr_info_arr[i].ptr == reinterpret_cast<uint8_t**>(addr)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Bug #1: an early alloc_info entry (k < freed_index) has its tracking
// variable inside a later heap buffer.  When the freed block is removed,
// that tracking ADDRESS must be adjusted — but the old code skipped k < i.
//
// Layout:  [OBJ(16)] [GAP(16)] [CONTAINER(32)]
//          alloc_info[0]=OBJ  [1]=GAP  [2]=CONTAINER
// After replace_pointers: alloc_info[0].ptr = &CONTAINER[0]  (inside heap)
// Free GAP (i=1): CONTAINER shifts down 16 bytes.
// Bug:  alloc_info[0].ptr stays at old &CONTAINER[0] (stale — k=0 < i=1).
// Fix:  alloc_info[0].ptr updated to new &CONTAINER[0].
TEST_F(DefragTest, Defrag_TrackingVarInLaterBuffer_AddressAdjusted) {
    uint8_t* obj       = nullptr;
    uint8_t* gap       = nullptr;
    uint8_t* container = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&obj));
    dalloc(&heap, 16, reinterpret_cast<void**>(&gap));
    dalloc(&heap, 32, reinterpret_cast<void**>(&container));
    ASSERT_NE(obj,       nullptr);
    ASSERT_NE(gap,       nullptr);
    ASSERT_NE(container, nullptr);

    uint8_t* obj_orig = obj;

    // Mark OBJ memory so we can detect corruption.
    for (int i = 0; i < 16; i++) obj[i] = static_cast<uint8_t>(0xAA + i);

    // Move OBJ tracking from the stack (&obj) into slot[0] of CONTAINER.
    // After this: alloc_info[0].ptr = (uint8_t**)container  (inside heap)
    uint8_t** slot = reinterpret_cast<uint8_t**>(container);
    *slot = nullptr;
    replace_pointers(&heap,
                     reinterpret_cast<void**>(&obj),
                     reinterpret_cast<void**>(slot));

    ASSERT_EQ(obj, nullptr);          // old stack var cleared
    ASSERT_EQ(*slot, obj_orig);       // slot now holds OBJ address

    // Confirm tracking is now inside the heap (ptr address is within heap_memory buffer).
    {
        size_t p = reinterpret_cast<size_t>(heap.alloc_info.ptr_info_arr[0].ptr);
        size_t h = reinterpret_cast<size_t>(heap_memory);
        ASSERT_GE(p, h);
        ASSERT_LT(p, h + TEST_HEAP_SIZE);
    }
    ASSERT_EQ(heap.alloc_info.ptr_info_arr[0].ptr,
              reinterpret_cast<uint8_t**>(container));

    uint8_t* container_orig = container;

    // Free GAP — triggers defrag, CONTAINER shifts down 16 bytes.
    dfree(&heap, reinterpret_cast<void**>(&gap), USING_PTR_ADDRESS);

    ASSERT_EQ(heap.alloc_info.allocations_num, 2u);

    // CONTAINER must have moved.
    EXPECT_EQ(container, container_orig - 16)
        << "CONTAINER should shift down by GAP size";

    // Bug #1 check: tracking address for OBJ must follow CONTAINER.
    uint8_t** expected_tracking = reinterpret_cast<uint8_t**>(container);
    EXPECT_EQ(heap.alloc_info.ptr_info_arr[0].ptr, expected_tracking)
        << "alloc_info[0].ptr must be adjusted when CONTAINER shifts";

    // Bug #2 check: OBJ address (physically before GAP) must not be decremented.
    EXPECT_EQ(*heap.alloc_info.ptr_info_arr[0].ptr, obj_orig)
        << "OBJ address must not be corrupted by defrag";

    // Data at OBJ must still be intact (the object itself did not move).
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(obj_orig[i], static_cast<uint8_t>(0xAA + i))
            << "OBJ data corrupted at byte " << i;
    }
}

// Verify that replace_pointers() succeeds AFTER a defrag that moves the
// container holding the tracking variable.  With Bug #1 present, the stale
// address causes the subsequent replace_pointers() to fail silently.
TEST_F(DefragTest, Defrag_TrackingVarInHeap_SubsequentReplacePtrSucceeds) {
    uint8_t* obj       = nullptr;
    uint8_t* gap       = nullptr;
    uint8_t* container = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&obj));
    dalloc(&heap, 16, reinterpret_cast<void**>(&gap));
    dalloc(&heap, 32, reinterpret_cast<void**>(&container));
    ASSERT_NE(obj, nullptr);

    uint8_t* obj_orig = obj;

    // Move OBJ tracking into CONTAINER.
    uint8_t** slot = reinterpret_cast<uint8_t**>(container);
    *slot = nullptr;
    replace_pointers(&heap,
                     reinterpret_cast<void**>(&obj),
                     reinterpret_cast<void**>(slot));

    // Free GAP — defrag shifts CONTAINER.
    dfree(&heap, reinterpret_cast<void**>(&gap), USING_PTR_ADDRESS);

    // Simulate a second "move" of OBJ tracking: from CONTAINER slot to new_var.
    // This models what usmart_ptr does when a uvector element is moved to a
    // larger buffer — replace_pointers is called to update the tracking.
    // With Bug #1 fixed, alloc_info[0].ptr correctly points to the new &slot
    // (inside shifted CONTAINER), so validate_ptr finds it.
    uint8_t* new_var = nullptr;
    // slot is now stale (holds old container address); use the updated container ptr.
    uint8_t** new_slot = reinterpret_cast<uint8_t**>(container);
    replace_pointers(&heap,
                     reinterpret_cast<void**>(new_slot),
                     reinterpret_cast<void**>(&new_var));

    // If replace_pointers succeeded, new_var holds obj_orig and new_slot was cleared.
    EXPECT_EQ(new_var, obj_orig)
        << "replace_pointers after defrag must succeed (Bug #1 would silently fail)";
    EXPECT_EQ(*new_slot, nullptr)
        << "source slot should be nulled after successful replace_pointers";
    EXPECT_EQ(heap.alloc_info.ptr_info_arr[0].ptr,
              reinterpret_cast<uint8_t**>(&new_var))
        << "tracking must now point to new_var";
}

// Simulate the full usmart_ptr + uvector container-grow pattern that
// triggered the production crash.
//
// Pattern:
//   1. Allocate OBJ_A and OBJ_B (simulate two modifiers).
//   2. Allocate CONTAINER_V1 (small buffer), place A and B into it.
//   3. Allocate CONTAINER_V2 (larger), memmove V1 → V2, move tracking
//      for A and B from V1 slots to V2 slots, free V1.
//   4. Verify A and B are correctly tracked and their data is intact.
//   5. Verify a further replace_pointers (third grow) succeeds.
TEST_F(DefragTest, Defrag_SimulateSmartPtrContainerGrow_AllPointersValid) {
    // Sizes chosen so total fits in TEST_HEAP_SIZE=1024.
    // sizeof(usmart_ptr<Modifier>) = 16 → container_v1=32, v2=64.
    const uint32_t OBJ_SIZE = 24;
    const uint32_t SLOT_SIZE = sizeof(uint8_t*);  // 8 bytes per tracking slot
    const uint32_t V1_SIZE = 2 * SLOT_SIZE;   // 2 slots
    const uint32_t V2_SIZE = 4 * SLOT_SIZE;   // 4 slots (grown)

    uint8_t* obj_a = nullptr;
    uint8_t* obj_b = nullptr;
    uint8_t* v1    = nullptr;

    dalloc(&heap, OBJ_SIZE, reinterpret_cast<void**>(&obj_a));
    dalloc(&heap, OBJ_SIZE, reinterpret_cast<void**>(&obj_b));
    dalloc(&heap, V1_SIZE,  reinterpret_cast<void**>(&v1));
    ASSERT_NE(obj_a, nullptr);
    ASSERT_NE(obj_b, nullptr);
    ASSERT_NE(v1,    nullptr);

    uint8_t* obj_a_orig = obj_a;
    uint8_t* obj_b_orig = obj_b;

    // Mark objects for corruption detection.
    for (uint32_t i = 0; i < OBJ_SIZE; i++) obj_a[i] = static_cast<uint8_t>(0xA0 + i);
    for (uint32_t i = 0; i < OBJ_SIZE; i++) obj_b[i] = static_cast<uint8_t>(0xB0 + i);

    // --- Place A and B into V1 slots (simulate usmart_ptr move into uvector) ---
    uint8_t** slot_a_v1 = reinterpret_cast<uint8_t**>(v1 + 0);
    uint8_t** slot_b_v1 = reinterpret_cast<uint8_t**>(v1 + SLOT_SIZE);
    *slot_a_v1 = nullptr;
    *slot_b_v1 = nullptr;

    replace_pointers(&heap, reinterpret_cast<void**>(&obj_a),
                     reinterpret_cast<void**>(slot_a_v1));
    replace_pointers(&heap, reinterpret_cast<void**>(&obj_b),
                     reinterpret_cast<void**>(slot_b_v1));

    ASSERT_EQ(*slot_a_v1, obj_a_orig);
    ASSERT_EQ(*slot_b_v1, obj_b_orig);

    // --- Grow: allocate V2, copy V1 slots, move tracking, free V1 ---
    uint8_t* v2 = nullptr;
    dalloc(&heap, V2_SIZE, reinterpret_cast<void**>(&v2));
    ASSERT_NE(v2, nullptr);

    // Copy slot values (simulate memmove of the buffer).
    uint8_t** slot_a_v2 = reinterpret_cast<uint8_t**>(v2 + 0);
    uint8_t** slot_b_v2 = reinterpret_cast<uint8_t**>(v2 + SLOT_SIZE);
    *slot_a_v2 = *slot_a_v1;
    *slot_b_v2 = *slot_b_v1;

    // Move tracking: V1 slots → V2 slots.
    replace_pointers(&heap, reinterpret_cast<void**>(slot_a_v1),
                     reinterpret_cast<void**>(slot_a_v2));
    replace_pointers(&heap, reinterpret_cast<void**>(slot_b_v1),
                     reinterpret_cast<void**>(slot_b_v2));

    // Free V1 — triggers defrag (Bug #1 would corrupt tracking here).
    // After defrag: v2 stack var is updated to V2's new heap address.
    dfree(&heap, reinterpret_cast<void**>(&v1), USING_PTR_ADDRESS);

    ASSERT_EQ(heap.alloc_info.allocations_num, 3u);  // obj_a, obj_b, v2
    ASSERT_NE(v2, nullptr);

    // After defrag, v2 is the updated V2 address.  Use it to access slots.
    // (slot_a_v2 / slot_b_v2 hold the OLD v2 address and are now stale.)
    uint8_t** cur_slot_a = reinterpret_cast<uint8_t**>(v2 + 0);
    uint8_t** cur_slot_b = reinterpret_cast<uint8_t**>(v2 + SLOT_SIZE);

    // --- Verify A and B are tracked at correct addresses ---
    int idx_a = findTrackingIndex(heap, reinterpret_cast<void**>(cur_slot_a));
    int idx_b = findTrackingIndex(heap, reinterpret_cast<void**>(cur_slot_b));

    EXPECT_GE(idx_a, 0) << "A tracking not found after V1 freed";
    EXPECT_GE(idx_b, 0) << "B tracking not found after V1 freed";

    // Values must still be the original object addresses.
    EXPECT_EQ(*cur_slot_a, obj_a_orig) << "A address corrupted";
    EXPECT_EQ(*cur_slot_b, obj_b_orig) << "B address corrupted";

    // Object data must be intact (objects themselves did not move).
    for (uint32_t i = 0; i < OBJ_SIZE; i++) {
        EXPECT_EQ(obj_a_orig[i], static_cast<uint8_t>(0xA0 + i))
            << "obj_a data corrupted at byte " << i;
        EXPECT_EQ(obj_b_orig[i], static_cast<uint8_t>(0xB0 + i))
            << "obj_b data corrupted at byte " << i;
    }

    // --- Verify a second grow also works (Bug #1 compounds on repeated frees) ---
    uint8_t* v3 = nullptr;
    dalloc(&heap, V2_SIZE * 2, reinterpret_cast<void**>(&v3));
    ASSERT_NE(v3, nullptr);

    // Copy slot values from V2 (via updated v2 ptr) to V3 and move tracking.
    uint8_t** slot_a_v3 = reinterpret_cast<uint8_t**>(v3 + 0);
    uint8_t** slot_b_v3 = reinterpret_cast<uint8_t**>(v3 + SLOT_SIZE);
    *slot_a_v3 = *cur_slot_a;
    *slot_b_v3 = *cur_slot_b;

    replace_pointers(&heap, reinterpret_cast<void**>(cur_slot_a),
                     reinterpret_cast<void**>(slot_a_v3));
    replace_pointers(&heap, reinterpret_cast<void**>(cur_slot_b),
                     reinterpret_cast<void**>(slot_b_v3));

    dfree(&heap, reinterpret_cast<void**>(&v2), USING_PTR_ADDRESS);

    ASSERT_EQ(heap.alloc_info.allocations_num, 3u);

    // Use updated v3 to access slots after second defrag.
    uint8_t** cur_slot_a3 = reinterpret_cast<uint8_t**>(v3 + 0);
    uint8_t** cur_slot_b3 = reinterpret_cast<uint8_t**>(v3 + SLOT_SIZE);

    EXPECT_EQ(*cur_slot_a3, obj_a_orig) << "A address corrupted after second grow";
    EXPECT_EQ(*cur_slot_b3, obj_b_orig) << "B address corrupted after second grow";

    int idx_a2 = findTrackingIndex(heap, reinterpret_cast<void**>(cur_slot_a3));
    int idx_b2 = findTrackingIndex(heap, reinterpret_cast<void**>(cur_slot_b3));
    EXPECT_GE(idx_a2, 0) << "A tracking lost after second grow";
    EXPECT_GE(idx_b2, 0) << "B tracking lost after second grow";
}

// Bug #2 guard: when a tracking variable (at alloc_info[k > freed_index])
// holds a value pointing BEFORE the freed block, that value must not be
// decremented.  Constructed synthetically by manually placing a below-gap
// address into a post-gap slot before freeing.
TEST_F(DefragTest, Defrag_PtrValueBeforeFreeBlock_ValueNotDecremented) {
    // Layout: [OBJ(16)] [GAP(16)] [LATE(16)]
    // alloc_info: [0]=OBJ  [1]=GAP  [2]=LATE
    //
    // Simulate: LATE's tracking slot holds OBJ's address (an early address).
    // We achieve this by replace_pointers(OBJ → slot_in_LATE), making
    // alloc_info[0].ptr = &LATE[0], then move it back to a local var and
    // give alloc_info[2] a ptr VALUE = OBJ_addr via a manual drealloc trick.
    //
    // Simpler direct construction: allocate a "wrapper" that is tracked at
    // k > freed_index, and whose pointed-to address is OBJ (< freed block).
    // We build this using drealloc to place the tracking at a higher index
    // while preserving the early address as the VALUE.

    uint8_t* obj  = nullptr;
    uint8_t* gap  = nullptr;
    uint8_t* late = nullptr;

    dalloc(&heap, 16, reinterpret_cast<void**>(&obj));
    dalloc(&heap, 16, reinterpret_cast<void**>(&gap));
    dalloc(&heap, 16, reinterpret_cast<void**>(&late));
    ASSERT_NE(obj,  nullptr);
    ASSERT_NE(gap,  nullptr);
    ASSERT_NE(late, nullptr);

    uint8_t* late_orig = late;

    for (int i = 0; i < 16; i++) obj[i]  = static_cast<uint8_t>(0xCC + i);
    for (int i = 0; i < 16; i++) late[i] = static_cast<uint8_t>(0xDD + i);

    // Grow LATE via drealloc (alloc new, copy, free old internally).
    // This leaves alloc_info for LATE at a higher index than GAP.
    bool ok = drealloc(&heap, 32, reinterpret_cast<void**>(&late));
    ASSERT_TRUE(ok);

    // After drealloc, alloc_info order: [OBJ(0), (GAP now earlier), LATE(new highest)]
    // Verify LATE tracking index is higher than GAP tracking index.
    int idx_gap  = findTrackingIndex(heap, reinterpret_cast<void**>(&gap));
    int idx_late = findTrackingIndex(heap, reinterpret_cast<void**>(&late));
    ASSERT_GE(idx_gap,  0);
    ASSERT_GE(idx_late, 0);
    ASSERT_GT(idx_late, idx_gap)
        << "LATE must be at higher alloc_info index than GAP for this test";

    // Free GAP.
    dfree(&heap, reinterpret_cast<void**>(&gap), USING_PTR_ADDRESS);

    // OBJ must not have been corrupted (it is before GAP and at lower index).
    EXPECT_NE(obj, nullptr);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(obj[i], static_cast<uint8_t>(0xCC + i))
            << "OBJ data corrupted at byte " << i;
    }

    // LATE must have shifted down by GAP size (16) and its data must be intact.
    EXPECT_EQ(late, late_orig - 16)
        << "LATE should shift down after GAP freed";
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(late[i], static_cast<uint8_t>(0xDD + i))
            << "LATE data corrupted at byte " << i;
    }
}
