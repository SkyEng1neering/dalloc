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

#ifndef DALLOCCONF_H
#define DALLOCCONF_H

#ifdef __cplusplus
	extern "C" {
#endif

#define DALLOC_VERSION                      "1.3.0"

#define FILL_FREED_MEMORY_BY_NULLS          true
#define dalloc_debug                        printf
#define MAX_NUM_OF_ALLOCATIONS              100UL

/* Comment "USE_ALIGNMENT" define if you don't need to use alignment */
#define USE_ALIGNMENT

#ifdef USE_ALIGNMENT
#define ALLOCATION_ALIGNMENT_BYTES          4U
#endif

/* Uncomment "USE_SINGLE_HEAP_MEMORY" define if you want to use only 1 heap memory area */
//#define USE_SINGLE_HEAP_MEMORY

#ifdef USE_SINGLE_HEAP_MEMORY
#define SINGLE_HEAP_SIZE										4096UL
#endif

#ifdef __cplusplus
	}
#endif

#endif // DALLOCCONF_H
