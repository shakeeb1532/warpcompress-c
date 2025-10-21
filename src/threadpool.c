#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

typedef struct job {
  tp_func_t fn;
  void* arg;
  struct job* next;
} job_t;

struct tp {
  int threads;
  pthread_t* th;
  pthread_mutex_t mu;
  pthread_cond_t  cv;
  job_t* head;
  job_t* tail;
  atomic_int pending;
  int stop;
};

static void* worker(void* arg) {
  tp_t* tp = (tp_t*)arg;
  for (;;) {
    pthread_mutex_lock(&tp->mu);
    while (!tp->head && !tp->stop) pthread_cond_wait(&tp->cv, &tp->mu);
    if (tp->stop && !tp->head) { pthread_mutex_unlock(&tp->mu); break; }
    job_t* j = tp->head;
    tp->head = j->next;
    if (!tp->head) tp->tail = NULL;
    pthread_mutex_unlock(&tp->mu);

    j->fn(j->arg);
    free(j);
    atomic_fetch_sub_explicit(&tp->pending, 1, memory_order_relaxed);
  }
  return NULL;
}

tp_t* tp_create(int threads) {
  if (threads < 1) threads = 1;
  tp_t* tp = (tp_t*)calloc(1, sizeof(*tp));
  if (!tp) return NULL;
  tp->threads = threads;
  tp->th = (pthread_t*)malloc(sizeof(pthread_t)*threads);
  pthread_mutex_init(&tp->mu, NULL);
  pthread_cond_init(&tp->cv, NULL);
  atomic_store(&tp->pending, 0);
  tp->stop = 0;
  for (int i=0;i<threads;i++) pthread_create(&tp->th[i], NULL, worker, tp);
  return tp;
}

void tp_submit(tp_t* tp, tp_func_t fn, void* arg) {
  job_t* j = (job_t*)malloc(sizeof(*j));
  j->fn = fn; j->arg = arg; j->next = NULL;
  atomic_fetch_add_explicit(&tp->pending, 1, memory_order_relaxed);
  pthread_mutex_lock(&tp->mu);
  if (tp->tail) tp->tail->next = j; else tp->head = j;
  tp->tail = j;
  pthread_cond_signal(&tp->cv);
  pthread_mutex_unlock(&tp->mu);
}

void tp_barrier(tp_t* tp) {
  for (;;) {
    if (atomic_load_explicit(&tp->pending, memory_order_relaxed) == 0) break;
    struct timespec ts = {0, 1000000}; /* 1ms */
    nanosleep(&ts, NULL);
  }
}

void tp_destroy(tp_t* tp) {
  pthread_mutex_lock(&tp->mu);
  tp->stop = 1;
  pthread_cond_broadcast(&tp->cv);
  pthread_mutex_unlock(&tp->mu);
  for (int i=0;i<tp->threads;i++) pthread_join(tp->th[i], NULL);
  pthread_mutex_destroy(&tp->mu);
  pthread_cond_destroy(&tp->cv);
  free(tp->th);
  /* free leftover jobs if any */
  job_t* j = tp->head; while (j) { job_t* nx=j->next; free(j); j=nx; }
  free(tp);
}


