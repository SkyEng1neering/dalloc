/*
 * example.c - dalloc usage examples
 *
 * This file demonstrates both Multi-Heap and Single-Heap APIs.
 * Compile with: gcc -I./inc example.c src/dalloc.c -o example
 *
 * For Single Heap API, compile with:
 *   gcc -DUSE_SINGLE_HEAP_MEMORY -I./inc example.c src/dalloc.c -o example
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "dalloc.h"

/* ============================================================================
 * Multi-Heap API Example
 * ============================================================================ */

#define HEAP_SIZE  256

/* Heap buffer - must be aligned! */
__attribute__((aligned(4)))
static uint8_t heap_buffer[HEAP_SIZE];

void multi_heap_example(void) {
    heap_t heap;

    printf("=== Multi-Heap API Example ===\n\n");

    /* Initialize heap */
    heap_init(&heap, heap_buffer, HEAP_SIZE);

    /* Allocate first block */
    uint8_t *block1 = NULL;
    dalloc(&heap, 32, (void**)&block1);

    if (block1 == NULL) {
        printf("Allocation failed!\n");
        return;
    }

    printf("Allocated 32 bytes at %p\n", (void*)block1);
    memset(block1, 0xAA, 32);

    /* Allocate second block */
    uint8_t *block2 = NULL;
    dalloc(&heap, 64, (void**)&block2);

    if (block2 == NULL) {
        printf("Allocation failed!\n");
        dfree(&heap, (void**)&block1, USING_PTR_ADDRESS);
        return;
    }

    printf("Allocated 64 bytes at %p\n", (void*)block2);
    memset(block2, 0xBB, 64);

    /* Print heap info */
    print_dalloc_info(&heap);

    /* Free first block - triggers defragmentation */
    printf("\nFreeing first block (defragmentation will occur)...\n");
    dfree(&heap, (void**)&block1, USING_PTR_ADDRESS);

    /* block2 pointer was automatically updated! */
    printf("block2 moved to %p after defragmentation\n", (void*)block2);

    /* Verify data integrity */
    int data_ok = 1;
    for (int i = 0; i < 64; i++) {
        if (block2[i] != 0xBB) {
            data_ok = 0;
            break;
        }
    }
    printf("Data integrity after defrag: %s\n", data_ok ? "OK" : "FAILED");

    /* Reallocate block2 to larger size */
    printf("\nReallocating block2 from 64 to 128 bytes...\n");
    if (drealloc(&heap, 128, (void**)&block2)) {
        printf("Reallocation successful, new address: %p\n", (void*)block2);
    }

    /* Cleanup */
    dfree(&heap, (void**)&block2, USING_PTR_ADDRESS);
    heap_deinit(&heap);

    printf("\nHeap deinitialized.\n");
}

/* ============================================================================
 * Single-Heap API Example (requires USE_SINGLE_HEAP_MEMORY)
 * ============================================================================ */

#ifdef USE_SINGLE_HEAP_MEMORY

#define DEFAULT_HEAP_SIZE  512

__attribute__((aligned(4)))
static uint8_t default_heap_buffer[DEFAULT_HEAP_SIZE];

void single_heap_example(void) {
    printf("\n=== Single-Heap API Example ===\n\n");

    /* Register heap buffer */
    if (!dalloc_register_heap(default_heap_buffer, DEFAULT_HEAP_SIZE)) {
        printf("Failed to register heap!\n");
        return;
    }

    printf("Heap registered: %u bytes\n", DEFAULT_HEAP_SIZE);

    /* Allocate using simplified API */
    uint8_t *data = NULL;
    def_dalloc(100, (void**)&data);

    if (data == NULL) {
        printf("Allocation failed!\n");
        dalloc_unregister_heap(true);
        return;
    }

    printf("Allocated 100 bytes at %p\n", (void*)data);

    /* Fill with pattern */
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)i;
    }

    /* Reallocate */
    if (def_drealloc(200, (void**)&data)) {
        printf("Reallocated to 200 bytes at %p\n", (void*)data);

        /* Verify original data preserved */
        int ok = 1;
        for (int i = 0; i < 100; i++) {
            if (data[i] != (uint8_t)i) {
                ok = 0;
                break;
            }
        }
        printf("Original data preserved: %s\n", ok ? "OK" : "FAILED");
    }

    /* Check heap state */
    heap_t *heap = dalloc_get_default_heap();
    if (heap) {
        printf("\nHeap state:\n");
        printf("  Used: %lu / %lu bytes\n",
               (unsigned long)heap->offset,
               (unsigned long)heap->total_size);
        printf("  Allocations: %lu\n",
               (unsigned long)heap->alloc_info.allocations_num);
    }

    /* Free memory */
    def_dfree((void**)&data);
    printf("\nMemory freed, data pointer is now: %p\n", (void*)data);

    /* Unregister heap */
    if (dalloc_unregister_heap(false)) {
        printf("Heap unregistered successfully.\n");
    }
}

#endif /* USE_SINGLE_HEAP_MEMORY */

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("dalloc v%s - Example Program\n", DALLOC_VERSION);
    printf("========================================\n\n");

    /* Multi-heap example always available */
    multi_heap_example();

#ifdef USE_SINGLE_HEAP_MEMORY
    /* Single-heap example only when enabled */
    single_heap_example();
#else
    printf("\n(Single-Heap API not enabled. Compile with -DUSE_SINGLE_HEAP_MEMORY)\n");
#endif

    printf("\nDone.\n");
    return 0;
}
