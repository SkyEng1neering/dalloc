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

void heap_init(heap_t* heap_struct_ptr, void *mem_ptr, uint32_t mem_size){//Init here mem structures
	heap_struct_ptr->offset = 0;
	heap_struct_ptr->mem = (uint8_t*)mem_ptr;
	heap_struct_ptr->total_size = mem_size;
	heap_struct_ptr->alloc_info.allocations_num = 0;
	heap_struct_ptr->alloc_info.max_memory_amount = 0;
    for(uint32_t i = 0; i < heap_struct_ptr->total_size; i++){
    	heap_struct_ptr->mem[i] = 0;
    }
}

void dalloc(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
    if(!heap_struct_ptr || !size){
        *ptr = NULL;
    }

    uint32_t new_offset = heap_struct_ptr->offset + size;

    /* Correct offset if use alignment */
#ifdef USE_ALIGNMENT
    while(new_offset % ALLOCATION_ALIGNMENT_BYTES != 0){
    	new_offset += 1;
    }
#endif

    /* Check if there is enough memory for new allocation, and if number of allocations is exceeded */
    if((new_offset <= heap_struct_ptr->total_size) && (heap_struct_ptr->alloc_info.allocations_num < MAX_NUM_OF_ALLOCATIONS)) {
        *ptr = heap_struct_ptr->mem + heap_struct_ptr->offset;
        heap_struct_ptr->offset = new_offset;

        /* Save info about allocated memory */
        heap_struct_ptr->alloc_info.ptr_info_arr[heap_struct_ptr->alloc_info.allocations_num].ptr = (uint8_t**)ptr;
        heap_struct_ptr->alloc_info.ptr_info_arr[heap_struct_ptr->alloc_info.allocations_num].allocated_size = size;
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
    	print_dalloc_info(heap_struct_ptr);
        *ptr = NULL;
        if(new_offset > heap_struct_ptr->total_size){
        	dalloc_debug("dalloc: Heap size exceeded\n");
        }
        if(heap_struct_ptr->alloc_info.allocations_num > MAX_NUM_OF_ALLOCATIONS){
        	dalloc_debug("dalloc: Max number of allocations exceeded: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.allocations_num);
        }
    }
}

bool validate_ptr(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition, uint32_t *ptr_index){
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

void dfree(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition){
	uint32_t ptr_index = 0;

	/* Try to find given ptr in ptr_info array */
	if(validate_ptr(heap_struct_ptr, ptr, condition, &ptr_index) == true){

		/* Optimize memory, to escape fragmentation. */
        uint32_t start_ind = (uint32_t)((uint8_t*)*ptr - heap_struct_ptr->mem);
        uint32_t alloc_size = heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].allocated_size;
#ifdef USE_ALIGNMENT
        while(alloc_size % ALLOCATION_ALIGNMENT_BYTES != 0){
            alloc_size += 1;
        }
#endif

        uint32_t stop_ind = heap_struct_ptr->offset - alloc_size;
        for(uint32_t i = start_ind; i <= stop_ind; i++){
        	heap_struct_ptr->mem[i] = heap_struct_ptr->mem[i + alloc_size];
        }

        /* Remove ptr from ptr_info array */
		for(uint32_t i = ptr_index; i < heap_struct_ptr->alloc_info.allocations_num - 1; i++){
			heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr = heap_struct_ptr->alloc_info.ptr_info_arr[i + 1].ptr;
			heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size = heap_struct_ptr->alloc_info.ptr_info_arr[i + 1].allocated_size;
			/* Reassign pointers */
			*(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr) -= alloc_size;
		}
		heap_struct_ptr->alloc_info.allocations_num--;

        /* Refresh offset */
		heap_struct_ptr->offset = heap_struct_ptr->offset - alloc_size;

		/* Fill by 0 all freed memory */
		if(FILL_FREED_MEMORY_BY_NULLS){
			for(uint32_t i = 0; i < heap_struct_ptr->total_size - heap_struct_ptr->offset; i++){
				heap_struct_ptr->mem[heap_struct_ptr->offset + i] = 0;
			}
		}

        /* Set given ptr to NULL */
        if(condition == USING_PTR_ADDRESS){
        	*ptr = NULL;
        }
    }
    else {
    	dalloc_debug("Try to free unexisting pointer\n");
    }
}

void replace_pointers(heap_t* heap_struct_ptr, void **ptr_to_replace, void **ptr_new){
    uint32_t ptr_ind = 0;
    if(validate_ptr(heap_struct_ptr, ptr_to_replace, USING_PTR_ADDRESS, &ptr_ind) != true){
        dalloc_debug("Can't replace pointers. No pointer found in buffer\n");
        return;
    }
    *ptr_new = *ptr_to_replace;
    heap_struct_ptr->alloc_info.ptr_info_arr[ptr_ind].ptr = (uint8_t**)ptr_new;
    *ptr_to_replace = NULL;
}

bool drealloc(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
	uint32_t size_of_old_block = 0;
	uint32_t old_ptr_ind = 0;
	if(validate_ptr(heap_struct_ptr, ptr, USING_PTR_ADDRESS, &old_ptr_ind) == true){
		size_of_old_block = heap_struct_ptr->alloc_info.ptr_info_arr[old_ptr_ind].allocated_size;
	}
	else {
		return false;
	}

	uint8_t *new_ptr = NULL;
	dalloc(heap_struct_ptr, size, (void**)&new_ptr);

	uint8_t *old_ptr = (uint8_t *)(*ptr);

	for(uint32_t i = 0; i < size_of_old_block; i++){
		new_ptr[i] = old_ptr[i];
	}
	dfree(heap_struct_ptr, ptr, USING_PTR_ADDRESS);
	replace_pointers(heap_struct_ptr, (void**)&new_ptr, ptr);
	return true;
}

void print_dalloc_info(heap_t* heap_struct_ptr){
	dalloc_debug("\n***************************** Mem Info *****************************\n");
	dalloc_debug("Total memory, bytes: %lu\n", (long unsigned int)heap_struct_ptr->total_size);
	dalloc_debug("Memory in use, bytes: %lu\n", (long unsigned int)heap_struct_ptr->offset);
	dalloc_debug("Number of allocations: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.allocations_num);
	dalloc_debug("The biggest memory was in use: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.max_memory_amount);
	dalloc_debug("Max allocations number: %lu\n", (long unsigned int)heap_struct_ptr->alloc_info.max_allocations_amount);
	dalloc_debug("\n********************************************************************\n\n");
}

void dump_heap(heap_t* heap_struct_ptr){
	for(uint32_t i = 0; i < heap_struct_ptr->total_size; i++){
		dalloc_debug("%02X ", heap_struct_ptr->mem[i]);
	}
	dalloc_debug("\n");
}

void dump_dalloc_ptr_info(heap_t* heap_struct_ptr){
	dalloc_debug("\n***************************** Ptr Info *****************************\n");
	for(uint32_t i = 0; i < heap_struct_ptr->alloc_info.allocations_num; i++){
		dalloc_debug("Ptr address: 0x%08lX, ptr first val: 0x%02X, alloc size: %lu\n", (size_t)(heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr), (uint8_t)(**heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr), (long unsigned int)heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size);
	}
	dalloc_debug("\n********************************************************************\n\n");
}
