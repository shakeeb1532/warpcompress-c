#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct job { tp_fn fn; void *arg; struct job *next; } job_t;

struct tp {
  pthread_mutex_t mu;
  pthread_cond_t  cv;
  pthread_cond_t  idle;
  int threads, active, stop;
  job_t *head, *tail;
};

static void* worker(void *p) {
  tp_t *tp = (tp_t*)p;
  for (;;) {
    pthread_mutex_lock(&tp->mu);
    while (!tp->stop && tp->head == NULL) pthread_cond_wait(&tp->cv, &tp->mu);
    if (tp->stop && tp->head == NULL) { pthread_mutex_unlock(&tp->mu); break; }
    job_t *j = tp->head; tp->head = j->next; if (!tp->head) tp->tail = NULL; tp->active++;
    pthread_mutex_unlock(&tp->mu);
    j->fn(j->arg); free(j);
    pthread_mutex_lock(&tp->mu);
    tp->active--; if (tp->active == 0 && tp->head == NULL) pthread_cond_broadcast(&tp->idle);
    pthread_mutex_unlock(&tp->mu);
  }
  return NULL;
}

tp_t* tp_create(int threads) {
  if (threads <= 0) threads = 1;
  tp_t *tp = (tp_t*)calloc(1, sizeof(*tp));
  pthread_mutex_init(&tp->mu, NULL);
  pthread_cond_init(&tp->cv, NULL);
  pthread_cond_init(&tp->idle, NULL);
  tp->threads = threads;
  pthread_t t;
  for (int i=0;i<threads;i++) pthread_create(&t, NULL, worker, tp), pthread_detach(t);
  return tp;
}
void tp_submit(tp_t *tp, tp_fn fn, void *arg) {
  job_t *j = (job_t*)malloc(sizeof(*j)); j->fn = fn; j->arg = arg; j->next = NULL;
  pthread_mutex_lock(&tp->mu);
  if (tp->tail) tp->tail->next = j; else tp->head = j; tp->tail = j;
  pthread_cond_signal(&tp->cv);
  pthread_mutex_unlock(&tp->mu);
}
void tp_barrier(tp_t *tp) {
  pthread_mutex_lock(&tp->mu);
  while (tp->active || tp->head) pthread_cond_wait(&tp->idle, &tp->mu);
  pthread_mutex_unlock(&tp->mu);
}
void tp_destroy(tp_t *tp) {
  pthread_mutex_lock(&tp->mu); tp->stop = 1; pthread_cond_broadcast(&tp->cv); pthread_mutex_unlock(&tp->mu);
  pthread_mutex_destroy(&tp->mu); pthread_cond_destroy(&tp->cv); pthread_cond_destroy(&tp->idle); free(tp);
}

