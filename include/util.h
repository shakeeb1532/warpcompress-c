#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* A minimal iovec we control; we’ll cast to system struct iovec in util.c */
typedef struct {
  void  *iov_base;
  size_t iov_len;
} wc_iovec_t;

/* p{read,write} helpers that take 64-bit offsets */
ssize_t wc_pread (int fd, void *buf, size_t len, uint64_t off);
ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off);

/* best-effort “sequential” hint (no-op where unsupported) */
void    wc_advise_sequential(int fd);

/* file truncate via FILE* (used to pre-size outputs) */
int     wc_ftruncate_file(FILE *f, uint64_t size);

/* writev wrapper (returns bytes written or -1) */
ssize_t wc_writev(int fd, const wc_iovec_t *iov, int iovcnt);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UTIL_H */


