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
/* define single_heap array somewhere in your code, like on the example below:
              uint8_t single_heap[SINGLE_HEAP_SIZE] = {0};
*/
heap_t default_heap;
bool memory_init_flag = false;

void def_dalloc(uint32_t size, void **ptr){
	dalloc(&default_heap, size, ptr);
}

void def_dfree(void **ptr){
	dfree(&default_heap, ptr, USING_PTR_ADDRESS);
}

void def_replace_pointers(void **ptr_to_replace, void **ptr_new){
	replace_pointers(&default_heap, ptr_to_replace, ptr_new);
}

bool def_drealloc(uint32_t size, void **ptr){
	return drealloc(&default_heap, size, ptr);
}

void print_def_dalloc_info(){
	print_dalloc_info(&default_heap);
}

void dump_def_heap(){
	dump_heap(&default_heap);
}

void dump_def_dalloc_ptr_info(){
	dump_dalloc_ptr_info(&default_heap);
}
#endif

void heap_init(heap_t* heap_struct_ptr, void *mem_ptr, uint32_t mem_size){//Init here mem structures
	heap_struct_ptr->offset = 0;
	heap_struct_ptr->mem = (uint8_t*)mem_ptr;
	heap_struct_ptr->total_size = mem_size;
	heap_struct_ptr->alloc_info.allocations_num = 0;
	heap_struct_ptr->alloc_info.max_memory_amount = 0;
    for(uint32_t i = 0; i < MAX_NUM_OF_ALLOCATIONS; i++){
        heap_struct_ptr->alloc_info.ptr_info_arr[i].ptr = NULL;
        heap_struct_ptr->alloc_info.ptr_info_arr[i].allocated_size = 0;
        heap_struct_ptr->alloc_info.ptr_info_arr[i].free_flag = true;
    }
	for(uint32_t i = 0; i < heap_struct_ptr->total_size; i++){
		heap_struct_ptr->mem[i] = 0;
	}
}

void dalloc(heap_t* heap_struct_ptr, uint32_t size, void **ptr){
#ifdef USE_SINGLE_HEAP_MEMORY
	if(memory_init_flag == false){
		heap_init(&default_heap, single_heap, SINGLE_HEAP_SIZE);
		memory_init_flag = true;
	}
#endif

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

bool is_ptr_address_in_heap_area(heap_t* heap_struct_ptr, void **ptr){
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
            while(alloc_size % ALLOCATION_ALIGNMENT_BYTES != 0){
                alloc_size += 1;
            }
#endif
            /* Check if ptrs adresses of defragmentated memory are in heap region */
            for(uint32_t k = i + 1; k < heap_struct_ptr->alloc_info.allocations_num; k++){
                if(is_ptr_address_in_heap_area(heap_struct_ptr, (void**)heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr)){
                    if((size_t)heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr > (size_t)(start_mem_ptr)){
                        heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr = (uint8_t**)((size_t)(heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr) - alloc_size);
                    }
                }
            }

            /* Defragmentate memory */
            uint32_t stop_ind = heap_struct_ptr->offset - alloc_size;
            for(uint32_t k = start_ind; k <= stop_ind; k++){
                *(heap_struct_ptr->mem + k) = *(heap_struct_ptr->mem + k + alloc_size);
            }

            /* Reassign pointers */
            for(uint32_t k = i + 1; k < heap_struct_ptr->alloc_info.allocations_num; k++){
                *(heap_struct_ptr->alloc_info.ptr_info_arr[k].ptr) -= alloc_size;
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
            if(FILL_FREED_MEMORY_BY_NULLS){
                for(uint32_t k = 0; k < alloc_size; k++){
                    heap_struct_ptr->mem[heap_struct_ptr->offset + k] = 0;
                }
            }
        }
    }
}

void dfree(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition){
    /* Check if heap_ptr is not assigned */
    if(heap_struct_ptr == NULL){
        dalloc_debug("Heap pointer is not assigned\n");
        return;
    }

    uint32_t ptr_index = 0;

    /* Try to find given ptr in ptr_info array */
    if(validate_ptr(heap_struct_ptr, ptr, condition, &ptr_index) != true){
        dalloc_debug("Try to free unexisting pointer\n");
        return;
    }

    uint32_t alloc_size = heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].allocated_size;
#ifdef USE_ALIGNMENT
    while(alloc_size % ALLOCATION_ALIGNMENT_BYTES != 0){
        alloc_size += 1;
    }
#endif

    /* Edit ptr info array */
    heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].free_flag = true;
#ifdef FILL_FREED_MEMORY_BY_NULLS
    for(uint32_t i = 0; i < alloc_size; i++){
        *(*(heap_struct_ptr->alloc_info.ptr_info_arr[ptr_index].ptr) + i) = 0;
    }
#endif

//    /* Set given ptr to NULL */
//    if(condition == USING_PTR_ADDRESS){
//        *ptr = NULL;
//    }

    defrag_memory(heap_struct_ptr);
}

void _dfree(heap_t* heap_struct_ptr, void **ptr, validate_ptr_condition_t condition){
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
