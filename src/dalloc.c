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

#include "dalloc.h"

#ifdef USE_SINGLE_HEAP_MEMORY
/* Default heap state */
static heap_t default_heap;
static bool heap_registered = false;
static void *registered_buffer = NULL;
static uint32_t registered_size = 0;

bool dalloc_register_heap(void *buffer, uint32_t size){
	if(!buffer || !size){
		dalloc_debug("dalloc_register_heap: Invalid parameters\n");
		return false;
	}
	if(heap_registered){
		dalloc_debug("dalloc_register_heap: Heap already registered\n");
		return false;
	}
	heap_init(&default_heap, buffer, size);
	registered_buffer = buffer;
	registered_size = size;
	heap_registered = true;
	return true;
}

bool dalloc_unregister_heap(bool force){
	if(!heap_registered){
		dalloc_debug("dalloc_unregister_heap: No heap registered\n");
		return false;
	}
	if(!force && default_heap.alloc_info.allocations_num > 0){
		dalloc_debug("dalloc_unregister_heap: Heap has %lu active allocations. Use force=true or free them first\n",
			(unsigned long)default_heap.alloc_info.allocations_num);
		return false;
	}
	heap_deinit(&default_heap);
	heap_registered = false;
	registered_buffer = NULL;
	registered_size = 0;
	return true;
}

void dalloc_reset_heap(void){
	if(!heap_registered){
		dalloc_debug("dalloc_reset_heap: No heap registered\n");
		return;
	}
#ifdef USE_THREAD_SAFETY
	/* Delete old mutex before reinitializing */
	DALLOC_MUTEX_DELETE(default_heap.mutex);
#endif
	/* Reinitialize with the same buffer (creates new mutex) */
	heap_init(&default_heap, registered_buffer, registered_size);
}

bool dalloc_is_initialized(void){
	return heap_registered;
}

heap_t* dalloc_get_default_heap(void){
	if(!heap_registered){
		return NULL;
	}
	return &default_heap;
}

void def_dalloc(uint32_t size, void **ptr){
	if(!heap_registered){
		dalloc_debug("def_dalloc: Heap not registered. Call dalloc_register_heap() first\n");
		if(ptr) *ptr = NULL;
		return;
	}
	dalloc(&default_heap, size, ptr);
}

void def_dfree(void **ptr){
	if(!heap_registered){
		dalloc_debug("def_dfree: Heap not registered\n");
		return;
	}
	dfree(&default_heap, ptr, USING_PTR_ADDRESS);
}

void def_replace_pointers(void **ptr_to_replace, void **ptr_new){
	if(!heap_registered){
		dalloc_debug("def_replace_pointers: Heap not registered\n");
		return;
	}
	replace_pointers(&default_heap, ptr_to_replace, ptr_new);
}

bool def_drealloc(uint32_t size, void **ptr){
	if(!heap_registered){
		dalloc_debug("def_drealloc: Heap not registered\n");
		return false;
	}
	return drealloc(&default_heap, size, ptr);
}

void def_heap_lock(void){
	if(!heap_registered){
		return;
	}
	heap_lock(&default_heap);
}

void def_heap_unlock(void){
	if(!heap_registered){
		return;
	}
	heap_unlock(&default_heap);
}

void print_def_dalloc_info(void){
	if(!heap_registered){
		dalloc_debug("print_def_dalloc_info: Heap not registered\n");
		return;
	}
	print_dalloc_info(&default_heap);
}

int def_dalloc_heap_info_str(char *buf, uint32_t buf_size){
	if(!heap_registered){
		if(buf && buf_size > 0) buf[0] = '\0';
		return -1;
	}
	return dalloc_heap_info_str(&default_heap, buf, buf_size);
}

void dump_def_heap(void){
	if(!heap_registered){
		dalloc_debug("dump_def_heap: Heap not registered\n");
		return;
	}
	dump_heap(&default_heap);
}

void dump_def_dalloc_ptr_info(void){
	if(!heap_registered){
		dalloc_debug("dump_def_dalloc_ptr_info: Heap not registered\n");
		return;
	}
	dump_dalloc_ptr_info(&default_heap);
}
#endif

void heap_init(heap_t* heap_struct_ptr, void *mem_ptr, uint32_t mem_size){
	if(!heap_struct_ptr || !mem_ptr || !mem_size){
		return;
	}
	heap_struct_ptr->offset = 0;
	heap_struct_ptr->mem = (uint8_t*)mem_ptr;
	heap_struct_ptr->total_size = mem_size;
	heap_struct_ptr->alloc_info.allocations_num = 0;
	heap_struct_ptr->alloc_info.max_memory_amount = 0;
	heap_struct_ptr->alloc_info.max_allocations_amount = 0;
	for(uint32_t i = 0; i < MAX_NUM_OF_ALLOCATIONS; i++){
		heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr = NULL;
		heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size = 0;
		heap_struct_ptr->alloc_info.ptr_info_arr[i].free_flag = true;
	}
	memset(heap_struct_ptr->mem, 0, heap_struct_ptr->total_size);
#ifdef USE_THREAD_SAFETY
	heap_struct_ptr->mutex = DALLOC_MUTEX_CREATE();
	if(!heap_struct_ptr->mutex){
		dalloc_debug("heap_init: Failed to create mutex\n");
	}
#endif
}

void heap_deinit(heap_t* heap_struct_ptr){
	if(!heap_struct_ptr){
		return;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_DELETE(heap_struct_ptr->mutex);
	heap_struct_ptr->mutex = NULL;
#endif
	heap_struct_ptr->mem = NULL;
	heap_struct_ptr->offset = 0;
	heap_struct_ptr->total_size = 0;
}

/* Internal allocation function - no locking, called with mutex held */
static void dalloc_internal(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
	uint32_t new_offset = heap_struct_ptr->offset + size;

	/* Correct offset if use alignment */
#ifdef USE_ALIGNMENT
	new_offset = DALLOC_ALIGN_UP(new_offset);
#endif

	/* Check if there is enough memory for new allocation, and if number of allocations is exceeded */
	if((new_offset <= heap_struct_ptr->total_size) && (heap_struct_ptr->alloc_info.allocations_num < MAX_NUM_OF_ALLOCATIONS)) {
		*ptr = heap_struct_ptr->mem + heap_struct_ptr->offset;
		heap_struct_ptr->offset = new_offset;

		/* Save info about allocated memory */
		heap_struct_ptr->alloc_info.ptr_info_arr[heap_struct_ptr->alloc_info.allocations_num].ptr = (uint8_t**)ptr;
		heap_struct_ptr->alloc_info.ptr_info_arr[heap_struct_ptr->alloc_info.allocations_num].allocated_size = size;
		heap_struct_ptr->alloc_info.ptr_info_arr[heap_struct_ptr->alloc_info.allocations_num].free_flag = false;
		heap_struct_ptr->alloc_info.allocations_num = heap_struct_ptr->alloc_info.allocations_num + 1;

		if(heap_struct_ptr->offset > heap_struct_ptr->alloc_info.max_memory_amount){
			heap_struct_ptr->alloc_info.max_memory_amount = heap_struct_ptr->offset;
		}
		if(heap_struct_ptr->alloc_info.allocations_num > heap_struct_ptr->alloc_info.max_allocations_amount){
			heap_struct_ptr->alloc_info.max_allocations_amount = heap_struct_ptr->alloc_info.allocations_num;
		}
	}
	else {
		dalloc_debug("dalloc: Allocation failed\n");
		dalloc_debug("Total: %lu, Used: %lu, Allocs: %lu\n",
			(long unsigned int)heap_struct_ptr->total_size,
			(long unsigned int)heap_struct_ptr->offset,
			(long unsigned int)heap_struct_ptr->alloc_info.allocations_num);
		*ptr = NULL;
		if(new_offset > heap_struct_ptr->total_size){
			dalloc_debug("dalloc: Heap size exceeded\n");
		}
		if(heap_struct_ptr->alloc_info.allocations_num >= MAX_NUM_OF_ALLOCATIONS){
			dalloc_debug("dalloc: Max number of allocations exceeded: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.allocations_num);
		}
	}
}

void dalloc(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
	if(!ptr){
		return;
	}
	if(!heap_struct_ptr || !size){
		*ptr = NULL;
		return;
	}

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif

	dalloc_internal(heap_struct_ptr, size, ptr);

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}

/* Internal validate_ptr - no locking, called with mutex held */
static bool validate_ptr_internal(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition, uint32_t *ptr_index){
	if(!heap_struct_ptr || !ptr){
		return false;
	}
	for(uint32_t i = 0; i < heap_struct_ptr->alloc_info.allocations_num; i++){
		if(condition == USING_PTR_ADDRESS){
			if(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr == (uint8_t**)ptr){
				if(ptr_index != NULL){
					*ptr_index = i;
				}
				return true;
			}
		}
		else {
			if(*(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr) == *ptr){
				if(ptr_index != NULL){
					*ptr_index = i;
				}
				return true;
			}
		}
	}
	return false;
}

bool validate_ptr(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition, uint32_t *ptr_index){
	if(!heap_struct_ptr || !ptr){
		return false;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif
	bool result = validate_ptr_internal(heap_struct_ptr, ptr, condition, ptr_index);
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
	return result;
}

bool is_ptr_address_in_heap_area(heap_t* heap_struct_ptr, void **ptr){
    if(!heap_struct_ptr || !ptr){
        return false;
    }
    size_t heap_start_area = (size_t)(heap_struct_ptr->mem);
    size_t heap_stop_area = (size_t)(heap_struct_ptr->mem) + heap_struct_ptr->total_size;
    if(((size_t)ptr >= heap_start_area) && ((size_t)ptr <= heap_stop_area)){
        return true;
    }
    return false;
}

void defrag_memory(heap_t* heap_struct_ptr){
    for(uint32_t i = 0; i < heap_struct_ptr->alloc_info.allocations_num; i++){
        if(heap_struct_ptr->alloc_info.ptr_info_arr[i].free_flag == true){
            /* Optimize memory */
            uint8_t *start_mem_ptr = *(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr);
            uint32_t start_ind = (uint32_t)(start_mem_ptr - heap_struct_ptr->mem);

            /* Set given ptr to NULL */
            *(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr) = NULL;

            uint32_t alloc_size = heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size;
#ifdef USE_ALIGNMENT
            alloc_size = DALLOC_ALIGN_UP(alloc_size);
#endif
            /* Adjust ptr ADDRESSES that fall inside the moved heap region.
             * Must scan ALL entries (not just k > i): an earlier alloc_info entry
             * can have its tracking variable relocated into a later heap buffer
             * via replace_pointers, so its .ptr address may be above start_mem_ptr
             * even when k < i. Skip only the freed entry itself (k == i). */
            for(uint32_t k = 0; k < heap_struct_ptr->alloc_info.allocations_num; k++){
                if(k == i) continue;
                if(is_ptr_address_in_heap_area(heap_struct_ptr, (void**)heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr)){
                    if((size_t)heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr > (size_t)(start_mem_ptr)){
                        heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr = (uint8_t**)((size_t)(heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr) - alloc_size);
                    }
                }
            }

            /* Defragmentate memory */
            uint32_t bytes_to_move = heap_struct_ptr->offset - start_ind - alloc_size;
            memmove(heap_struct_ptr->mem + start_ind,
                    heap_struct_ptr->mem + start_ind + alloc_size,
                    bytes_to_move);

            /* Reassign pointer VALUES that point into the moved region.
             * Must scan ALL entries (not just k > i): after replace_pointers
             * and prior defrag cycles, an earlier alloc_info entry can hold a
             * value that points above start_mem_ptr even when k < i.
             * Skip only the freed entry itself (k == i) — its ptr was set to
             * NULL above and must not be dereferenced. */
            for(uint32_t k = 0; k < heap_struct_ptr->alloc_info.allocations_num; k++){
                if(k == i) continue;
                if(*(heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr) > start_mem_ptr){
                    *(heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr) -= alloc_size;
                }
            }

            /* Actualize ptr info array */
            for(uint32_t k = i; k < heap_struct_ptr->alloc_info.allocations_num - 1; k++){
                heap_struct_ptr->alloc_info.ptr_info_arr[k] = heap_struct_ptr->alloc_info.ptr_info_arr[k + 1];
            }

            /* Decrement allocations number */
            heap_struct_ptr->alloc_info.allocations_num--;

            /* Refresh offset */
            heap_struct_ptr->offset = heap_struct_ptr->offset - alloc_size;

            /* Fill by 0 all freed memory */
#if FILL_FREED_MEMORY_BY_NULLS
            memset(heap_struct_ptr->mem + heap_struct_ptr->offset, 0, alloc_size);
#endif
        }
    }
}

/* Internal free function - no locking, called with mutex held */
static bool dfree_internal(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition){
	uint32_t ptr_index = 0;

	/* Try to find given ptr in ptr_info array */
	if(validate_ptr_internal(heap_struct_ptr, ptr, condition, &ptr_index) != true){
		dalloc_debug("Try to free unexisting pointer\n");
		return false;
	}

	uint32_t alloc_size = heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].allocated_size;
#ifdef USE_ALIGNMENT
	alloc_size = DALLOC_ALIGN_UP(alloc_size);
#endif

	/* Edit ptr info array */
	heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].free_flag = true;
#if FILL_FREED_MEMORY_BY_NULLS
	memset(*(heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].ptr), 0, alloc_size);
#endif

	defrag_memory(heap_struct_ptr);
	return true;
}

void dfree(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition){
	/* Check if heap_ptr or ptr is not assigned */
	if(heap_struct_ptr == NULL){
		dalloc_debug("Heap pointer is not assigned\n");
		return;
	}
	if(ptr == NULL){
		dalloc_debug("Pointer is not assigned\n");
		return;
	}

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif

	dfree_internal(heap_struct_ptr, ptr, condition);

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}

/* Internal replace_pointers - no locking, called with mutex held */
static bool replace_pointers_internal(heap_t* heap_struct_ptr, void **ptr_to_replace, void **ptr_new){
	uint32_t ptr_ind = 0;
	if(validate_ptr_internal(heap_struct_ptr, ptr_to_replace, USING_PTR_ADDRESS, &ptr_ind) != true){
		dalloc_debug("Can't replace pointers. No pointer found in buffer\n");
		return false;
	}
	*ptr_new = *ptr_to_replace;
	heap_struct_ptr->alloc_info.ptr_info_arr[ptr_ind].ptr = (uint8_t**)ptr_new;
	*ptr_to_replace = NULL;
	return true;
}

void replace_pointers(heap_t* heap_struct_ptr, void **ptr_to_replace, void **ptr_new){
	if(!heap_struct_ptr || !ptr_to_replace || !ptr_new){
		dalloc_debug("replace_pointers: Invalid parameters\n");
		return;
	}

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif

	replace_pointers_internal(heap_struct_ptr, ptr_to_replace, ptr_new);

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}

void heap_lock(heap_t *heap_struct_ptr){
#ifdef USE_THREAD_SAFETY
	if(heap_struct_ptr){
		DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
	}
#else
	(void)heap_struct_ptr;
#endif
}

void heap_unlock(heap_t *heap_struct_ptr){
#ifdef USE_THREAD_SAFETY
	if(heap_struct_ptr){
		DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
	}
#else
	(void)heap_struct_ptr;
#endif
}

bool drealloc(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
	if(!heap_struct_ptr || !ptr){
		return false;
	}

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif

	uint32_t size_of_old_block = 0;
	uint32_t old_ptr_ind = 0;
	if(validate_ptr_internal(heap_struct_ptr, ptr, USING_PTR_ADDRESS, &old_ptr_ind) == true){
		size_of_old_block = heap_struct_ptr->alloc_info.ptr_info_arr[old_ptr_ind].allocated_size;
	}
	else {
#ifdef USE_THREAD_SAFETY
		DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
		return false;
	}

	uint8_t *new_ptr = NULL;
	dalloc_internal(heap_struct_ptr, size, (void**)&new_ptr);

	/* Check if allocation failed */
	if(new_ptr == NULL){
#ifdef USE_THREAD_SAFETY
		DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
		return false;
	}

	uint8_t *old_ptr = (uint8_t *)(*ptr);

	/* Copy min(size_of_old_block, size) bytes to avoid buffer overflow */
	uint32_t copy_size = (size_of_old_block < size) ? size_of_old_block : size;
	memmove(new_ptr, old_ptr, copy_size);

	dfree_internal(heap_struct_ptr, ptr, USING_PTR_ADDRESS);
	replace_pointers_internal(heap_struct_ptr, (void**)&new_ptr, ptr);

#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
	return true;
}

/* Internal print - no locking, called with mutex held */
static void print_dalloc_info_internal(heap_t* heap_struct_ptr){
	(void)heap_struct_ptr; /* Suppress warning when dalloc_debug is disabled */
	dalloc_debug("\n***************************** Mem Info *****************************\n");
	dalloc_debug("Total memory, bytes: %lu\n", (long unsigned int)heap_struct_ptr->total_size);
	dalloc_debug("Memory in use, bytes: %lu\n", (long unsigned int)heap_struct_ptr->offset);
	dalloc_debug("Number of allocations: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.allocations_num);
	dalloc_debug("The biggest memory was in use: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.max_memory_amount);
	dalloc_debug("Max allocations number: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.max_allocations_amount);
	dalloc_debug("\n********************************************************************\n\n");
}

void print_dalloc_info(heap_t* heap_struct_ptr){
	if(!heap_struct_ptr){
		dalloc_debug("print_dalloc_info: heap_struct_ptr is NULL\n");
		return;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif
	print_dalloc_info_internal(heap_struct_ptr);
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}

int dalloc_heap_info_str(heap_t* heap_struct_ptr, char *buf, uint32_t buf_size){
	if(!heap_struct_ptr || !buf || buf_size == 0){
		if(buf && buf_size > 0) buf[0] = '\0';
		return -1;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif
	int n = dalloc_snprintf(buf, buf_size,
		"used=%lu/%lu allocs=%lu peak_mem=%lu peak_allocs=%lu",
		(unsigned long)heap_struct_ptr->offset,
		(unsigned long)heap_struct_ptr->total_size,
		(unsigned long)heap_struct_ptr->alloc_info.allocations_num,
		(unsigned long)heap_struct_ptr->alloc_info.max_memory_amount,
		(unsigned long)heap_struct_ptr->alloc_info.max_allocations_amount);
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
	buf[buf_size - 1] = '\0';
	return n;
}

void dump_heap(heap_t* heap_struct_ptr){
	if(!heap_struct_ptr){
		dalloc_debug("dump_heap: heap_struct_ptr is NULL\n");
		return;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif
	for(uint32_t i = 0; i < heap_struct_ptr->total_size; i++){
		dalloc_debug("%02X ", heap_struct_ptr->mem[i]);
	}
	dalloc_debug("\n");
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}

void dump_dalloc_ptr_info(heap_t* heap_struct_ptr){
	if(!heap_struct_ptr){
		dalloc_debug("dump_dalloc_ptr_info: heap_struct_ptr is NULL\n");
		return;
	}
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_LOCK(heap_struct_ptr->mutex);
#endif
	dalloc_debug("\n***************************** Ptr Info *****************************\n");
	for(uint32_t i = 0; i < heap_struct_ptr->alloc_info.allocations_num; i++){
		dalloc_debug("Ptr address: 0x%lx, ptr first val: 0x%02X, alloc size: %lu\n",
			(unsigned long)(size_t)(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr),
			(uint8_t)(**heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr),
			(unsigned long)heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size);
	}
	dalloc_debug("\n********************************************************************\n\n");
#ifdef USE_THREAD_SAFETY
	DALLOC_MUTEX_UNLOCK(heap_struct_ptr->mutex);
#endif
}
