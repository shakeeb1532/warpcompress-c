#include "bufpool.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct node {
  void* buf;
  struct node* next;
} node_t;

struct bufpool {
  size_t buf_size;
  node_t* head;
  int count;
  pthread_mutex_t mu;
  pthread_cond_t  cv;
};

bufpool_t* pool_create(size_t buf_size, int count) {
  if (count < 1) count = 1;
  bufpool_t* p = (bufpool_t*)calloc(1, sizeof(*p));
  if (!p) return NULL;
  p->buf_size = buf_size;
  pthread_mutex_init(&p->mu, NULL);
  pthread_cond_init(&p->cv, NULL);
  for (int i=0;i<count;i++) {
    void* b = malloc(buf_size);
    if (!b) continue;
    node_t* n = (node_t*)malloc(sizeof(*n));
    if (!n) { free(b); continue; }
    n->buf=b; n->next=p->head; p->head=n; p->count++;
  }
  return p;
}

void* pool_acquire(bufpool_t* p) {
  pthread_mutex_lock(&p->mu);
  while (!p->head) pthread_cond_wait(&p->cv, &p->mu);
  node_t* n = p->head; p->head = n->next; p->count--;
  pthread_mutex_unlock(&p->mu);
  void* b = n->buf; free(n); return b;
}

void pool_release(bufpool_t* p, void* buf) {
  node_t* n = (node_t*)malloc(sizeof(*n));
  n->buf = buf;
  pthread_mutex_lock(&p->mu);
  n->next = p->head; p->head = n; p->count++;
  pthread_cond_signal(&p->cv);
  pthread_mutex_unlock(&p->mu);
}

void pool_destroy(bufpool_t* p) {
  if (!p) return;
  node_t* n = p->head;
  while (n) { node_t* nx = n->next; free(n->buf); free(n); n = nx; }
  pthread_mutex_destroy(&p->mu);
  pthread_cond_destroy(&p->cv);
  free(p);
}
