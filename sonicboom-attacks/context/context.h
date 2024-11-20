//#define OVERWRITE_MALLOC

#ifndef _NOUARCH_H_
#define _NOUARCH_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


#ifdef NOUARCH_DISABLE
#define free_nouarch free
#define malloc_nouarch malloc
#define calloc_nouarch calloc
#define realloc_nouarch realloc
#define recalloc_nouarch recalloc
#define reallocarray_nouarch reallocarray
#define recallocarray_nouarch recallocarray
#define nouarch
#else
#define nouarch __attribute__((section(".secret")))
void* malloc_nouarch(size_t len);
void* secure_malloc(size_t len);
void free_nouarch(void* addr);
void* calloc_nouarch(size_t nmemb, size_t size);
void* reallocarray_nouarch(void* addr, size_t nmemb, size_t size);
void* recallocarray_nouarch(void* addr, size_t onmemb, size_t nmemb, size_t size);
void* realloc_nouarch(void* addr, size_t size);
void* recalloc_nouarch(void* addr, size_t size);
void nouarch_set(void* addr, size_t len);

#ifdef OVERWRITE_MALLOC
#define malloc malloc_nouarch
#define free free_nouarch
#define calloc calloc_nouarch
#define realloc realloc_nouarch
#define recalloc recalloc_nouarch
#endif //OVERWRITE_MALLOC
#endif
void nouarch_secure_heap(int secure);

#endif
