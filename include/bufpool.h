#pragma once
#include <stddef.h>

typedef struct bufpool bufpool_t;

bufpool_t* pool_create(size_t buf_size, int count);
void*      pool_acquire(bufpool_t* p);
void       pool_release(bufpool_t* p, void* buf);
void       pool_destroy(bufpool_t* p);

