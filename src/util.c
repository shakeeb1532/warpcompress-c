/* Reserved for future helpers (prefetch, posix_fadvise, large file hints). */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------- ssize_t on Windows -------- */
#if defined(_WIN32) && !defined(__MINGW32__)
  #include <BaseTsd.h>
  typedef SSIZE_T ssize_t;
#endif

/* -------- Cross-platform pread/pwrite wrappers -------- */
ssize_t wc_pread (int fd, void *buf, size_t len, uint64_t off);
ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off);

/* Truncate a FILE* to size (POSIX/Windows) */
int wc_ftruncate_file(void *FILE_ptr, uint64_t size);

/* Advise sequential access (best-effort; no-op if unsupported) */
void wc_advise_sequential(int fd);

/* -------- Tiny fixed-size buffer pool -------- */
typedef struct bufpool bufpool_t;

/* Create a pool of `count` blocks, each `block_size` bytes. Returns NULL on OOM. */
bufpool_t* pool_create(size_t block_size, int count);
/* Get a block (wait-free). Returns NULL if exhausted (should not happen if count>=threads*2). */
void*      pool_acquire(bufpool_t *p);
/* Return a block to the pool. */
void       pool_release(bufpool_t *p, void *block);
void       pool_destroy(bufpool_t *p);

/* -------- writev batching (POSIX) with safe fallback -------- */
#if !defined(_WIN32)
  #include <sys/uio.h>
  typedef struct iovec wc_iovec_t;
#else
  typedef struct { void *iov_base; size_t iov_len; } wc_iovec_t;
#endif

/* Write an array of iovecs at current file position. Returns total bytes or -1 on error. */
ssize_t wc_writev(int fd, const wc_iovec_t *iov, int iovcnt);

#ifdef __cplusplus
}
#endif
#endif
