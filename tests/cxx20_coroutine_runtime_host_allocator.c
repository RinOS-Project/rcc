/* SPDX-License-Identifier: Apache-2.0 */

#define _POSIX_C_SOURCE 200112L
#include <stddef.h>
#include <stdlib.h>

/* The RinOS allocator entry points delegate to the host allocator so the
 * unmodified RinCRT exception implementation can run in this Linux test. */
extern void* __libc_malloc(size_t size);
extern void __libc_free(void* pointer);
extern void* __libc_realloc(void* pointer, size_t size);

void* rin_user_allocator_malloc(size_t size)
{
    return __libc_malloc(size);
}

void rin_user_allocator_free(void* pointer)
{
    __libc_free(pointer);
}

void* rin_user_allocator_realloc(void* pointer, size_t size)
{
    return __libc_realloc(pointer, size);
}

void* rin_malloc(size_t size)
{
    return __libc_malloc(size);
}

void rin_free(void* pointer)
{
    __libc_free(pointer);
}

void* rin_aligned_alloc(size_t size, size_t alignment)
{
    void* pointer = NULL;
    return posix_memalign(&pointer, alignment, size) == 0 ? pointer : NULL;
}


void rincrt_test_process_exit(int status)
{
    _Exit(status);
}
