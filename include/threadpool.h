#pragma once
#include <stddef.h>

typedef void (*tp_func_t)(void*);

typedef struct tp tp_t;

tp_t* tp_create(int threads);
void  tp_submit(tp_t* tp, tp_func_t fn, void* arg);
void  tp_barrier(tp_t* tp);
void  tp_destroy(tp_t* tp);

