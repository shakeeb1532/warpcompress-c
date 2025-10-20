#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <stddef.h>
typedef void (*tp_fn)(void *arg);
typedef struct tp tp_t;
tp_t* tp_create(int threads);
void  tp_submit(tp_t *tp, tp_fn fn, void *arg);
void  tp_barrier(tp_t *tp);
void  tp_destroy(tp_t *tp);
#endif
