#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>   /* ssize_t, off_t */
#include <stdio.h>       /* FILE */

/* POSIX pread/pwrite helpers (offset-based I/O) */
ssize_t wc_pread (int fd, void *buf, size_t len, uint64_t off);
ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off);

/* Truncate a FILE* to a specific size (returns 0 on success, -1 on error) */
int wc_truncate_file(FILE *f, uint64_t size);
