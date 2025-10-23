#include "warpc/threadpool.h"
#include <stdlib.h>

static void* worker(void* p) {
  struct threadpool* tp = (struct threadpool*)p;
  for (;;) {
    pthread_mutex_lock(&tp->mtx);
    while (!tp->stop && tp->head == NULL) pthread_cond_wait(&tp->cv, &tp->mtx);
    if (tp->stop && tp->head == NULL) { pthread_mutex_unlock(&tp->mtx); break; }
    struct job* j = tp->head;
    tp->head = j->next;
    if (!tp->head) tp->tail = NULL;
    pthread_mutex_unlock(&tp->mtx);
    j->fn(j->arg);
    free(j);
  }
  return NULL;
}

struct threadpool* tp_create(size_t nthreads) {
  if (nthreads == 0) nthreads = 1;
  struct threadpool* tp = (struct threadpool*)calloc(1, sizeof(*tp));
  if (!tp) return NULL;
  tp->nth = nthreads;
  tp->th = (pthread_t*)calloc(nthreads, sizeof(pthread_t));
  if (!tp->th) { free(tp); return NULL; }
  pthread_mutex_init(&tp->mtx, NULL);
  pthread_cond_init(&tp->cv, NULL);
  for (size_t i = 0; i < nthreads; ++i) {
    if (pthread_create(&tp->th[i], NULL, worker, tp) != 0) { /* best effort */ }
  }
  return tp;
}

void tp_destroy(struct threadpool* tp) {
  if (!tp) return;
  pthread_mutex_lock(&tp->mtx);
  tp->stop = 1;
  pthread_cond_broadcast(&tp->cv);
  pthread_mutex_unlock(&tp->mtx);
  for (size_t i = 0; i < tp->nth; ++i) pthread_join(tp->th[i], NULL);
  struct job* j = tp->head;
  while (j) { struct job* n = j->next; free(j); j = n; }
  pthread_mutex_destroy(&tp->mtx);
  pthread_cond_destroy(&tp->cv);
  free(tp->th);
  free(tp);
}

int tp_submit(struct threadpool* tp, void (*fn)(void*), void* arg) {
  struct job* j = (struct job*)malloc(sizeof(*j));
  if (!j) return -1; /* OOM */
  j->fn = fn; j->arg = arg; j->next = NULL;
  pthread_mutex_lock(&tp->mtx);
  if (tp->tail) tp->tail->next = j; else tp->head = j;
  tp->tail = j;
  pthread_cond_signal(&tp->cv);
  pthread_mutex_unlock(&tp->mtx);
  return 0;
}


