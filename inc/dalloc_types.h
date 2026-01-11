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
 * @file dalloc_types.h
 * @brief Type definitions for dalloc memory allocator
 *
 * This file defines the core data structures used by dalloc:
 * - heap_t: Main heap structure containing memory and allocation tracking
 * - ptr_info_t: Information about a single allocation
 * - alloc_info_t: Collection of all allocation tracking data
 */

#ifndef DALLOCTYPES_H
#define DALLOCTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "dalloc_conf.h"

/**
 * @brief Status codes for dalloc operations
 * @deprecated Use bool return values instead
 */
typedef enum {
	DALLOC_OK = 0,   /**< Operation successful */
	DALLOC_ERR       /**< Operation failed */
} dalloc_stat_t;

/**
 * @brief Pointer validation mode for dfree() and validate_ptr()
 *
 * Determines how the allocator finds the allocation to operate on.
 */
typedef enum {
	/**
	 * Match by pointer variable address (recommended).
	 * Use when you have the same pointer variable that was passed to dalloc().
	 */
	USING_PTR_ADDRESS = 0,

	/**
	 * Match by pointer value.
	 * Use when the original pointer variable is no longer available,
	 * but you have another variable containing the same memory address.
	 */
	USING_PTR_VALUE
} validate_ptr_condition_t;

/**
 * @brief Information about a single memory allocation
 *
 * Stored in the heap's allocation tracking array. Contains the address
 * of the user's pointer variable, allowing dalloc to update it during
 * defragmentation.
 */
typedef struct {
	uint8_t **ptr;          /**< Address of user's pointer variable */
	uint32_t allocated_size; /**< Size of allocation in bytes (before alignment) */
	bool free_flag;          /**< true if marked for freeing during defrag */
} ptr_info_t;

/**
 * @brief Allocation tracking information for a heap
 *
 * Maintains an array of all active allocations and usage statistics.
 */
typedef struct {
	/** Array of allocation info, indexed 0 to allocations_num-1 */
	ptr_info_t ptr_info_arr[MAX_NUM_OF_ALLOCATIONS];

	uint32_t allocations_num;       /**< Current number of active allocations */
	uint32_t max_memory_amount;     /**< Peak memory usage (bytes) - for statistics */
	uint32_t max_allocations_amount; /**< Peak allocation count - for statistics */
} alloc_info_t;

/**
 * @brief Main heap structure
 *
 * Represents a memory heap with its buffer, current state, and allocation
 * tracking. Can be initialized with heap_init() and deinitialized with
 * heap_deinit().
 *
 * Memory layout:
 * @code
 *   mem[0]                    mem[offset]              mem[total_size-1]
 *   |                         |                        |
 *   v                         v                        v
 *   +-------------------------+------------------------+
 *   |   Allocated Memory      |     Free Space         |
 *   +-------------------------+------------------------+
 * @endcode
 *
 * When memory is freed, defragmentation compacts all allocations to the
 * beginning, updating tracked pointers automatically.
 */
typedef struct {
	uint8_t *mem;           /**< Pointer to the memory buffer used for allocations */
	uint32_t offset;        /**< Current allocation offset (bytes used) */
	uint32_t total_size;    /**< Total size of memory buffer in bytes */
	alloc_info_t alloc_info; /**< Allocation tracking data */
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_TYPE mutex; /**< Mutex for thread-safe operations */
#endif
} heap_t;

#ifdef __cplusplus
}
#endif

#endif /* DALLOCTYPES_H */
