#ifndef DALLOCTYPES_H
#define DALLOCTYPES_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"

#define DALLOC_VERSION					"1.1.0"

#define FILL_FREED_MEMORY_BY_NULLS		false
#define dalloc_debug					printf
#define MAX_NUM_OF_ALLOCATIONS       	100UL
#define USE_ALIGNMENT

#ifdef USE_ALIGNMENT
#define ALLOCATION_ALIGNMENT_BYTES    	4U
#endif

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
