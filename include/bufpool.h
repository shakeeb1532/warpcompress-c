#ifndef BUFPOOL_H
#define BUFPOOL_H

typedef struct bufpool bufpool_t;

bufpool_t* pool_create(size_t buf_size, int count);
void* pool_acquire(bufpool_t* pool);
void  pool_release(bufpool_t* pool, void* buf);
void  pool_destroy(bufpool_t* pool);

#endif
