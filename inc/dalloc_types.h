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

#ifndef DALLOCTYPES_H
#define DALLOCTYPES_H

#ifdef __cplusplus
	extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "dalloc_conf.h"

typedef enum {
	DALLOC_OK = 0,
	DALLOC_ERR
} dalloc_stat_t;

typedef enum {
	USING_PTR_ADDRESS = 0,
	USING_PTR_VALUE
} validate_ptr_condition_t;

typedef struct {
	uint8_t **ptr;
	uint32_t allocated_size;
} ptr_info_t;

typedef struct {
	ptr_info_t ptr_info_arr[MAX_NUM_OF_ALLOCATIONS];
	uint32_t allocations_num;
	uint32_t max_memory_amount;
	uint32_t max_allocations_amount;
} alloc_info_t;

typedef struct {
	uint8_t *mem;					/* Pointer to the memory area using for heap */
	uint32_t offset;				/* Size of currently allocated memory */
	uint32_t total_size;			/* Total size of memory that can be used for allocation memory */
	alloc_info_t alloc_info;
} heap_t;

#ifdef __cplusplus
	}
#endif

#endif // DALLOCTYPES_H
