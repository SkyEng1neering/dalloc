# About the project
This is the custom implementation of function malloc for embedded systems, that defragmentate memory after using it. Good solution when you need to allocate memory dynamically, but memory fragmentation is the problem. On this allocator based such project as [uvector](https://github.com/SkyEng1neering/uvector), [ustring](https://github.com/SkyEng1neering/ustring) and [usmartpointer](https://github.com/SkyEng1neering/usmartpointer).

## What is the problem that [dalloc](https://github.com/SkyEng1neering/dalloc) solves?
### Here is an illustration of memory fragmentation problem, while using default malloc function:    
![this](https://github.com/SkyEng1neering/files/blob/main/default_malloc.drawio%20(1).png)
On the last step there are 7 kB of free memory but not in solid region, so allocating of 4 kB fails
<br></br>

### And how dalloc works:
![this](https://github.com/SkyEng1neering/files/blob/main/defrag_malloc.drawio.png)

## How it works
When you call allocate function, it saves the pointer to the pointer of allocated memory area and when corresponding memory region is freed - the defragmentation procedure executes, an the content of pointer to the memory area is replaced by new actual value. So using of dalloc allocator is not the same as default malloc.
