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

#ifndef DALLOC_H
#define DALLOC_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "dalloc_types.h"

#ifdef USE_SINGLE_HEAP_MEMORY
extern heap_t default_heap;

void def_dalloc(uint32_t size, void **ptr);
void def_dfree(void **ptr);
void def_replace_pointers(void **ptr_to_replace, void **ptr_new);
bool def_drealloc(uint32_t size, void **ptr);
void print_def_dalloc_info();
void dump_def_heap();
void dump_def_dalloc_ptr_info();
#endif


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
