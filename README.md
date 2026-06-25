# OS Memory Allocator

A small C memory allocator that reimplements the basic behavior of `malloc`, `calloc`, `realloc`, and `free`. The project focuses on manual virtual memory management on Linux, using low-level mechanisms such as `brk`, `sbrk`, `mmap`, and `munmap`.

The goal is not to replace a production allocator, but to understand how heap allocation works internally: how blocks are tracked, reused, split, merged, and returned to the operating system when needed.

## Features

The allocator exposes four main functions:

* `os_malloc(size)` - allocates an uninitialized memory block;
* `os_calloc(nmemb, size)` - allocates and zero-initializes an array;
* `os_realloc(ptr, size)` - resizes an existing allocation;
* `os_free(ptr)` - releases a previously allocated block.

Small allocations are handled through the heap, while larger blocks are mapped directly with `mmap`. This helps reduce unnecessary heap growth and keeps large allocations easier to release.

## Memory Blocks

Each allocation is managed through a metadata structure placed before the actual payload. This metadata stores the block size, its status, and links to the previous and next blocks.

The returned pointer always points to the usable payload, not to the metadata itself.

All blocks are aligned to 8 bytes, which matches the requirements of 64-bit systems and avoids problems caused by misaligned memory access.

## Allocation Strategy

The implementation tries to reuse memory before asking the operating system for more. Free blocks are searched using a best-fit approach, so the allocator chooses the smallest available block that can satisfy the request.

To reduce fragmentation, the allocator also supports:

* splitting larger free blocks when only part of the block is needed;
* coalescing adjacent free blocks into a single larger block;
* expanding the last heap block when possible;
* preallocating a larger heap area on the first small allocation.

`realloc` also tries to grow a block in place before moving it somewhere else, which helps avoid unnecessary copies.

## Project Structure

* `src/` - allocator implementation;
* `utils/` - shared headers and helper definitions;
* `osmem.h` - public interface for the allocator;
* `block_meta.h` - metadata structure used to manage blocks.

## Building

The allocator can be built from the `src` directory:

```bash
cd src
make
```

This produces the shared library used by programs that link against the custom allocator.

## Notes

The most important part of the project is keeping the internal block list consistent. Every allocation and free operation has to preserve correct links between blocks, respect alignment, and avoid losing track of reusable memory.

The implementation is intentionally simple, but it covers the core ideas behind real allocators: metadata, heap growth, mapped memory, fragmentation, and block reuse.
