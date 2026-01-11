# dalloc - Defragmenting Memory Allocator

Version: 1.5.0

A custom memory allocator for embedded systems with automatic defragmentation. Unlike standard malloc, dalloc tracks pointer addresses and can move allocations during defragmentation, updating all affected pointers automatically.

Libraries built on dalloc: [uvector](https://github.com/SkyEng1neering/uvector), [ustring](https://github.com/SkyEng1neering/ustring), [usmartpointer](https://github.com/SkyEng1neering/usmartpointer).

## The Problem dalloc Solves

### Standard malloc - fragmentation issue:
![malloc fragmentation](https://github.com/SkyEng1neering/files/blob/main/default_malloc.drawio%20(1).png)

On the last step there are 7 kB free but not contiguous, so allocating 4 kB fails.

### dalloc - automatic defragmentation:
![dalloc defragmentation](https://github.com/SkyEng1neering/files/blob/main/defrag_malloc.drawio.png)

## How It Works

dalloc saves the **address of the pointer** (not just its value). When memory is freed, defragmentation runs automatically and updates all affected pointers to their new locations.

**Important:** This means the pointer variable must remain valid until `dfree()` is called.

## Quick Start

### Option 1: Single Global Heap (Recommended for simple projects)

```c
#include "dalloc.h"

// Place buffer in specific memory section if needed
__attribute__((aligned(4)))  // Ensure 4-byte alignment!
static uint8_t heap_buffer[4096];

int main() {
    // Register the heap buffer (must be called before any allocation)
    if (!dalloc_register_heap(heap_buffer, sizeof(heap_buffer))) {
        return -1;  // Registration failed
    }

    // Allocate memory
    uint8_t *data = NULL;
    def_dalloc(64, (void**)&data);

    if (data == NULL) {
        return -1;  // Allocation failed
    }

    // Use allocated memory
    for (int i = 0; i < 64; i++) {
        data[i] = i;
    }

    // Free memory (data becomes NULL)
    def_dfree((void**)&data);

    return 0;
}
```

### Option 2: Multiple Heaps (Explicit API)

```c
#include "dalloc.h"

__attribute__((aligned(4)))
static uint8_t buffer1[2048];
static uint8_t buffer2[2048];

heap_t heap_fast;  // For frequently accessed data
heap_t heap_slow;  // For less critical data

int main() {
    heap_init(&heap_fast, buffer1, sizeof(buffer1));
    heap_init(&heap_slow, buffer2, sizeof(buffer2));

    // Allocate from specific heap
    uint8_t *critical_data = NULL;
    dalloc(&heap_fast, 256, (void**)&critical_data);

    uint8_t *temp_data = NULL;
    dalloc(&heap_slow, 512, (void**)&temp_data);

    // Use memory...

    // Free when done
    dfree(&heap_fast, (void**)&critical_data, USING_PTR_ADDRESS);
    dfree(&heap_slow, (void**)&temp_data, USING_PTR_ADDRESS);

    // Cleanup heaps
    heap_deinit(&heap_fast);
    heap_deinit(&heap_slow);

    return 0;
}
```

## Examples

### Reallocation

```c
uint8_t *buffer = NULL;
def_dalloc(32, (void**)&buffer);

// Fill with data
for (int i = 0; i < 32; i++) {
    buffer[i] = i;
}

// Grow the buffer - data is preserved
if (def_drealloc(64, (void**)&buffer)) {
    // buffer now has 64 bytes, first 32 contain original data
    buffer[32] = 0xFF;  // Use new space
}

def_dfree((void**)&buffer);
```

### Transferring Pointer Ownership

When you need to move an allocation to a different pointer variable:

```c
uint8_t *temp_ptr = NULL;
def_dalloc(64, (void**)&temp_ptr);

// Work with temp_ptr...
temp_ptr[0] = 0x42;

// Transfer to permanent storage
uint8_t *permanent_ptr = NULL;
def_replace_pointers((void**)&temp_ptr, (void**)&permanent_ptr);
// Now: temp_ptr == NULL, permanent_ptr points to the memory

// Later...
def_dfree((void**)&permanent_ptr);
```

### Bulk Reset (Clear All Allocations)

```c
// Array of pointers - must persist until free/reset
uint8_t *objects[100];

// Allocate many objects
for (int i = 0; i < 100; i++) {
    objects[i] = NULL;
    def_dalloc(16, (void**)&objects[i]);
    if (objects[i]) {
        objects[i][0] = i;  // Use allocated memory
    }
}

// Instead of freeing each one, reset the entire heap
dalloc_reset_heap();
// All allocations are cleared, heap is empty
// WARNING: All pointers in objects[] are now invalid!
```

### Checking Heap State

```c
if (!dalloc_is_initialized()) {
    dalloc_register_heap(buffer, sizeof(buffer));
}

heap_t *heap = dalloc_get_default_heap();
if (heap != NULL) {
    printf("Used: %lu / %lu bytes\n",
           (unsigned long)heap->offset,
           (unsigned long)heap->total_size);
    printf("Allocations: %lu\n",
           (unsigned long)heap->alloc_info.allocations_num);
}
```

### ESP32/STM32: Using Specific Memory Regions

```c
// ESP32: Use IRAM for fast access
__attribute__((section(".iram1")))
static uint8_t fast_heap[4096];

// STM32: Use CCM RAM
__attribute__((section(".ccmram")))
static uint8_t ccm_heap[8192];

// External SRAM (if available)
__attribute__((section(".sdram")))
static uint8_t large_heap[65536];

void init_heaps(void) {
    dalloc_register_heap(fast_heap, sizeof(fast_heap));
}
```

## API Reference

### Single Heap API (USE_SINGLE_HEAP_MEMORY)

| Function | Description |
|----------|-------------|
| `dalloc_register_heap(buffer, size)` | Register user buffer as default heap |
| `dalloc_unregister_heap(force)` | Unregister heap. If `force=false`, fails when allocations exist |
| `dalloc_reset_heap()` | Clear all allocations, reinitialize heap |
| `dalloc_is_initialized()` | Check if heap is registered |
| `dalloc_get_default_heap()` | Get pointer to heap_t structure |
| `def_dalloc(size, &ptr)` | Allocate memory |
| `def_dfree(&ptr)` | Free memory |
| `def_drealloc(size, &ptr)` | Reallocate memory |
| `def_replace_pointers(&old, &new)` | Transfer allocation to new pointer |

### Multi-Heap API

| Function | Description |
|----------|-------------|
| `heap_init(heap, buffer, size)` | Initialize heap with buffer |
| `heap_deinit(heap)` | Deinitialize heap, release resources |
| `dalloc(heap, size, &ptr)` | Allocate from specific heap |
| `dfree(heap, &ptr, condition)` | Free from specific heap |
| `drealloc(heap, size, &ptr)` | Reallocate in specific heap |
| `replace_pointers(heap, &old, &new)` | Transfer allocation |
| `validate_ptr(heap, &ptr, condition, &index)` | Check if pointer is valid |

### Debug Functions

| Function | Description |
|----------|-------------|
| `print_dalloc_info(heap)` / `print_def_dalloc_info()` | Print heap statistics |
| `dump_heap(heap)` / `dump_def_heap()` | Hex dump of heap memory |
| `dump_dalloc_ptr_info(heap)` / `dump_def_dalloc_ptr_info()` | Print allocation details |

## Configuration

You can override default settings without modifying `dalloc_conf.h`:

### Method 1: Compiler flags
```bash
gcc -DMAX_NUM_OF_ALLOCATIONS=200 -DUSE_SINGLE_HEAP_MEMORY ...
```

### Method 2: Custom config file
```bash
gcc -DDALLOC_CUSTOM_CONF_FILE=\"my_dalloc_conf.h\" ...
```

```c
// my_dalloc_conf.h - only override what you need
#define MAX_NUM_OF_ALLOCATIONS      50
#define USE_SINGLE_HEAP_MEMORY
#define dalloc_debug(...)           // disable debug output
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `MAX_NUM_OF_ALLOCATIONS` | 100 | Maximum simultaneous allocations |
| `USE_ALIGNMENT` | defined | Enable pointer alignment |
| `ALLOCATION_ALIGNMENT_BYTES` | 4 | Alignment boundary (typically 4 for ARM) |
| `FILL_FREED_MEMORY_BY_NULLS` | true | Zero memory on free (security) |
| `USE_SINGLE_HEAP_MEMORY` | undefined | Enable single heap mode |
| `USE_THREAD_SAFETY` | undefined | Enable thread-safe operations |
| `USE_FREE_RTOS` | undefined | Use FreeRTOS mutex primitives |
| `dalloc_debug` | printf | Debug output function |

## Thread Safety

dalloc supports optional thread safety via `USE_THREAD_SAFETY`. Each heap has its own mutex for protection.

### FreeRTOS (Easiest)

```c
// my_dalloc_conf.h
#define USE_THREAD_SAFETY
#define USE_FREE_RTOS
```

dalloc will automatically use `xSemaphoreCreateMutex()`, `xSemaphoreTake()`, `xSemaphoreGive()`.

### POSIX Threads

```c
// my_dalloc_conf.h
#define USE_THREAD_SAFETY

#include <pthread.h>
#include <stdlib.h>

#define DALLOC_MUTEX_TYPE           pthread_mutex_t*

#define DALLOC_MUTEX_CREATE()       ({ \
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t)); \
    if (m) pthread_mutex_init(m, NULL); \
    m; \
})

#define DALLOC_MUTEX_DELETE(mutex)  do { \
    if (mutex) { pthread_mutex_destroy(mutex); free(mutex); } \
} while(0)

#define DALLOC_MUTEX_LOCK(mutex)    do { if (mutex) pthread_mutex_lock(mutex); } while(0)
#define DALLOC_MUTEX_UNLOCK(mutex)  do { if (mutex) pthread_mutex_unlock(mutex); } while(0)
```

### Required Mutex Macros

| Macro | Description |
|-------|-------------|
| `DALLOC_MUTEX_TYPE` | The mutex handle type |
| `DALLOC_MUTEX_CREATE()` | Create and return a mutex handle |
| `DALLOC_MUTEX_DELETE(mutex)` | Delete/destroy the mutex |
| `DALLOC_MUTEX_LOCK(mutex)` | Acquire the mutex (blocking) |
| `DALLOC_MUTEX_UNLOCK(mutex)` | Release the mutex |

## Important Limitations

### Buffer Alignment

The heap buffer **must be aligned** to `ALLOCATION_ALIGNMENT_BYTES` (default: 4 bytes):

```c
// CORRECT - explicitly aligned
__attribute__((aligned(4)))
static uint8_t heap_buffer[4096];

// ALSO OK - naturally aligned on most systems
static uint32_t heap_buffer_u32[1024];  // 4096 bytes, 4-byte aligned
static uint8_t *heap_buffer = (uint8_t*)heap_buffer_u32;
```

### Pointer Lifetime

The pointer **variable** passed to `dalloc()` must remain in scope until `dfree()` is called (dalloc stores the address of the variable, not just its value):

```c
// WRONG - data_ptr is destroyed when function returns
uint8_t* bad_function(heap_t *heap) {
    uint8_t* data_ptr = NULL;
    dalloc(heap, 64, (void**)&data_ptr);
    return data_ptr;  // BUG: &data_ptr becomes invalid!
}

// CORRECT - use pointer from caller's scope
bool good_function(heap_t *heap, uint8_t **data_ptr) {
    dalloc(heap, 64, (void**)data_ptr);
    return (*data_ptr != NULL);
}
```

### Memory Overhead

Each allocation uses:
- Actual data (aligned to `ALLOCATION_ALIGNMENT_BYTES`)
- Entry in `ptr_info_arr` array (12 bytes per allocation on 32-bit systems)

For many small allocations, consider using a dedicated pool allocator instead.

## Building Tests

```bash
cd tests
mkdir build && cd build
cmake ..
make -j$(nproc)
./dalloc_tests           # 85 tests
./dalloc_thread_tests    # 7 thread safety tests (Linux only)
```

## License

Apache License 2.0 - See [LICENSE](LICENSE) file.
