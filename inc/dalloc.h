#ifndef DALLOC_H
#define DALLOC_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "dalloc_types.h"

void heap_init(heap_t *heap_struct_ptr, void *mem_ptr, uint32_t mem_size);
void dalloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);
bool drealloc(heap_t *heap_struct_ptr, uint32_t size, void **ptr);
bool validate_ptr(heap_t *heap_struct_ptr, void **ptr, validate_ptr_condition_t condition, uint32_t *ptr_index);
void dfree(heap_t *heap_struct_ptr, void **ptr, validate_ptr_condition_t condition);
void print_dalloc_info(heap_t *heap_struct_ptr);
void dump_dalloc_ptr_info(heap_t* heap_struct_ptr);
void dump_heap(heap_t* heap_struct_ptr);
void replace_pointers(heap_t *heap_struct_ptr, void **ptr_to_replace, void **ptr_new);

#ifdef __cplusplus
 }
#endif

#endif // DALLOC_H
