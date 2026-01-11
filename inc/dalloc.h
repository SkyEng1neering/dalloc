/*
 * Copyright 2021 Alexey Vasilenko
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @file dalloc.h
 * @brief Defragmenting Memory Allocator - Public API
 *
 * dalloc is a memory allocator designed for embedded systems that automatically
 * defragments memory when allocations are freed. Unlike standard malloc, dalloc
 * tracks pointer ADDRESSES (not just values), allowing it to update user pointers
 * when memory is moved during defragmentation.
 *
 * Key features:
 * - Automatic memory defragmentation on free
 * - Optional thread safety (USE_THREAD_SAFETY)
 * - Single global heap mode (USE_SINGLE_HEAP_MEMORY) or multiple explicit heaps
 * - Configurable alignment and allocation limits
 *
 * Important: The pointer variable passed to dalloc() must remain valid until
 * dfree() is called, as dalloc stores the ADDRESS of the pointer.
 *
 * @see dalloc_conf.h for configuration options
 * @see dalloc_types.h for type definitions
 */

#ifndef DALLOC_H
#define DALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dalloc_types.h"

/* ============================================================================
 * Single Heap API (USE_SINGLE_HEAP_MEMORY)
 * ============================================================================
 * These functions operate on a single global heap. Enable by defining
 * USE_SINGLE_HEAP_MEMORY in your configuration.
 * ========================================================================== */

#ifdef USE_SINGLE_HEAP_MEMORY

/**
 * @brief Register a user-provided buffer as the default heap
 *
 * This function must be called before any allocation operations when using
 * single heap mode. The buffer can be placed in any memory section
 * (e.g., CCM RAM, external SRAM).
 *
 * @param buffer Pointer to user's buffer
 * @param size   Size of the buffer in bytes
 * @return true if registration successful, false if:
 *         - buffer is NULL
 *         - size is 0
 *         - heap is already registered
 *
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 *
 * Example:
 * @code
 *   __attribute__((section(".ccmram"))) uint8_t my_heap[8192];
 *   if (!dalloc_register_heap(my_heap, sizeof(my_heap))) {
 *       // Handle error
 *   }
 * @endcode
 */
bool dalloc_register_heap(void *buffer, uint32_t size);

/**
 * @brief Unregister the default heap
 *
 * Releases the heap and destroys associated resources (including mutex if
 * thread safety is enabled).
 *
 * @param force If false, fails when active allocations exist.
 *              If true, forces unregister regardless of allocations.
 * @return true if successfully unregistered, false if:
 *         - no heap is registered
 *         - force=false and allocations exist
 *
 * @warning Using force=true with active allocations causes dangling pointers.
 *          Consider using dalloc_reset_heap() first to invalidate pointers.
 */
bool dalloc_unregister_heap(bool force);

/**
 * @brief Reset heap - reinitialize with the same buffer
 *
 * Clears all allocations and reinitializes the heap. Useful for quickly
 * releasing all memory without tracking individual allocations.
 *
 * @note All existing pointers become invalid after this call
 * @note The heap remains registered after reset
 */
void dalloc_reset_heap(void);

/**
 * @brief Check if default heap has been registered
 * @return true if heap is registered and ready to use
 */
bool dalloc_is_initialized(void);

/**
 * @brief Get pointer to the default heap structure
 *
 * Provides direct access to the heap structure for advanced operations
 * or inspection.
 *
 * @return Pointer to default heap, or NULL if not registered
 */
heap_t* dalloc_get_default_heap(void);

/**
 * @brief Allocate memory from the default heap
 *
 * @param size Number of bytes to allocate
 * @param ptr  Address of pointer variable to receive allocation.
 *             Set to NULL on failure.
 *
 * @note ptr must remain valid until def_dfree() is called
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 *
 * Example:
 * @code
 *   uint8_t *data = NULL;
 *   def_dalloc(64, (void**)&data);
 *   if (data != NULL) {
 *       // Use data...
 *       def_dfree((void**)&data);
 *   }
 * @endcode
 */
void def_dalloc(uint32_t size, void **ptr);

/**
 * @brief Free memory allocated from the default heap
 *
 * Frees the allocation and triggers automatic defragmentation.
 * The pointer is set to NULL after freeing.
 *
 * @param ptr Address of pointer to free (same address passed to def_dalloc)
 *
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
void def_dfree(void **ptr);

/**
 * @brief Reallocate memory from the default heap
 *
 * Changes the size of an existing allocation. Data is preserved up to
 * the minimum of old and new sizes.
 *
 * @param size New size in bytes
 * @param ptr  Address of pointer to reallocate
 * @return true on success, false if:
 *         - heap not registered
 *         - ptr is not a valid allocation
 *         - insufficient memory for new size
 *
 * @note On failure, the original allocation is preserved
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
bool def_drealloc(uint32_t size, void **ptr);

/**
 * @brief Transfer allocation ownership to a new pointer variable
 *
 * Moves the allocation tracking from one pointer variable to another.
 * The old pointer is set to NULL, and the new pointer receives the value.
 *
 * @param ptr_to_replace Address of current pointer (set to NULL after call)
 * @param ptr_new        Address of new pointer to track
 *
 * @note Both pointers must be valid addresses
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
void def_replace_pointers(void **ptr_to_replace, void **ptr_new);

/**
 * @brief Print default heap statistics to debug output
 */
void print_def_dalloc_info(void);

/**
 * @brief Dump default heap memory contents to debug output
 */
void dump_def_heap(void);

/**
 * @brief Dump default heap pointer tracking info to debug output
 */
void dump_def_dalloc_ptr_info(void);

#endif /* USE_SINGLE_HEAP_MEMORY */


/* ============================================================================
 * Multi-Heap API (Explicit heap parameter)
 * ============================================================================
 * These functions operate on an explicit heap_t structure, allowing multiple
 * independent heaps.
 * ========================================================================== */

/**
 * @brief Initialize a heap structure with a memory buffer
 *
 * Prepares a heap for use. The memory buffer is zeroed during initialization.
 * If thread safety is enabled, creates a mutex for the heap.
 *
 * @param heap_struct_ptr Pointer to heap_t structure to initialize
 * @param mem_ptr         Pointer to memory buffer for heap storage
 * @param mem_size        Size of memory buffer in bytes
 *
 * @note Does nothing if any parameter is NULL or size is 0
 *
 * Example:
 * @code
 *   uint8_t buffer[4096];
 *   heap_t heap;
 *   heap_init(&heap, buffer, sizeof(buffer));
 * @endcode
 */
void heap_init(heap_t *heap_struct_ptr, void *mem_ptr, uint32_t mem_size);

/**
 * @brief Deinitialize a heap structure
 *
 * Releases heap resources. If thread safety is enabled, destroys the mutex.
 * Does NOT free any active allocations - they become invalid.
 *
 * @param heap_struct_ptr Pointer to heap_t structure to deinitialize
 *
 * @warning Active allocations become dangling pointers after this call
 */
void heap_deinit(heap_t *heap_struct_ptr);

/**
 * @brief Allocate memory from a heap
 *
 * Allocates size bytes from the specified heap. The allocation is tracked
 * by the ADDRESS of ptr, not its value.
 *
 * @param heap_struct_ptr Pointer to heap structure
 * @param size            Number of bytes to allocate
 * @param ptr             Address of pointer variable to receive allocation
 *
 * @note ptr is set to NULL on failure
 * @note ptr variable must remain valid until dfree() is called
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 *
 * Example:
 * @code
 *   uint8_t *data = NULL;
 *   dalloc(&heap, 64, (void**)&data);
 *   if (data != NULL) {
 *       // Use data...
 *   }
 * @endcode
 */
void dalloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);

/**
 * @brief Free memory and trigger defragmentation
 *
 * Frees the specified allocation and compacts remaining allocations to
 * eliminate fragmentation. All tracked pointers are updated automatically.
 *
 * @param heap_struct_ptr Pointer to heap structure
 * @param ptr             Address of pointer to free
 * @param condition       How to find the allocation:
 *                        - USING_PTR_ADDRESS: Match by pointer address (recommended)
 *                        - USING_PTR_VALUE: Match by pointer value
 *
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
void dfree(heap_t *heap_struct_ptr, void **ptr, validate_ptr_condition_t condition);

/**
 * @brief Reallocate memory with a new size
 *
 * Changes the size of an existing allocation. Data is preserved up to
 * the minimum of old and new sizes. Internally allocates new memory,
 * copies data, and frees the old allocation.
 *
 * @param heap_struct_ptr Pointer to heap structure
 * @param size            New size in bytes
 * @param ptr             Address of pointer to reallocate
 * @return true on success, false on failure
 *
 * @note On failure, the original allocation is preserved
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
bool drealloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);

/**
 * @brief Validate if a pointer is tracked by the heap
 *
 * Checks if a pointer is a valid allocation in the specified heap.
 *
 * @param heap_struct_ptr Pointer to heap structure
 * @param ptr             Address of pointer to validate
 * @param condition       How to match:
 *                        - USING_PTR_ADDRESS: Match by pointer address
 *                        - USING_PTR_VALUE: Match by pointer value
 * @param ptr_index       Output: index in allocation array (can be NULL)
 * @return true if pointer is valid, false otherwise
 */
bool validate_ptr(heap_t *heap_struct_ptr, void **ptr,
                  validate_ptr_condition_t condition, uint32_t *ptr_index);

/**
 * @brief Transfer allocation ownership to a new pointer variable
 *
 * Updates the heap's tracking to use a new pointer variable address.
 * The old pointer is set to NULL.
 *
 * @param heap_struct_ptr Pointer to heap structure
 * @param ptr_to_replace  Address of current pointer (set to NULL)
 * @param ptr_new         Address of new pointer to track
 *
 * @note Thread-safe if USE_THREAD_SAFETY is enabled
 */
void replace_pointers(heap_t *heap_struct_ptr, void **ptr_to_replace, void **ptr_new);

/**
 * @brief Print heap statistics to debug output
 *
 * Outputs: total size, used bytes, allocation count, peak usage.
 *
 * @param heap_struct_ptr Pointer to heap structure
 */
void print_dalloc_info(heap_t *heap_struct_ptr);

/**
 * @brief Dump heap memory contents to debug output
 *
 * Outputs raw hex dump of the entire heap memory.
 *
 * @param heap_struct_ptr Pointer to heap structure
 */
void dump_heap(heap_t *heap_struct_ptr);

/**
 * @brief Dump pointer tracking information to debug output
 *
 * Outputs details of each tracked allocation: address, first byte value, size.
 *
 * @param heap_struct_ptr Pointer to heap structure
 */
void dump_dalloc_ptr_info(heap_t *heap_struct_ptr);

#ifdef __cplusplus
}
#endif

#endif /* DALLOC_H */
