#include "warpc/bufpool.h"
#include <stdlib.h>

struct bufpool* pool_create(size_t count, size_t bufsz) {
  struct bufpool* pool = (struct bufpool*)malloc(sizeof(*pool));
  if (!pool) return NULL;
  pool->count = count;
  pool->bufsz = bufsz;
  pool->bufs = (void**)calloc(count, sizeof(void*));
  if (!pool->bufs) { free(pool); return NULL; }
  for (size_t i = 0; i < count; ++i) {
    pool->bufs[i] = malloc(bufsz);
    if (!pool->bufs[i]) {
      for (size_t k = 0; k < i; ++k) free(pool->bufs[k]);
      free(pool->bufs);
      free(pool);
      return NULL;
    }
  }
  pool->head = count; /* all free */
  pthread_mutex_init(&pool->mtx, NULL);
  return pool;
}

void pool_destroy(struct bufpool* p) {
  if (!p) return;
  for (size_t i = 0; i < p->count; ++i) free(p->bufs[i]);
  free(p->bufs);
  pthread_mutex_destroy(&p->mtx);
  free(p);
}

void* pool_acquire(struct bufpool* p) {
  pthread_mutex_lock(&p->mtx);
  void* out = NULL;
  if (p->head > 0) {
    out = p->bufs[--p->head];
    p->bufs[p->head] = NULL;
  }
  pthread_mutex_unlock(&p->mtx);
  return out; /* may be NULL if pool exhausted */
}

void pool_release(struct bufpool* p, void* buf) {
  if (!buf) return;
  pthread_mutex_lock(&p->mtx);
  if (p->head < p->count) p->bufs[p->head++] = buf; /* drop if full */
  pthread_mutex_unlock(&p->mtx);
}
