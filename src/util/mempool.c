#define _RS_MEMPOOL_C_
#include "mempool.h"
#include <sys/param.h>
#include <stdio.h>
#include <pthread.h>

typedef struct mempool_t {
  void **entries;
  size_t top;
  size_t cap;
  size_t max;  // max size for pool
  mempool_alloc_fn alloc;
  mempool_free_fn free;
  pthread_mutex_t lock;
} mempool_t;

mempool_t *mempool_new(size_t cap, mempool_alloc_fn alloc, mempool_free_fn freefn) {
  return mempool_new_limited(cap, 0, alloc, freefn);
}

static int mempoolDisable_g = -1;

mempool_t *mempool_new_limited(size_t cap, size_t max, mempool_alloc_fn alloc,
                               mempool_free_fn freefn) {
  mempool_t *p = malloc(sizeof(mempool_t));
  p->entries = calloc(cap, sizeof(void *));
  p->alloc = alloc;
  p->free = freefn;
  p->cap = cap;
  p->max = max;
  p->top = 0;
  if (mempoolDisable_g == -1) {
    if (getenv("REDISEARCH_NO_MEMPOOL")) {
      fprintf(stderr, "[redisearch]: REDISEARCH_NO_MEMPOOL in environment. Disabling\n");
      mempoolDisable_g = 1;
    } else {
      mempoolDisable_g = 0;
    }
  }
  if (mempoolDisable_g) {
    p->cap = 0;
    p->max = 0;
    free(p->entries);
    p->entries = NULL;
  }
  return p;
}

void *mempool_get(mempool_t *p) {
  void *ret = NULL;
  if (p->top > 0) {
    ret = p->entries[--p->top];

  } else {
    ret = p->alloc();
  }
  return ret;
}

inline void mempool_release(mempool_t *p, void *ptr) {
  if (p->top == p->cap) {
    // This is a limited pool, and we can't outgrow ourselves now, just free the ptr immediately
    if (p->entries == NULL || (p->max && p->max == p->top)) {
      p->free(ptr);
      return;
    }
    // grow the pool
    p->cap += p->cap ? MIN(p->cap, 1024) : 1;
    p->entries = realloc(p->entries, p->cap * sizeof(void *));
  }
  p->entries[p->top++] = ptr;
}

void mempool_destroy(mempool_t *p) {
  for (size_t i = 0; i < p->top; i++) {
    p->free(p->entries[i]);
  }
  free(p->entries);
  free(p);
}
