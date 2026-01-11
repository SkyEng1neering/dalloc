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
 * @file dalloc_conf.h
 * @brief Configuration options for dalloc memory allocator
 *
 * This file contains all configurable parameters for dalloc. You can override
 * any setting in two ways:
 *
 * 1. Compiler flags:
 *    @code
 *    gcc -DMAX_NUM_OF_ALLOCATIONS=200 -DUSE_SINGLE_HEAP_MEMORY ...
 *    @endcode
 *
 * 2. Custom configuration file:
 *    @code
 *    gcc -DDALLOC_CUSTOM_CONF_FILE=\"my_config.h\" ...
 *    @endcode
 *
 * Configuration options:
 * - General: DALLOC_VERSION, MAX_NUM_OF_ALLOCATIONS, dalloc_debug
 * - Memory: FILL_FREED_MEMORY_BY_NULLS
 * - Alignment: USE_ALIGNMENT, ALLOCATION_ALIGNMENT_BYTES
 * - Single heap: USE_SINGLE_HEAP_MEMORY
 * - Thread safety: USE_THREAD_SAFETY, USE_FREE_RTOS
 */

#ifndef DALLOCCONF_H
#define DALLOCCONF_H

#ifdef __cplusplus
	extern "C" {
#endif

/*
 * Custom configuration support:
 * Define DALLOC_CUSTOM_CONF_FILE to include your own configuration file.
 * Example: -DDALLOC_CUSTOM_CONF_FILE="my_dalloc_conf.h"
 *
 * In your custom config, define only the values you want to override.
 * Any undefined values will use the defaults below.
 */
#ifdef DALLOC_CUSTOM_CONF_FILE
	#include DALLOC_CUSTOM_CONF_FILE
#endif

/* ==================== Default Configuration ==================== */

#ifndef DALLOC_VERSION
#define DALLOC_VERSION                      "1.5.0"
#endif

/* Fill freed memory with zeros for security/debugging */
#ifndef FILL_FREED_MEMORY_BY_NULLS
#define FILL_FREED_MEMORY_BY_NULLS          true
#endif

/* Debug output function (set to empty macro to disable) */
#ifndef dalloc_debug
#define dalloc_debug                        printf
#endif

/* Maximum number of simultaneous allocations */
#ifndef MAX_NUM_OF_ALLOCATIONS
#define MAX_NUM_OF_ALLOCATIONS              100UL
#endif

/* ==================== Alignment Configuration ==================== */

/* Define USE_ALIGNMENT to enable memory alignment (recommended) */
#ifndef USE_ALIGNMENT
#define USE_ALIGNMENT
#endif

#ifdef USE_ALIGNMENT
	#ifndef ALLOCATION_ALIGNMENT_BYTES
	#define ALLOCATION_ALIGNMENT_BYTES      4U
	#endif

	/* Align value up to the nearest multiple of ALLOCATION_ALIGNMENT_BYTES */
	#ifndef DALLOC_ALIGN_UP
	#define DALLOC_ALIGN_UP(val) (((val) + (ALLOCATION_ALIGNMENT_BYTES - 1U)) & ~(ALLOCATION_ALIGNMENT_BYTES - 1U))
	#endif
#endif

/* ==================== Single Heap Configuration ==================== */

/*
 * Define USE_SINGLE_HEAP_MEMORY to use a single global heap.
 * This enables convenient def_dalloc(), def_dfree() functions.
 *
 * Usage:
 *   1. Define USE_SINGLE_HEAP_MEMORY
 *   2. Create a buffer (can be placed in specific memory section)
 *   3. Call dalloc_register_heap(buffer, size) before any allocations
 *
 * Example:
 *   __attribute__((section(".ccmram"))) uint8_t my_heap[8192];
 *   dalloc_register_heap(my_heap, sizeof(my_heap));
 *   // Now use def_dalloc(), def_dfree(), etc.
 */
#ifndef USE_SINGLE_HEAP_MEMORY
/* #define USE_SINGLE_HEAP_MEMORY */
#endif

/* ==================== Thread Safety Configuration ==================== */

/*
 * Define USE_THREAD_SAFETY to enable thread-safe operations.
 * Each heap will have its own mutex for protection.
 *
 * If USE_FREE_RTOS is also defined, FreeRTOS mutex primitives will be used
 * automatically. Otherwise, you must define the following macros:
 *
 *   DALLOC_MUTEX_TYPE              - The mutex handle type
 *   DALLOC_MUTEX_CREATE()          - Create and return a mutex handle
 *   DALLOC_MUTEX_DELETE(mutex)     - Delete/destroy the mutex
 *   DALLOC_MUTEX_LOCK(mutex)       - Acquire the mutex (blocking)
 *   DALLOC_MUTEX_UNLOCK(mutex)     - Release the mutex
 *
 * Example for POSIX threads:
 *   #define DALLOC_MUTEX_TYPE              pthread_mutex_t*
 *   #define DALLOC_MUTEX_CREATE()          ({ \
 *       pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t)); \
 *       pthread_mutex_init(m, NULL); m; })
 *   #define DALLOC_MUTEX_DELETE(mutex)     do { pthread_mutex_destroy(mutex); free(mutex); } while(0)
 *   #define DALLOC_MUTEX_LOCK(mutex)       pthread_mutex_lock(mutex)
 *   #define DALLOC_MUTEX_UNLOCK(mutex)     pthread_mutex_unlock(mutex)
 */
#ifndef USE_THREAD_SAFETY
/* #define USE_THREAD_SAFETY */
#endif

#ifdef USE_THREAD_SAFETY

/* FreeRTOS implementation */
#ifdef USE_FREE_RTOS
	#include "FreeRTOS.h"
	#include "semphr.h"

	#ifndef DALLOC_MUTEX_TYPE
	#define DALLOC_MUTEX_TYPE               SemaphoreHandle_t
	#endif

	#ifndef DALLOC_MUTEX_CREATE
	#define DALLOC_MUTEX_CREATE()           xSemaphoreCreateMutex()
	#endif

	#ifndef DALLOC_MUTEX_DELETE
	#define DALLOC_MUTEX_DELETE(mutex)      do { if(mutex) vSemaphoreDelete(mutex); } while(0)
	#endif

	#ifndef DALLOC_MUTEX_LOCK
	#define DALLOC_MUTEX_LOCK(mutex)        do { if(mutex) xSemaphoreTake(mutex, portMAX_DELAY); } while(0)
	#endif

	#ifndef DALLOC_MUTEX_UNLOCK
	#define DALLOC_MUTEX_UNLOCK(mutex)      do { if(mutex) xSemaphoreGive(mutex); } while(0)
	#endif

#else /* Custom OS - user must define mutex macros */

	#ifndef DALLOC_MUTEX_TYPE
	#error "USE_THREAD_SAFETY defined but DALLOC_MUTEX_TYPE is not defined. Define mutex macros or enable USE_FREE_RTOS."
	#endif

	#ifndef DALLOC_MUTEX_CREATE
	#error "USE_THREAD_SAFETY defined but DALLOC_MUTEX_CREATE is not defined."
	#endif

	#ifndef DALLOC_MUTEX_DELETE
	#error "USE_THREAD_SAFETY defined but DALLOC_MUTEX_DELETE is not defined."
	#endif

	#ifndef DALLOC_MUTEX_LOCK
	#error "USE_THREAD_SAFETY defined but DALLOC_MUTEX_LOCK is not defined."
	#endif

	#ifndef DALLOC_MUTEX_UNLOCK
	#error "USE_THREAD_SAFETY defined but DALLOC_MUTEX_UNLOCK is not defined."
	#endif

#endif /* USE_FREE_RTOS */

#endif /* USE_THREAD_SAFETY */

#ifdef __cplusplus
	}
#endif

#endif /* DALLOCCONF_H */
