#ifndef WARPC_THREADPOOL_H
#define WARPC_THREADPOOL_H

#include <pthread.h>
#include <stddef.h>

struct job {
  void (*fn)(void*);
  void* arg;
  struct job* next;
};

struct threadpool {
  pthread_t*    th;
  size_t        nth;
  struct job*   head;
  struct job*   tail;
  pthread_mutex_t mtx;
  pthread_cond_t  cv;
  int           stop;
};

struct threadpool* tp_create(size_t nthreads);
void               tp_destroy(struct threadpool* tp);
/* Returns 0 on success, -1 on OOM */
int                tp_submit(struct threadpool* tp, void (*fn)(void*), void* arg);

#endif
