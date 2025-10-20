/* Reserved for future helpers (prefetch, posix_fadvise, large file hints). */
#include "util.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
  #include <io.h>
  #include <stdio.h>
  #define wc_lseek _lseeki64
  #define wc_read  _read
  #define wc_write _write
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <sys/uio.h>
#endif

/* ---------- pread/pwrite wrappers ---------- */
ssize_t wc_pread(int fd, void *buf, size_t len, uint64_t off) {
#if defined(_WIN32)
  /* Fallback: seek+read (not thread-safe on shared fd without a lock).
     Our usage: we only use this path on Windows builds that aren't common here.
     On macOS/Linux we use real pread. */
  if (wc_lseek(fd, (long long)off, SEEK_SET) < 0) return -1;
  return wc_read(fd, buf, (unsigned)len);
#else
  return pread(fd, buf, len, (off_t)off);
#endif
}

ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off) {
#if defined(_WIN32)
  if (wc_lseek(fd, (long long)off, SEEK_SET) < 0) return -1;
  return wc_write(fd, buf, (unsigned)len);
#else
  return pwrite(fd, buf, len, (off_t)off);
#endif
}

/* ---------- ftruncate on FILE* ---------- */
int wc_ftruncate_file(void *FILE_ptr, uint64_t size) {
  FILE *f = (FILE*)FILE_ptr;
#if defined(_WIN32)
  return _chsize_s(_fileno(f), size);
#else
  return ftruncate(fileno(f), (off_t)size);
#endif
}

/* ---------- advise sequential ---------- */
void wc_advise_sequential(int fd) {
#if defined(__APPLE__)
  (void)fd; /* fcntl(F_RDAHEAD,1) could be used; leave as no-op to keep code tiny */
#elif defined(__linux__)
  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#else
  (void)fd;
#endif
}

/* ---------- buffer pool ---------- */
#include <pthread.h>

struct bufpool {
  size_t   blk;
  int      cap;
  int     *free;
  void   **slots;
  pthread_mutex_t mu;
};

bufpool_t* pool_create(size_t block_size, int count) {
  if (count < 1) count = 1;
  bufpool_t *p = (bufpool_t*)calloc(1, sizeof(*p));
  if (!p) return NULL;
  p->blk = block_size; p->cap = count;
  p->free  = (int*)malloc(sizeof(int)*count);
  p->slots = (void**)malloc(sizeof(void*)*count);
  if (!p->free || !p->slots) { free(p->free); free(p->slots); free(p); return NULL; }
  pthread_mutex_init(&p->mu, NULL);
  for (int i=0;i<count;i++) {
    p->slots[i] = malloc(block_size);
    if (!p->slots[i]) { p->cap = i; break; }
    p->free[i]  = 1;
  }
  return p;
}

void* pool_acquire(bufpool_t *p) {
  pthread_mutex_lock(&p->mu);
  for (int i=0;i<p->cap;i++) {
    if (p->free[i]) { p->free[i]=0; void *b=p->slots[i]; pthread_mutex_unlock(&p->mu); return b; }
  }
  pthread_mutex_unlock(&p->mu);
  return NULL; /* exhausted */
}

void pool_release(bufpool_t *p, void *block) {
  pthread_mutex_lock(&p->mu);
  for (int i=0;i<p->cap;i++) {
    if (p->slots[i] == block) { p->free[i]=1; break; }
  }
  pthread_mutex_unlock(&p->mu);
}

void pool_destroy(bufpool_t *p) {
  if (!p) return;
  for (int i=0;i<p->cap;i++) free(p->slots[i]);
  free(p->slots); free(p->free);
  pthread_mutex_destroy(&p->mu);
  free(p);
}

/* ---------- writev wrapper ---------- */
ssize_t wc_writev(int fd, const wc_iovec_t *iov, int iovcnt) {
#if defined(_WIN32)
  ssize_t total = 0;
  for (int i=0;i<iovcnt;i++) {
    const char *p = (const char*)iov[i].iov_base;
    size_t n = iov[i].iov_len;
    ssize_t w = wc_write(fd, p, (unsigned)n);
    if (w < 0) return -1;
    total += w;
  }
  return total;
#else
  return writev(fd, (const struct iovec*)iov, iovcnt);
#endif
}
