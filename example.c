/*
 * example.cpp
 *
 *  Created on: Sep 24, 2021
 *      Author: SkyEngineering
 */

#include "dalloc.h"

#define HEAP_SIZE			32

uint8_t heap_array[HEAP_SIZE];
heap_t heap;

int main(){
	/* Init heap memory */
	heap_init(&heap, (void*)heap_array, HEAP_SIZE);

	/* Allocate memory region in heap memory */
	uint8_t *first_data_ptr = NULL;
	uint32_t first_allocation_size = 8;
	dalloc(&heap, first_allocation_size, &first_data_ptr);

	/* Check if allocation is successful */
	if(first_data_ptr == NULL){
		printf("Can't allocate %u bytes\n", first_allocation_size);
		return 0;
	}

	printf("Memory allocated successfully, allocated %u bytes\n", first_allocation_size);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Fill first allocated region by test values */
	printf("Fill first allocated region by test values\n");
	memset(first_data_ptr, 0x1, first_allocation_size);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Reallocate first memory region */
	printf("Reallocate first memory region\n");
	uint32_t new_size_for_first_allocation = 11;
	if(drealloc(&heap, new_size_for_first_allocation, &first_data_ptr) != true){
		printf("Memory reallocation error, can't allocate %u bytes\n", new_size_for_first_allocation);
	}

	printf("Memory reallocated successfully, allocated %u bytes\n", new_size_for_first_allocation);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Allocate second memory region in heap memory */
	uint8_t *second_data_ptr = NULL;
	uint32_t second_allocation_size = 5;
	dalloc(&heap, second_allocation_size, &second_data_ptr);

	/* Check if allocation is successful */
	if(second_data_ptr == NULL){
		printf("Can't allocate %u bytes\n", second_allocation_size);
		return 0;
	}

	printf("Memory allocated successfully, allocated %u bytes\n", second_allocation_size);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Fill second allocated region by test values */
	printf("Fill second allocated region by test values\n");
	memset(second_data_ptr, 0x2, second_allocation_size);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Free first memory region */
	printf("Free first memory region\n");
	dfree(&heap, &first_data_ptr, USING_PTR_ADDRESS);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	/* Free second memory region */
	printf("Free second memory region\n");
	dfree(&heap, &second_data_ptr, USING_PTR_ADDRESS);

	/* Print info about all allocations in given heap memory */
	print_dalloc_info(&heap);
	dump_heap(&heap);
	dump_dalloc_ptr_info(&heap);

	return 0;
}
