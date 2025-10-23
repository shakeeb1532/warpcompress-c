#ifndef WARPC_BUFPOOL_H
#define WARPC_BUFPOOL_H

#include <pthread.h>
#include <stddef.h>

struct bufpool {
  void**         bufs;
  size_t         count;
  size_t         bufsz;
  pthread_mutex_t mtx;
  size_t         head; /* free-stack index */
};

struct bufpool* pool_create(size_t count, size_t bufsz);
void            pool_destroy(struct bufpool* p);
void*           pool_acquire(struct bufpool* p); /* may return NULL if empty */
void            pool_release(struct bufpool* p, void* buf);

#endif
